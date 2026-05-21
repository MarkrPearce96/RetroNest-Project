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

#import <AppKit/NSOpenGL.h>
#import <OpenGL/OpenGL.h>

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
    NSOpenGLPixelFormat* pixelFormat = nil;
    NSOpenGLContext*     hwCtx       = nil;   // core renders into this
    NSOpenGLContext*     mainCtx     = nil;   // frontend composites with this
    // GLuint           fboId;     — pending Phase 1c
    // GLuint           colorTex;  — pending Phase 1c
    // GLuint           depthRbo;  — pending Phase 1c
    // IOSurfaceRef     ioSurface; — pending Phase 1c
};

VideoHardwareGL::VideoHardwareGL(QObject* parent)
    : QObject(parent), m_impl(std::make_unique<Impl>()) {}

VideoHardwareGL::~VideoHardwareGL() = default;

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
    qInfo("[VideoHardwareGL] init OK — pixel format + 2 shared NSOpenGLContexts "
          "(GL 3.2 Core, depth=16)");
    return true;
}

void VideoHardwareGL::shutdown() {
    if (!m_impl->initialised) return;
    // FBO teardown lands in Phase 1c. For now just drop the contexts.
    // ARC handles release; explicit nil clarifies intent.
    m_impl->mainCtx     = nil;
    m_impl->hwCtx       = nil;
    m_impl->pixelFormat = nil;
    m_impl->initialised = false;
    qInfo("[VideoHardwareGL] shutdown — contexts released");
}

bool VideoHardwareGL::allocateFbo(int width, int height, bool depth) {
    Q_UNUSED(depth);
    qWarning("[VideoHardwareGL] allocateFbo(%d,%d) — not yet implemented (task #7 phase 1c)",
             width, height);
    m_impl->fboWidth = width;
    m_impl->fboHeight = height;
    return false;
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
    // Pending Phase 1c — returns the singleton's current FBO id once
    // CoreRuntime owns a VideoHardwareGL instance.
    qWarning("[VideoHardwareGL] getCurrentFramebufferThunk() — stub (task #7 phase 1c)");
    return 0;
}

retro_proc_address_t VideoHardwareGL::getProcAddressThunk(const char* sym) {
    // Pending Phase 1e — dlsym from OpenGL.framework.
    Q_UNUSED(sym);
    return nullptr;
}

void VideoHardwareGL::submitFrame(int width, int height) {
    // Pending Phase 1f
    emit frameReady(width, height);
}

bool VideoHardwareGL::isReady() const {
    return m_impl->initialised;
}
