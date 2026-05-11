// SPDX-License-Identifier: GPL-3.0+
// pcsx2-libretro / RetroNest — Metal-backed video item.
//
// QQuickItem that exposes a layer-backed NSView for hardware-rendering
// libretro cores (PCSX2 on macOS uses Metal via GSDeviceMTL; the core
// installs its own CAMetalLayer on the NSView we provide).
//
// LibretroMetalItem is the hardware-render counterpart of
// LibretroVideoItem (software). EmulationView.qml hosts one or the
// other based on the active adapter's prefersHardwareRender().
//
// Implementation lives in libretro_metal_item.mm because it touches
// Objective-C++ (NSView, CAMetalLayer).

#pragma once

#include <QQuickItem>
#include <QtQml/qqmlregistration.h>

class QWindow;

class LibretroMetalItem : public QQuickItem {
    Q_OBJECT
    QML_ELEMENT
public:
    explicit LibretroMetalItem(QQuickItem* parent = nullptr);
    ~LibretroMetalItem() override;

    LibretroMetalItem(const LibretroMetalItem&) = delete;
    LibretroMetalItem& operator=(const LibretroMetalItem&) = delete;

    /**
     * Returns the underlying NSView pointer suitable for
     * RETRONEST_ENVIRONMENT_GET_MACOS_NSVIEW. Returned as qulonglong so
     * QML can read it without raw-pointer marshalling. Pass to
     * GameSession::registerHardwareView before retro_load_game.
     * Returns 0 if the underlying window hasn't been realized yet.
     */
    Q_INVOKABLE qulonglong nativeView() const;

protected:
    void itemChange(ItemChange change, const ItemChangeData& value) override;
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;

private:
    QWindow* m_window = nullptr;   // owns the underlying NSView via Qt's surface
};
