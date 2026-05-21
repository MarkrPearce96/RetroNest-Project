#pragma once
#include "libretro.h"
#include <QObject>
#include <memory>
#include <cstdint>

// VideoHardwareGL — RetroNest's libretro hardware-render path for cores
// that request RETRO_HW_CONTEXT_OPENGL_CORE (PPSSPP today; Dolphin /
// Citra / other GL cores in future). Frontend owns a pair of
// NSOpenGLContexts (main + shared HW) and an FBO backed by an IOSurface;
// the core renders into the FBO via standard libretro callbacks
// (get_current_framebuffer / get_proc_address) and we composite the
// IOSurface texture into Qt's scene graph via LibretroGLItem (not yet
// written — phase 2 of task #7).
//
// Pattern mirrors RetroArch's cocoa_gl_ctx.m two-context setup. See
// reference_retroarch_ppsspp_pattern.md for the proven recipe and
// reference_ppsspp_hw_render_observed.md for the observed request
// parameters (OPENGL_CORE 3.1, depth=1, no stencil, bottom_left_origin).
//
// PImpl keeps NSOpenGLContext / CGL types out of this header; the .mm
// owns all the Objective-C++ bits.
class VideoHardwareGL : public QObject {
    Q_OBJECT
public:
    explicit VideoHardwareGL(QObject* parent = nullptr);
    ~VideoHardwareGL() override;

    // Initialise the context pair + pixel format. Must succeed before
    // any FBO allocation. Idempotent — second call is a no-op.
    bool init();

    // Tear down FBO, contexts, pixel format. Safe to call from any
    // thread that isn't currently executing a GL operation.
    void shutdown();

    // (Re)allocate the FBO + color texture + optional depth renderbuffer
    // to the given dimensions. Called when the core reports new av_info
    // geometry (initial setup) and on RETRO_ENVIRONMENT_SET_GEOMETRY
    // (resolution changes mid-game). The main context must be current
    // when this is called — handled by makeMainCurrent() internally.
    bool allocateFbo(int width, int height, bool depth);

    // Make the HW context current on the calling thread. The libretro
    // core calls this implicitly via the hw_render callbacks; we expose
    // it explicitly for the context_reset call site in CoreRuntime.
    void makeHwCurrent();

    // Make the main (frontend) context current on the calling thread.
    // Used before any frontend-side GL operation (composite, screenshot,
    // resize). Sharing is enabled so resources are visible across both.
    void makeMainCurrent();

    // libretro hw_render callbacks. Pointers to these go into the
    // retro_hw_render_callback struct we hand back to the core.
    static uintptr_t getCurrentFramebufferThunk();
    static retro_proc_address_t getProcAddressThunk(const char* sym);

    // Submit a finished frame — called from CoreRuntime::videoTrampoline
    // when data == RETRO_HW_FRAME_BUFFER_VALID. Triggers frameReady.
    void submitFrame(int width, int height);

    // Did init() complete successfully?
    bool isReady() const;

    // Current IOSurface backing the color attachment, exposed as void* to
    // keep this header Qt-only (IOSurfaceRef is a CoreFoundation type).
    // Cast to IOSurfaceRef in callers. Returned pointer is valid until
    // shutdown() or allocateFbo() reallocates (callers may CFRetain to
    // extend lifetime). Null when no FBO is allocated.
    void* currentIOSurface() const;
    int   fboWidth() const;
    int   fboHeight() const;

signals:
    // Emitted after the core finishes rendering a frame into our FBO.
    // The LibretroGLItem (phase 2) imports the underlying IOSurface as
    // a Metal texture and composites it into Qt's scene graph.
    void frameReady(int width, int height);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
