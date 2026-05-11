#include "libretro_overlay_panel.h"
#include "core/macos_fullscreen.h"

#include <QGuiApplication>
#include <QMetaObject>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QScreen>
#include <QDebug>

LibretroOverlayPanel::LibretroOverlayPanel(QQmlEngine* engine, QObject* parent)
    : QObject(parent), m_engine(engine) {
    Q_ASSERT(engine);
}

LibretroOverlayPanel::~LibretroOverlayPanel() = default;

void LibretroOverlayPanel::ensureCreated() {
    if (m_window) return;

    if (!m_component) {
        m_component = new QQmlComponent(m_engine,
            QUrl(QStringLiteral("qrc:/AppUI/qml/AppUI/LibretroOverlayPanel.qml")), this);
    }
    if (m_component->isError()) {
        qWarning() << "[LibretroOverlayPanel] component errors:" << m_component->errors();
        return;
    }

    QObject* obj = m_component->create();
    m_window = qobject_cast<QQuickWindow*>(obj);
    if (!m_window) {
        qWarning() << "[LibretroOverlayPanel] root is not a QQuickWindow";
        if (obj) obj->deleteLater();
        return;
    }
    // QObject parenting: window dies with this panel instance.
    static_cast<QObject*>(m_window.data())->setParent(this);

    wireSignals();
}

void LibretroOverlayPanel::wireSignals() {
    if (!m_window) return;
    connect(m_window, SIGNAL(resumeRequested()),
            this, SIGNAL(resumeRequested()));
    connect(m_window, SIGNAL(exitWithSaveRequested()),
            this, SIGNAL(exitWithSaveRequested()));
    connect(m_window, SIGNAL(exitWithoutSaveRequested()),
            this, SIGNAL(exitWithoutSaveRequested()));
    connect(m_window, SIGNAL(saveStateRequested()),
            this, SIGNAL(saveStateRequested()));
    connect(m_window, SIGNAL(loadStateRequested()),
            this, SIGNAL(loadStateRequested()));
    connect(m_window, SIGNAL(toggleFastForwardRequested()),
            this, SIGNAL(toggleFastForwardRequested()));
}

void LibretroOverlayPanel::showForCurrentGame() {
    ensureCreated();
    if (!m_window) return;

    // Cover the screen the primary RetroNest window lives on. RetroNest
    // runs borderless fullscreen, so this is the whole screen geometry.
    QScreen* primary = QGuiApplication::primaryScreen();
    if (primary) m_window->setGeometry(primary->geometry());

    m_window->show();

    // Apply NSPanel chrome + start in mouse-passthrough mode so toasts /
    // badges render above Metal but mouse clicks still fall through to
    // the game NSView below.
    void* nsView = reinterpret_cast<void*>(m_window->winId());
    MacFullscreen::configurePanelWindow(nsView);
    MacFullscreen::setIgnoresMouseEvents(nsView, true);

    // Attach as a child of RetroNest's main top-level window so we track
    // its screen + geometry + Spaces membership.
    auto windows = QGuiApplication::topLevelWindows();
    for (QWindow* w : windows) {
        if (w == m_window) continue;
        if (w->flags() & Qt::Tool) continue;  // skip other tool windows
        if (!w->isVisible()) continue;
        void* parentNSView = reinterpret_cast<void*>(w->winId());
        MacFullscreen::attachChildWindow(parentNSView, nsView);
        break;
    }
}

void LibretroOverlayPanel::hide() {
    if (!m_window) return;
    m_window->hide();
    // Destroying the QQuickWindow forces a fresh scene graph on the next
    // game start; matches the agreed-upon "created at game start,
    // destroyed at game end" lifecycle and avoids stale state from a
    // prior session leaking forward.
    m_window->deleteLater();
    m_window.clear();
    m_menuOpen = false;
}

void LibretroOverlayPanel::openMenu() {
    ensureCreated();
    if (!m_window) return;

    void* nsView = reinterpret_cast<void*>(m_window->winId());
    MacFullscreen::setIgnoresMouseEvents(nsView, false);
    MacFullscreen::makePanelKey(nsView);

    QMetaObject::invokeMethod(m_window, "openMenu");
    m_menuOpen = true;
    emit menuVisibleChanged();
}

void LibretroOverlayPanel::closeMenu() {
    if (!m_window) return;

    QMetaObject::invokeMethod(m_window, "closeMenu");

    void* nsView = reinterpret_cast<void*>(m_window->winId());
    MacFullscreen::setIgnoresMouseEvents(nsView, true);

    m_menuOpen = false;
    emit menuVisibleChanged();
}

bool LibretroOverlayPanel::isMenuOpen() const {
    return m_menuOpen;
}
