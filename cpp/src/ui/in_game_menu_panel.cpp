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

namespace {
constexpr int kPanelWidth = 480;
constexpr int kPanelHeight = 140;
constexpr int kPanelBottomMargin = 32;
} // namespace

InGameMenuPanel::InGameMenuPanel(QQmlEngine* engine, QObject* parent)
    : QObject(parent), m_engine(engine) {
    Q_ASSERT(engine);
}

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

    // Resolve target screen.
    //   1. Prefer the screen displaying the emulator's window (Qt mirrors
    //      [NSScreen screens] order on macOS, so the index from
    //      MacFullscreen translates directly).
    //   2. Fall back to the screen containing the cursor — covers cases
    //      where the emulator's window can't be located (kiosk,
    //      headless-style adapters, or the index is somehow stale).
    //   3. Last resort: primary screen.
    QScreen* targetQScreen = nullptr;

    const int idx = MacFullscreen::screenIndexForProcess(emulatorPid);
    const QList<QScreen*> screens = QGuiApplication::screens();
    if (idx >= 0 && idx < screens.size()) {
        targetQScreen = screens.at(idx);
    }

    if (!targetQScreen) {
        const QPoint cursorPos = QCursor::pos();
        for (QScreen* s : screens) {
            if (s->geometry().contains(cursorPos)) {
                targetQScreen = s;
                break;
            }
        }
    }

    if (!targetQScreen) targetQScreen = QGuiApplication::primaryScreen();

    const QRect screenGeom = targetQScreen->geometry();
    const int x = screenGeom.x() + (screenGeom.width() - kPanelWidth) / 2;
    const int y = screenGeom.y() + screenGeom.height() - kPanelHeight - kPanelBottomMargin;
    m_window->setGeometry(x, y, kPanelWidth, kPanelHeight);

    m_window->show();
    applyPanelChrome();

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
