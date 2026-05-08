#include "in_game_menu_panel.h"
#include "core/macos_fullscreen.h"

#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QScreen>
#include <QGuiApplication>
#include <QMetaObject>
#include <QCursor>
#include <QDebug>

InGameMenuPanel::InGameMenuPanel(QQmlEngine* engine, QObject* parent)
    : QObject(parent), m_engine(engine) {}

InGameMenuPanel::~InGameMenuPanel() = default;

void InGameMenuPanel::ensureCreated() {
    if (m_window) return;

    if (!m_component) {
        // Module is registered with RESOURCE_PREFIX "/"; QML files in the
        // AppUI module live at qrc:/AppUI/<file>.qml.
        m_component = new QQmlComponent(m_engine,
            QUrl(QStringLiteral("qrc:/AppUI/InGameMenuPanel.qml")), this);
    }
    if (m_component->isError()) {
        qWarning() << "[InGameMenuPanel] component errors:" << m_component->errors();
        return;
    }

    QObject* obj = m_component->create();
    m_window = qobject_cast<QQuickWindow*>(obj);
    if (!m_window) {
        qWarning() << "[InGameMenuPanel] root is not a QQuickWindow";
        if (obj) obj->deleteLater();
        return;
    }
    // Use QObject::setParent (not QWindow::setParent which expects a QWindow*)
    // so the QQuickWindow is destroyed alongside this InGameMenuPanel.
    static_cast<QObject*>(m_window.data())->setParent(this);

    wireSignals();
}

void InGameMenuPanel::wireSignals() {
    if (!m_window) return;
    connect(m_window, SIGNAL(resumeRequested()),
            this, SIGNAL(resumeRequested()));
    connect(m_window, SIGNAL(achievementsRequested(int, QString)),
            this, SIGNAL(achievementsRequested(int, QString)));
    connect(m_window, SIGNAL(exitWithSaveRequested()),
            this, SIGNAL(exitWithSaveRequested()));
    connect(m_window, SIGNAL(exitWithoutSaveRequested()),
            this, SIGNAL(exitWithoutSaveRequested()));
    connect(m_window, &QWindow::visibleChanged,
            this, &InGameMenuPanel::visibilityChanged);
}

void InGameMenuPanel::applyPanelChrome() {
    if (!m_window || m_chromeApplied) return;
    // QWindow::winId() on macOS returns NSView*.
    void* nsView = reinterpret_cast<void*>(m_window->winId());
    MacFullscreen::configurePanelWindow(nsView);
    m_chromeApplied = true;
}

void InGameMenuPanel::showOverEmulator(int64_t emulatorPid) {
    ensureCreated();
    if (!m_window) return;

    // Resolve target screen. screenForProcess returns an NSScreen* but
    // Qt's QScreen doesn't expose NSScreen* directly. Best-effort: prefer
    // the QScreen that contains the cursor (the user just pressed a hotkey,
    // the cursor is on the active display). Fall back to primary screen.
    Q_UNUSED(emulatorPid);
    QScreen* targetQScreen = QGuiApplication::primaryScreen();
    {
        const QPoint cursorPos = QCursor::pos();
        for (QScreen* s : QGuiApplication::screens()) {
            if (s->geometry().contains(cursorPos)) {
                targetQScreen = s;
                break;
            }
        }
    }

    const QRect screenGeom = targetQScreen->geometry();
    const int panelW = 480;
    const int panelH = 140;
    const int bottomMargin = 32;
    const int x = screenGeom.x() + (screenGeom.width() - panelW) / 2;
    const int y = screenGeom.y() + screenGeom.height() - panelH - bottomMargin;
    m_window->setGeometry(x, y, panelW, panelH);

    // Show the window first so winId() is valid, then apply NSPanel chrome
    // (one-time). configurePanelWindow is idempotent because m_chromeApplied
    // gates re-application.
    m_window->show();
    applyPanelChrome();

    // Tell the QML side to bring up the HUD content + force focus.
    QMetaObject::invokeMethod(m_window, "openMenu");
}

void InGameMenuPanel::hide() {
    if (!m_window) return;
    QMetaObject::invokeMethod(m_window, "closeMenu");
    m_window->hide();
}

bool InGameMenuPanel::isVisible() const {
    return m_window && m_window->isVisible();
}
