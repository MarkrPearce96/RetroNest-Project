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
#import <AppKit/NSOpenGL.h>
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
};

VideoHardwareGL::VideoHardwareGL(QObject* parent)
    : QObject(parent), m_impl(std::make_unique<Impl>()) {}

VideoHardwareGL::~VideoHardwareGL() {
    // shutdown() is idempotent and the only path that releases GL handles
    // + IOSurface refcount. Without this, default destruction of m_impl
    // would leak NSOpenGLContext / IOSurface / FBO / textures / RBO.
    shutdown();
}

bool VideoHardwareGL::init() {
    if (m_impl->initialised) return true;

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
    // - OpenGLProfile Version3_2Core: PPSSPP requests OPENGL_CORE 3.1.
    //   macOS only ships 3.2 Core (no 3.1 Core); 3.2 is a strict
    //   superset so PPSSPP's resolution succeeds.
    NSOpenGLPixelFormatAttribute attributes[] = {
        NSOpenGLPFAColorSize,            24,
        NSOpenGLPFADoubleBuffer,
        NSOpenGLPFAAllowOfflineRenderers,
        NSOpenGLPFADepthSize,            16,
        NSOpenGLPFAOpenGLProfile,        NSOpenGLProfileVersion3_2Core,
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

    m_impl->initialised = true;
    if (s_activeInstance && s_activeInstance != this) {
        qWarning("[VideoHardwareGL] init() called while another instance is "
                 "active — overwriting singleton pointer; hw_render thunks "
                 "for the old instance will now dispatch to the new one");
    }
    s_activeInstance = this;
    qInfo("[VideoHardwareGL] init OK — pixel format + 2 shared NSOpenGLContexts "
          "(GL 3.2 Core, depth=16)");
    return true;
}

void VideoHardwareGL::shutdown() {
    if (!m_impl->initialised) return;

    // Tear down FBO + attachments on the main context. We need a
    // context current to delete GL objects — otherwise the driver
    // leaks them. ARC won't help here; these are raw GL handles.
    if (m_impl->fboId || m_impl->colorTex || m_impl->depthRbo) {
        [m_impl->mainCtx makeCurrentContext];
        if (m_impl->fboId)    { glDeleteFramebuffers(1, &m_impl->fboId);   m_impl->fboId   = 0; }
        if (m_impl->colorTex) { glDeleteTextures(1, &m_impl->colorTex);    m_impl->colorTex = 0; }
        if (m_impl->depthRbo) { glDeleteRenderbuffers(1, &m_impl->depthRbo); m_impl->depthRbo = 0; }
        [NSOpenGLContext clearCurrentContext];
    }
    if (m_impl->ioSurface) {
        CFRelease(m_impl->ioSurface);
        m_impl->ioSurface = nullptr;
    }

    // Contexts + pixel format: ARC releases when the strong refs drop.
    m_impl->mainCtx     = nil;
    m_impl->hwCtx       = nil;
    m_impl->pixelFormat = nil;
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

    [m_impl->mainCtx makeCurrentContext];

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
    CGLContextObj cgl = [m_impl->mainCtx CGLContextObj];
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

    m_impl->fboWidth = width;
    m_impl->fboHeight = height;
    m_impl->fboHasDepth = depth;
    [NSOpenGLContext clearCurrentContext];
    qInfo("[VideoHardwareGL] allocateFbo OK: %dx%d depth=%d fbo=%u tex=%u rbo=%u",
          width, height, depth ? 1 : 0,
          m_impl->fboId, m_impl->colorTex, m_impl->depthRbo);
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
    if (!s_activeInstance || !s_activeInstance->m_impl->fboId) {
        qWarning("[VideoHardwareGL] getCurrentFramebufferThunk: no active FBO "
                 "(instance=%p fboId=%u) — core will render to GL default FB "
                 "and the frame won't reach our composite path",
                 (void*)s_activeInstance,
                 s_activeInstance ? s_activeInstance->m_impl->fboId : 0);
        return 0;
    }
    return static_cast<uintptr_t>(s_activeInstance->m_impl->fboId);
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
    // Pending Phase 1f
    emit frameReady(width, height);
}

bool VideoHardwareGL::isReady() const {
    return m_impl->initialised;
}
