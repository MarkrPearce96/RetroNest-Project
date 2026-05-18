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
// Wide enough to fit the maximum HUD pill: 7 icons × 92 px + 6 ×
// 12 px spacing + 32 px pill x-padding = 748 px, plus margin for the
// achievements popup chrome. Items: Resume / Save State / Load State /
// Fast Forward / Achievements / Save & Quit / Quit. Bump if more
// actions land or icon width changes.
constexpr int kPanelWidth = 820;
// Tall enough to fit the slide-up Achievements popup card (460 px,
// since it grew when we added the tab bar + larger size for parity
// with the libretro path) plus a 12 px margin above the HUD pill
// (~92 px) plus the pill's own 32 px bottom margin plus an optional
// hardcore-mode badge (34 px including margin) — total ~640 px.
// Previously 540, which clipped the popup top (tabs disappeared
// behind the panel boundary). The window is transparent so the
// unused vertical space costs nothing visually.
constexpr int kPanelHeight = 640;
constexpr int kPanelBottomMargin = 32;
} // namespace

InGameMenuPanel::InGameMenuPanel(QQmlEngine* engine, QObject* parent)
    : QObject(parent), m_engine(engine) {
    Q_ASSERT(engine);
}

InGameMenuPanel::~InGameMenuPanel() {
    // Bug C variant fix (the LibretroOverlayPanel side was patched at
    // commit b468676; this one was missed and surfaced during SP3.5 smoke
    // 2026-05-18 when DuckStation was launched -> InGameMenuPanel opened ->
    // Cmd+Q -> identical NSException to Bug C, same _windowLayerContext
    // keypath, same QNSPanel_RNKeyEnabled class, just from this dtor
    // instead of LibretroOverlayPanel's).
    //
    // applyPanelChrome() calls MacFullscreen::configurePanelWindow() ->
    // promoteToKeyEnabled(), which per-instance isa-swaps the NSWindow
    // class to QNSPanel_RNKeyEnabled. AppKit registers _windowLayerContext
    // forwarded KVO observers against the *original* class's per-class
    // observer table when the NSWindow is realized at winId()/show() time
    // (before our swizzle). At -[NSWindow dealloc] AppKit looks up the
    // per-class table by the window's *current* class (the swizzled
    // subclass), finds nothing, throws NSException ("not registered as an
    // observer") -> SIGABRT.
    //
    // Restoring the original class here -- before Qt's deleteChildren
    // chain destroys m_window -- lets dealloc walk the same per-class
    // table that registration used. Pairs with the
    // objc_setAssociatedObject stash inside promoteToKeyEnabled.
    //
    // No-op if applyPanelChrome was never called (m_chromeApplied gates
    // the swizzle; restoreOriginalClass is itself a no-op when the
    // associated-object stash is absent).
    if (m_window) {
        void* nsView = reinterpret_cast<void*>(m_window->winId());
        MacFullscreen::restoreOriginalClass(nsView);
    }
}

void InGameMenuPanel::ensureCreated() {
    if (m_window) return;

    if (!m_component) {
        // Module URI is "AppUI" and RESOURCE_PREFIX is "/". Each file's
        // path under the module dir (e.g. qml/AppUI/InGameMenuPanel.qml)
        // is preserved, so the full qrc URL is /AppUI/<file_path>.
        m_component = new QQmlComponent(m_engine,
            QUrl(QStringLiteral("qrc:/AppUI/qml/AppUI/InGameMenuPanel.qml")), this);
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
    // makeKeyWindow must be called on every show — the
    // nonactivatingPanel style mask permits it; orderFront alone does
    // not promote the panel to key. Without this the emulator's
    // PauseOnFocusLoss won't fire and Qt focusWindow won't be the
    // panel (so SDL controller events go to the wrong scene).
    void* nsView = reinterpret_cast<void*>(m_window->winId());
    MacFullscreen::makePanelKey(nsView);

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
