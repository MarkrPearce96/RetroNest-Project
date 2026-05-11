// SPDX-License-Identifier: GPL-3.0+

#include "libretro_metal_item.h"

#include <QGuiApplication>
#include <QQuickWindow>
#include <QWindow>
#include <QDebug>

#import <AppKit/AppKit.h>
#import <QuartzCore/CAMetalLayer.h>

LibretroMetalItem::LibretroMetalItem(QQuickItem* parent)
    : QQuickItem(parent)
{
    setFlag(QQuickItem::ItemHasContents, false);

    // Create a child QWindow that will own a layer-backed NSView.
    // setSurfaceType(MetalSurface) makes Qt request a NSView suitable
    // for CAMetalLayer attachment.
    m_window = new QWindow();
    m_window->setSurfaceType(QSurface::MetalSurface);
    m_window->setFlags(Qt::Widget);
    m_window->create(); // realizes the NSView immediately

    NSView* view = (__bridge NSView*)reinterpret_cast<void*>(m_window->winId());
    if (view) {
        [view setWantsLayer:YES];
        // We do NOT install a CAMetalLayer here. PCSX2's GSDeviceMTL
        // creates and installs its own CAMetalLayer on this NSView via
        // AttachSurfaceOnMainThread(). Leaving the layer empty lets that
        // installation succeed.
    } else {
        qWarning() << "[LibretroMetalItem] winId() returned null — NSView not realized";
    }
}

LibretroMetalItem::~LibretroMetalItem()
{
    // m_window is owned; Qt will tear it down. PCSX2's CAMetalLayer was
    // installed on the NSView; when the view is destroyed the layer
    // detaches with it. GameSession::registerHardwareView(0) is expected
    // to be called from QML's Component.onDestruction BEFORE this dtor
    // runs.
    delete m_window;
    m_window = nullptr;
}

qulonglong LibretroMetalItem::nativeView() const
{
    if (!m_window) return 0;
    return static_cast<qulonglong>(m_window->winId());
}

void LibretroMetalItem::itemChange(ItemChange change, const ItemChangeData& value)
{
    QQuickItem::itemChange(change, value);

    // When the item is added to a window, parent our child QWindow to it
    // so geometry tracking works. Without this, the underlying NSView is
    // a free-floating window not visually inside RetroNest.
    if (change == QQuickItem::ItemSceneChange && value.window) {
        if (m_window) {
            m_window->setParent(value.window);
            const QRect r = mapRectToScene(boundingRect()).toRect();
            m_window->setGeometry(r);
            m_window->show();
        }
    }
}

void LibretroMetalItem::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    if (m_window && window()) {
        const QRect r = mapRectToScene(newGeometry).toRect();
        m_window->setGeometry(r);
    }
}
