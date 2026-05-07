#include "controller_mapping_page.h"

#include "controller_bindings_view.h"
#include "core/binding_def.h"
#include "core/sdl_input_manager.h"
#include "settings_dialog_theme.h"
#include "ui/app_controller.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QShortcut>
#include <QTimer>
#include <QVBoxLayout>

ControllerMappingPage::ControllerMappingPage(SdlInputManager* inputManager,
                                              AppController* appController,
                                              const QString& emuId,
                                              const QString& controllerTypeId,
                                              QWidget* parent)
    : QDialog(parent)
    , m_inputManager(inputManager)
    , m_appController(appController)
    , m_emuId(emuId)
    , m_controllerTypeId(controllerTypeId)
{
    setWindowTitle("Controller");
    setMinimumSize(1280, 780);
    resize(1280, 780);
    setStyleSheet(QString("QDialog { background: %1; }")
                  .arg(SettingsDialogTheme::windowBg().name()));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ─── Top chrome: amber title strip ────────────────
    auto* head = new QFrame(this);
    head->setObjectName("ControllerMappingHead");
    head->setAttribute(Qt::WA_StyledBackground, true);
    head->setFixedHeight(56);
    head->setStyleSheet(QStringLiteral(
        "QFrame#ControllerMappingHead {"
        "  background: #4a4642;"
        "  border-bottom: 1px solid #3a3632;"
        "}"));
    auto* headLay = new QHBoxLayout(head);
    headLay->setContentsMargins(24, 0, 24, 0);

    auto* crumb = new QLabel(emuId.toUpper(), head);
    crumb->setStyleSheet(QStringLiteral(
        "color: #9a9690; font-size: 10px; letter-spacing: 3px;"
        "background: transparent;"));
    headLay->addWidget(crumb);

    auto* title = new QLabel(head);
    title->setStyleSheet(QStringLiteral(
        "color: #f59e0b; font-size: 14px; font-weight: 600;"
        "letter-spacing: 2px; background: transparent;"));
    const auto types = m_appController->controllerTypes(emuId);
    QString displayName;
    if (!controllerTypeId.isEmpty()) {
        for (const auto& v : types) {
            const auto m = v.toMap();
            if (m.value("id").toString() == controllerTypeId) {
                displayName = m.value("displayName").toString();
                break;
            }
        }
    } else if (!types.isEmpty()) {
        displayName = types.first().toMap().value("displayName").toString();
    }
    if (!displayName.isEmpty()) title->setText(displayName.toUpper());
    headLay->addSpacing(14);
    headLay->addWidget(title);
    headLay->addStretch(1);

    root->addWidget(head);

    // ─── Body: ControllerBindingsView ─────────────────
    m_view = new ControllerBindingsView(inputManager, appController, emuId,
                                          controllerTypeId, /*port=*/1, this);
    root->addWidget(m_view, 1);

    // ─── Wiring ───────────────────────────────────────
    connect(m_view, &ControllerBindingsView::rebindRequested, this,
            &ControllerMappingPage::onRebindRequested);
    connect(m_view, &ControllerBindingsView::clearRequested, this,
            &ControllerMappingPage::onClearRequested);

    // Auto-Map (Y / M)
    m_autoMapShortcut = new QShortcut(QKeySequence(Qt::Key_M), this);
    connect(m_autoMapShortcut, &QShortcut::activated, this, &ControllerMappingPage::onAutoMapRequested);

    // Close (X / Square — Backspace from SDL face-button injection).
    // Cards intercept Backspace via their own keyPressEvent (treated as
    // Clear when a card is focused), so this shortcut only fires when
    // focus is OUTSIDE a card — exactly the chrome-navigation case where
    // X-to-close makes sense.
    m_closeShortcut = new QShortcut(QKeySequence(Qt::Key_Backspace), this);
    connect(m_closeShortcut, &QShortcut::activated, this, &QDialog::accept);

    // Capture-completion routing.
    connect(m_inputManager, &SdlInputManager::bindingCaptured, this,
        [this](int devIdx, const QString& element, bool isAxis, bool positive){
            if (m_capturingKey.isEmpty()) return;
            const QString formatted = m_appController->formatCapturedBinding(
                m_emuId, devIdx, element, isAxis, positive);
            m_appController->saveBindingForPort(m_emuId, /*port=*/1,
                                                  m_controllerTypeId,
                                                  m_capturingKey, formatted,
                                                  /*deviceIndex=*/devIdx);
            m_capturingKey.clear();
            m_view->reloadBindings();
            // Defer the capturing-flag clear so any lingering arrow-key
            // injections from the same SDL input (e.g. analog axis push
            // also fires Key_Right) are still seen as "in capture" and
            // dropped by BindingCard's keyPressEvent guard.
            QTimer::singleShot(150, this, [this]() {
                m_view->setCapturing(false);
                if (m_autoMapShortcut) m_autoMapShortcut->setEnabled(true);
                if (m_closeShortcut)   m_closeShortcut->setEnabled(true);
            });
        });
    connect(m_inputManager, &SdlInputManager::keyboardCaptured, this,
        [this](const QString& keyString){
            if (m_capturingKey.isEmpty()) return;
            m_appController->saveBindingForPort(m_emuId, /*port=*/1,
                                                  m_controllerTypeId,
                                                  m_capturingKey, keyString,
                                                  /*deviceIndex=*/-1);
            m_capturingKey.clear();
            m_view->reloadBindings();
            QTimer::singleShot(150, this, [this]() {
                m_view->setCapturing(false);
                if (m_autoMapShortcut) m_autoMapShortcut->setEnabled(true);
                if (m_closeShortcut)   m_closeShortcut->setEnabled(true);
            });
        });

    // Cancel-capture cleanup. SdlInputManager::cancelCapture (Escape
    // during capture) flips m_capturing without firing bindingCaptured
    // / keyboardCaptured. Defer the cleanup by 50 ms so successful
    // captures (which fire those signals AFTER capturingChanged) run
    // their handlers first, clear m_capturingKey, and the bail-early
    // check below sees nothing to do.
    connect(m_inputManager, &SdlInputManager::capturingChanged, this,
        [this]() {
            if (m_inputManager->isCapturing()) return;   // entry, not exit
            QTimer::singleShot(50, this, [this]() {
                if (m_capturingKey.isEmpty()) return;     // success handler cleaned up
                // Real cancel — Escape was pressed, no signal followed.
                m_capturingKey.clear();
                m_view->setCapturing(false);
                if (m_autoMapShortcut) m_autoMapShortcut->setEnabled(true);
                if (m_closeShortcut)   m_closeShortcut->setEnabled(true);
            });
        });
}

void ControllerMappingPage::onRebindRequested(const BindingDef& b) {
    m_capturingKey = b.key;
    m_view->setCapturing(true);
    // Disable dialog-level shortcuts so the user can bind M / Backspace
    // (or other shortcut keys) without firing Auto-Map / Close instead.
    if (m_autoMapShortcut) m_autoMapShortcut->setEnabled(false);
    if (m_closeShortcut)   m_closeShortcut->setEnabled(false);
    m_inputManager->startCapture();
}

void ControllerMappingPage::onClearRequested(const BindingDef& b) {
    m_appController->clearBindingForPort(m_emuId, /*port=*/1,
                                          m_controllerTypeId, b.key);
    m_view->reloadBindings();
}

void ControllerMappingPage::onAutoMapRequested() {
    const int activeType = m_inputManager->controllerType();
    QString notification;

    if (activeType == 0) {
        // Keyboard — no keyboard schema defaults, so clear and let the
        // user rebind manually. Notification spells out what happened
        // since the visible result is just "Not bound" everywhere.
        m_appController->clearAllBindingsForPort(m_emuId, /*port=*/1, m_controllerTypeId);
        notification = "Bindings cleared — rebind each entry with a keyboard key.";
    } else {
        const QVariantList controllers = m_inputManager->connectedControllers();
        if (controllers.isEmpty()) {
            // controllerType reports a device but none is connected — odd
            // race. Clear and notify.
            m_appController->clearAllBindingsForPort(m_emuId, /*port=*/1, m_controllerTypeId);
            notification = "No controller connected — bindings cleared.";
        } else {
            // Pick the device of the matching type, falling back to the
            // first connected device.
            int devIdx = controllers.first().toMap()["deviceIndex"].toInt();
            QString deviceName = controllers.first().toMap()["name"].toString();
            for (const auto& c : controllers) {
                const auto map = c.toMap();
                if (m_inputManager->controllerTypeForDevice(map["deviceIndex"].toInt()) == activeType) {
                    devIdx = map["deviceIndex"].toInt();
                    deviceName = map["name"].toString();
                    break;
                }
            }
            m_appController->autoMapControllerForPort(m_emuId, /*port=*/1,
                                                        m_controllerTypeId, devIdx);
            notification = QString("Auto-mapped to %1.").arg(deviceName);
        }
    }
    m_view->reloadBindings();
    m_view->showStatus(notification);
}
