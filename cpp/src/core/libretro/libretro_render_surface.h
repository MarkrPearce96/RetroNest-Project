#pragma once

/**
 * LibretroRenderSurface — the render-seam interface between GameSession
 * (core) and the QML GL video item (ui), so GameSession no longer
 * #includes ui/libretro/libretro_gl_item.h and compiles inside
 * retronest_core / under test (app-shell review P10, known since
 * packet 5).
 *
 * Only the GL hardware-render backend (PPSSPP) has an
 * IOSurface→MTLTexture coupling that races worker-side VideoHardwareGL
 * teardown against the Qt scene graph. GameSession drives the shutdown
 * fence through this interface; the concrete LibretroGLItem (a
 * QQuickItem) owns all the Qt Quick / scene-graph specifics.
 *
 * Implemented by multiple inheritance (LibretroGLItem : QQuickItem,
 * LibretroRenderSurface), so GameSession recovers the interface from the
 * registered QObject* via dynamic_cast.
 */
class LibretroRenderSurface {
public:
    virtual ~LibretroRenderSurface() = default;

    /**
     * Drop the GL texture reference and drain enough render passes that
     * the IOSurface→MTLTexture wrapper is safe to tear down, THEN return.
     * Blocks (bounded by an internal timeout) on the GUI thread. Safe to
     * call when there's nothing to drain.
     */
    virtual void fenceForShutdown() = 0;
};
