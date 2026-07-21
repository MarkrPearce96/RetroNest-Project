// NSOpenGL was deprecated on macOS 10.14 in favour of Metal/MetalKit. We
// knowingly use it here because the libretro `RETRO_HW_CONTEXT_OPENGL_CORE`
// contract speaks GL, the cores we host (PPSSPP, future Dolphin/Citra)
// render via GL, and an NSOpenGL-shared-context pair is the production-
// tested pattern (RetroArch's cocoa_gl_ctx.m). The long-term migration
// path is Vulkan via MoltenVK — see project_retronest_hw_render_decision.
// Silence the deprecation noise so this file's warnings don't drown out
// real diagnostics in the build log.
#define GL_SILENCE_DEPRECATION 1

#include "video_hardware_gl.h"
#include <QDebug>

#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#import <OpenGL/OpenGL.h>
#import <OpenGL/gl3.h>
#import <OpenGL/CGLIOSurface.h>
#import <IOSurface/IOSurfaceRef.h>
#include <dlfcn.h>

// Static singleton pointer the libretro hw_render callbacks dispatch
// through. We host one game at a time so a single active instance is
// the natural invariant. init() sets it; shutdown() clears it.
// Captured here (file scope, not class static) to keep the .h Qt-only.
static VideoHardwareGL* s_activeInstance = nullptr;

// PImpl — the real implementation lands incrementally in subsequent
// task #7 phases:
//   - Phase 1b ✅: init() / shutdown() — NSOpenGLPixelFormat + two
//     NSOpenGLContexts (main + shared HW)
//   - Phase 1c: allocateFbo() — IOSurface-backed color texture + depth
//     renderbuffer + FBO assembly
//   - Phase 1d: makeMainCurrent() / makeHwCurrent() — [ctx
//     makeCurrentContext] dispatch
//   - Phase 1e: getProcAddressThunk() — dlsym from OpenGL.framework
//   - Phase 1f: submitFrame() — emit frameReady with IOSurface handle
//
// Header stays Qt-only via PImpl; .mm holds the ObjC++ bits.
struct VideoHardwareGL::Impl {
    bool initialised = false;
    int fboWidth = 0;
    int fboHeight = 0;
    bool fboHasDepth = false;
    NSOpenGLPixelFormat* pixelFormat = nil;
    NSOpenGLContext*     hwCtx       = nil;   // core renders into this
    NSOpenGLContext*     mainCtx     = nil;   // frontend composites with this
    GLuint               fboId      = 0;
    GLuint               colorTex   = 0;   // GL_TEXTURE_RECTANGLE bound to ioSurface
    GLuint               depthRbo   = 0;   // 0 when fboHasDepth==false
    IOSurfaceRef         ioSurface  = nullptr;   // CoreFoundation refcount, not ARC
    // Intermediate present FBO: the core renders into THIS (a plain
    // GL_TEXTURE_2D-attachment FBO), and submitFrame glBlitFramebuffers it
    // into fboId (the IOSurface's GL_TEXTURE_RECTANGLE FBO). Rationale:
    // GLideN64's copy-shader present draw raises GL_INVALID_OPERATION when
    // the bound draw framebuffer's color attachment is a RECTANGLE texture
    // (macOS core profile); rendering into a 2D FBO and blitting sidesteps
    // it (glBlitFramebuffer to a rectangle attachment is legal). The
    // rectangle FBO thus becomes a blit destination only, never a draw
    // target. get_current_framebuffer returns presentFbo; the IOSurface
    // still receives every frame via the blit.
    GLuint               presentFbo   = 0;
    GLuint               presentTex   = 0;   // GL_TEXTURE_2D color attachment
    GLuint               presentDepth = 0;   // 0 when fboHasDepth==false
};

VideoHardwareGL::VideoHardwareGL(QObject* parent)
    : QObject(parent), m_impl(std::make_unique<Impl>()) {}

VideoHardwareGL::~VideoHardwareGL() {
    // shutdown() is idempotent and the only path that releases GL handles
    // + IOSurface refcount. Without this, default destruction of m_impl
    // would leak NSOpenGLContext / IOSurface / FBO / textures / RBO.
    shutdown();
}

bool VideoHardwareGL::init(unsigned reqMajor, unsigned reqMinor) {
    if (m_impl->initialised) return true;

    // macOS has exactly two Core profiles: 3.2 and 4.1. Pick 4.1 Core when the
    // core asks for anything past 3.2 (GLideN64 requests 3.3 and compiles GLSL
    // #version 330 shaders, impossible on a 3.2 Core context → black screen);
    // otherwise keep 3.2 Core (PPSSPP). 4.1 Core is a strict superset, so the
    // higher context never regresses a ≤3.2 core.
    const bool useGL41 = (reqMajor > 3) || (reqMajor == 3 && reqMinor > 2);
    const NSOpenGLPixelFormatAttribute glProfile =
        useGL41 ? NSOpenGLProfileVersion4_1Core : NSOpenGLProfileVersion3_2Core;

    // Pixel format mirrors RetroArch's cocoa_gl_ctx.m:399-409 — the
    // exact attribute set that ships in their production buildbot core.
    // - ColorSize 24: matches PSP framebuffer; PPSSPP's HW path tolerates
    //   any color size but 24 is what RetroArch uses.
    // - DoubleBuffer: even though we render into an FBO, the context
    //   itself is double-buffered (NSOpenGL requirement when bound to
    //   a view; harmless when not).
    // - AllowOfflineRenderers: lets the context use a non-display GPU
    //   (relevant on multi-GPU macs).
    // - DepthSize 16: PPSSPP observed request has depth=1; 16 bits is
    //   sufficient for PSP-accuracy depth tests.
    // - OpenGLProfile: 3.2 Core for cores that ask for ≤3.2 (PPSSPP requests
    //   OPENGL_CORE 3.1; macOS has no 3.1 Core, and 3.2 is a strict superset);
    //   4.1 Core for cores that ask past 3.2 (GLideN64 → GLSL 330). Selected
    //   above from the core's SET_HW_RENDER version.
    NSOpenGLPixelFormatAttribute attributes[] = {
        NSOpenGLPFAColorSize,            24,
        NSOpenGLPFADoubleBuffer,
        NSOpenGLPFAAllowOfflineRenderers,
        NSOpenGLPFADepthSize,            16,
        NSOpenGLPFAOpenGLProfile,        glProfile,
        0
    };
    m_impl->pixelFormat =
        [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
    if (!m_impl->pixelFormat) {
        qCritical("[VideoHardwareGL] NSOpenGLPixelFormat creation failed — "
                  "the host GPU does not support OpenGL 3.2 Core or any "
                  "required attribute");
        return false;
    }

    // Two-context pattern (cocoa_gl_ctx.m:446-457). g_hw_ctx is the
    // base of the share group; g_main_ctx shares with it. Any thread
    // that calls [either makeCurrentContext] sees the same FBO /
    // texture / shader namespace — that's what saves PPSSPP's internal
    // render thread (which we don't own) from running with a stale
    // context.
    m_impl->hwCtx =
        [[NSOpenGLContext alloc] initWithFormat:m_impl->pixelFormat
                                   shareContext:nil];
    if (!m_impl->hwCtx) {
        qCritical("[VideoHardwareGL] HW NSOpenGLContext creation failed");
        m_impl->pixelFormat = nil;
        return false;
    }

    m_impl->mainCtx =
        [[NSOpenGLContext alloc] initWithFormat:m_impl->pixelFormat
                                   shareContext:m_impl->hwCtx];
    if (!m_impl->mainCtx) {
        qCritical("[VideoHardwareGL] main NSOpenGLContext creation failed");
        m_impl->hwCtx = nil;
        m_impl->pixelFormat = nil;
        return false;
    }

    // hwCtx intentionally has no NSView attached. PPSSPP's libretro
    // OPENGL_CORE backend renders into the FBO returned by our
    // get_current_framebuffer thunk (our IOSurface-backed FBO), never
    // into the GL default framebuffer — so a backing drawable isn't
    // needed. (An earlier "Path A" attempt added an offscreen NSWindow
    // + NSView to give hwCtx a real FBO 0, but that turned out to be a
    // workaround for the wrong bug; the real issue was upstream PPSSPP
    // not calling SetGLCoreContext(true) for its OPENGL_CORE backend.
    // Fixed in our ppsspp-libretro fork.)
    m_impl->initialised = true;
    if (s_activeInstance && s_activeInstance != this) {
        qWarning("[VideoHardwareGL] init() called while another instance is "
                 "active — overwriting singleton pointer; hw_render thunks "
                 "for the old instance will now dispatch to the new one");
    }
    s_activeInstance = this;
    qInfo("[VideoHardwareGL] init OK — pixel format + 2 shared NSOpenGLContexts "
          "(GL %s Core, depth=16)", useGL41 ? "4.1" : "3.2");
    return true;
}

void VideoHardwareGL::shutdown() {
    if (!m_impl->initialised) return;

    // Tear down FBO + attachments on the HW context (same one that
    // allocated them — FBO IDs are per-context). Without makeCurrent
    // the driver leaks the handles.
    if (m_impl->fboId || m_impl->colorTex || m_impl->depthRbo ||
        m_impl->presentFbo || m_impl->presentTex || m_impl->presentDepth) {
        [m_impl->hwCtx makeCurrentContext];
        if (m_impl->fboId)    { glDeleteFramebuffers(1, &m_impl->fboId);   m_impl->fboId   = 0; }
        if (m_impl->colorTex) { glDeleteTextures(1, &m_impl->colorTex);    m_impl->colorTex = 0; }
        if (m_impl->depthRbo) { glDeleteRenderbuffers(1, &m_impl->depthRbo); m_impl->depthRbo = 0; }
        if (m_impl->presentFbo)   { glDeleteFramebuffers(1, &m_impl->presentFbo);    m_impl->presentFbo   = 0; }
        if (m_impl->presentTex)   { glDeleteTextures(1, &m_impl->presentTex);        m_impl->presentTex   = 0; }
        if (m_impl->presentDepth) { glDeleteRenderbuffers(1, &m_impl->presentDepth); m_impl->presentDepth = 0; }
        [NSOpenGLContext clearCurrentContext];
    }
    if (m_impl->ioSurface) {
        CFRelease(m_impl->ioSurface);
        m_impl->ioSurface = nullptr;
    }

    // Contexts + pixel format: ARC releases when the strong refs drop.
    m_impl->mainCtx       = nil;
    m_impl->hwCtx         = nil;
    m_impl->pixelFormat   = nil;
    m_impl->fboWidth = 0;
    m_impl->fboHeight = 0;
    m_impl->fboHasDepth = false;
    m_impl->initialised = false;
    if (s_activeInstance == this) s_activeInstance = nullptr;
    qInfo("[VideoHardwareGL] shutdown — FBO + contexts released");
}

bool VideoHardwareGL::allocateFbo(int width, int height, bool depth) {
    if (!m_impl->initialised) {
        qWarning("[VideoHardwareGL] allocateFbo() before init()");
        return false;
    }
    if (width <= 0 || height <= 0) {
        qWarning("[VideoHardwareGL] allocateFbo: invalid dimensions %dx%d", width, height);
        return false;
    }
    // Skip realloc if dimensions+depth match — PPSSPP triggers SET_GEOMETRY
    // on internal-resolution changes; reallocating the FBO on every poll
    // would thrash GPU memory.
    if (m_impl->fboId &&
        m_impl->fboWidth == width &&
        m_impl->fboHeight == height &&
        m_impl->fboHasDepth == depth) {
        return true;
    }

    // FBOs are NOT shared across NSOpenGL share groups — only the
    // attachments (textures, renderbuffers) are. PPSSPP's render thread
    // binds whatever id our get_current_framebuffer thunk returns; that
    // id must reference an FBO that exists in the HW context PPSSPP
    // operates on. So allocate the FBO on the HW context, not main.
    // The IOSurface-backed color texture + depth renderbuffer ARE shared,
    // so the composite side can still observe writes via the IOSurface
    // (which is independent of GL FBO identity).
    [m_impl->hwCtx makeCurrentContext];

    // 1) IOSurface — the cross-API backing store. BGRA8 matches what
    // Qt's Metal compositor expects when importing the texture later.
    // Tear down any pre-existing surface first.
    if (m_impl->ioSurface) {
        CFRelease(m_impl->ioSurface);
        m_impl->ioSurface = nullptr;
    }
    NSDictionary* surfaceProps = @{
        (__bridge id)kIOSurfaceWidth:           @(width),
        (__bridge id)kIOSurfaceHeight:          @(height),
        (__bridge id)kIOSurfaceBytesPerElement: @(4),
        (__bridge id)kIOSurfacePixelFormat:     @((uint32_t)'BGRA'),
    };
    m_impl->ioSurface = IOSurfaceCreate((__bridge CFDictionaryRef)surfaceProps);
    if (!m_impl->ioSurface) {
        qCritical("[VideoHardwareGL] IOSurfaceCreate failed for %dx%d", width, height);
        [NSOpenGLContext clearCurrentContext];
        return false;
    }

    // 2) Color texture bound to the IOSurface via CGLTexImageIOSurface2D.
    // Required target is GL_TEXTURE_RECTANGLE (CGL constraint). PPSSPP
    // only writes via the FBO — it doesn't sample the texture — so the
    // rectangle vs 2D distinction is internal to us; the composite side
    // imports the IOSurface as an MTLTexture (Metal's normalized UVs).
    if (m_impl->colorTex) {
        glDeleteTextures(1, &m_impl->colorTex);
        m_impl->colorTex = 0;
    }
    glGenTextures(1, &m_impl->colorTex);
    glBindTexture(GL_TEXTURE_RECTANGLE, m_impl->colorTex);
    // CGL requires the context handle to match the currently-current
    // context. We're on hwCtx (allocateFbo's makeCurrentContext above),
    // so query its CGL handle, not main's.
    CGLContextObj cgl = [m_impl->hwCtx CGLContextObj];
    CGLError cglErr = CGLTexImageIOSurface2D(
        cgl, GL_TEXTURE_RECTANGLE, GL_RGBA,
        width, height,
        GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
        m_impl->ioSurface, /*plane*/ 0);
    if (cglErr != kCGLNoError) {
        qCritical("[VideoHardwareGL] CGLTexImageIOSurface2D failed: %s",
                  CGLErrorString(cglErr));
        glDeleteTextures(1, &m_impl->colorTex);
        m_impl->colorTex = 0;
        CFRelease(m_impl->ioSurface);
        m_impl->ioSurface = nullptr;
        [NSOpenGLContext clearCurrentContext];
        return false;
    }
    glBindTexture(GL_TEXTURE_RECTANGLE, 0);

    // 3) Depth renderbuffer (16-bit — matches the pixel format DepthSize).
    // Only when the core asked for depth; PPSSPP does (depth=1 in SET_HW_RENDER).
    if (m_impl->depthRbo) {
        glDeleteRenderbuffers(1, &m_impl->depthRbo);
        m_impl->depthRbo = 0;
    }
    if (depth) {
        glGenRenderbuffers(1, &m_impl->depthRbo);
        glBindRenderbuffer(GL_RENDERBUFFER, m_impl->depthRbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, width, height);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
    }

    // 4) FBO assembly + completeness check.
    if (m_impl->fboId) {
        glDeleteFramebuffers(1, &m_impl->fboId);
        m_impl->fboId = 0;
    }
    glGenFramebuffers(1, &m_impl->fboId);
    glBindFramebuffer(GL_FRAMEBUFFER, m_impl->fboId);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_RECTANGLE, m_impl->colorTex, 0);
    if (depth) {
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                  GL_RENDERBUFFER, m_impl->depthRbo);
    }
    const GLenum fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (fboStatus != GL_FRAMEBUFFER_COMPLETE) {
        qCritical("[VideoHardwareGL] FBO incomplete after assembly: status=0x%X "
                  "(%dx%d, depth=%d)", fboStatus, width, height, depth ? 1 : 0);
        glDeleteFramebuffers(1, &m_impl->fboId);    m_impl->fboId    = 0;
        if (m_impl->depthRbo) { glDeleteRenderbuffers(1, &m_impl->depthRbo); m_impl->depthRbo = 0; }
        glDeleteTextures(1, &m_impl->colorTex);     m_impl->colorTex = 0;
        CFRelease(m_impl->ioSurface);               m_impl->ioSurface = nullptr;
        [NSOpenGLContext clearCurrentContext];
        return false;
    }

    // 5) Intermediate present FBO (plain GL_TEXTURE_2D color) that the core
    // actually renders into — see the Impl comment. Same size + depth as the
    // IOSurface FBO so it is a drop-in default framebuffer for the core.
    if (m_impl->presentTex)   { glDeleteTextures(1, &m_impl->presentTex);        m_impl->presentTex   = 0; }
    if (m_impl->presentDepth) { glDeleteRenderbuffers(1, &m_impl->presentDepth); m_impl->presentDepth = 0; }
    if (m_impl->presentFbo)   { glDeleteFramebuffers(1, &m_impl->presentFbo);    m_impl->presentFbo   = 0; }
    glGenTextures(1, &m_impl->presentTex);
    glBindTexture(GL_TEXTURE_2D, m_impl->presentTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0,
                 GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    if (depth) {
        glGenRenderbuffers(1, &m_impl->presentDepth);
        glBindRenderbuffer(GL_RENDERBUFFER, m_impl->presentDepth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, width, height);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
    }
    glGenFramebuffers(1, &m_impl->presentFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_impl->presentFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, m_impl->presentTex, 0);
    if (depth) {
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                  GL_RENDERBUFFER, m_impl->presentDepth);
    }
    const GLenum presentStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (presentStatus != GL_FRAMEBUFFER_COMPLETE) {
        qCritical("[VideoHardwareGL] present FBO incomplete: status=0x%X", presentStatus);
        glDeleteFramebuffers(1, &m_impl->presentFbo);    m_impl->presentFbo   = 0;
        if (m_impl->presentDepth) { glDeleteRenderbuffers(1, &m_impl->presentDepth); m_impl->presentDepth = 0; }
        glDeleteTextures(1, &m_impl->presentTex);        m_impl->presentTex   = 0;
        glDeleteFramebuffers(1, &m_impl->fboId);         m_impl->fboId    = 0;
        if (m_impl->depthRbo) { glDeleteRenderbuffers(1, &m_impl->depthRbo); m_impl->depthRbo = 0; }
        glDeleteTextures(1, &m_impl->colorTex);          m_impl->colorTex = 0;
        CFRelease(m_impl->ioSurface);                    m_impl->ioSurface = nullptr;
        [NSOpenGLContext clearCurrentContext];
        return false;
    }

    m_impl->fboWidth = width;
    m_impl->fboHeight = height;
    m_impl->fboHasDepth = depth;
    [NSOpenGLContext clearCurrentContext];
    qInfo("[VideoHardwareGL] allocateFbo OK: %dx%d depth=%d fbo=%u tex=%u rbo=%u "
          "presentFbo=%u presentTex=%u",
          width, height, depth ? 1 : 0,
          m_impl->fboId, m_impl->colorTex, m_impl->depthRbo,
          m_impl->presentFbo, m_impl->presentTex);
    return true;
}

void VideoHardwareGL::makeHwCurrent() {
    if (!m_impl->initialised) {
        qWarning("[VideoHardwareGL] makeHwCurrent() before init()");
        return;
    }
    [m_impl->hwCtx makeCurrentContext];
}

void VideoHardwareGL::makeMainCurrent() {
    if (!m_impl->initialised) {
        qWarning("[VideoHardwareGL] makeMainCurrent() before init()");
        return;
    }
    [m_impl->mainCtx makeCurrentContext];
}

uintptr_t VideoHardwareGL::getCurrentFramebufferThunk() {
    // Return the intermediate GL_TEXTURE_2D present FBO — the core renders
    // here, and submitFrame blits it into the IOSurface (rectangle) FBO.
    // See the Impl struct comment for why the core must not draw straight
    // into the rectangle attachment. Falls back to the IOSurface FBO if the
    // present FBO wasn't allocated (keeps a valid target no matter what).
    if (!s_activeInstance) return 0;
    if (s_activeInstance->m_impl->presentFbo) return s_activeInstance->m_impl->presentFbo;
    return s_activeInstance->m_impl->fboId;
}

retro_proc_address_t VideoHardwareGL::getProcAddressThunk(const char* sym) {
    if (!sym) return nullptr;
    // RTLD_DEFAULT searches all loaded images. We link -framework OpenGL
    // at link time, so OpenGL.framework's symbols are already in the
    // process address space and dlsym finds them without an explicit
    // dlopen. PPSSPP's glewInit resolves ~200 symbols sequentially via
    // this — keep it cheap.
    return reinterpret_cast<retro_proc_address_t>(dlsym(RTLD_DEFAULT, sym));
}

void VideoHardwareGL::submitFrame(int width, int height) {
    // The core rendered into the intermediate GL_TEXTURE_2D present FBO
    // (get_current_framebuffer returns presentFbo). Blit it into the
    // IOSurface-backed rectangle FBO so the Metal compositor sees the frame,
    // then flush and notify the QML side. Save/restore the FBO bindings
    // around the blit so glsm's (and the core's) cached binding state stays
    // consistent — the blit uses raw GL that glsm doesn't observe.
    if (m_impl->presentFbo && m_impl->fboId) {
        GLint prevDraw = 0, prevRead = 0;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevDraw);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prevRead);
        const int w = m_impl->fboWidth, h = m_impl->fboHeight;
        glBindFramebuffer(GL_READ_FRAMEBUFFER, m_impl->presentFbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_impl->fboId);
        // 1:1 copy — same orientation the core wrote, so the existing
        // MirrorVertically compositing stays correct. Every GL core presents
        // bottom-left-origin here (GLideN64's present pre-flips via
        // blitParams.invertY on its enableOverscan==0 path, which the fork
        // forces on Apple; PPSSPP renders GL-conventional natively).
        glBlitFramebuffer(0, 0, w, h, 0, 0, w, h,
                          GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)prevRead);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)prevDraw);
    }

    glFlush();
    emit frameReady(width, height);
}

bool VideoHardwareGL::isReady() const {
    return m_impl->initialised;
}

void* VideoHardwareGL::currentIOSurface() const {
    return static_cast<void*>(m_impl->ioSurface);
}

int VideoHardwareGL::fboWidth() const  { return m_impl->fboWidth;  }
int VideoHardwareGL::fboHeight() const { return m_impl->fboHeight; }
