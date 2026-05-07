#include "controller_mapping_page.h"

#include "controller_bindings_view.h"
#include "core/binding_def.h"
#include "core/sdl_input_manager.h"
#include "settings_dialog_theme.h"
#include "ui/app_controller.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QShortcut>
#include <QVBoxLayout>

ControllerMappingPage::ControllerMappingPage(SdlInputManager* inputManager,
                                              AppController* appController,
                                              const QString& emuId,
                                              QWidget* parent)
    : QDialog(parent)
    , m_inputManager(inputManager)
    , m_appController(appController)
    , m_emuId(emuId)
{
    setWindowTitle("Controller");
    setMinimumSize(1280, 720);
    resize(1280, 720);
    setStyleSheet(QString("QDialog { background: %1; }")
                  .arg(SettingsDialogTheme::windowBg().name()));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ─── Top chrome: amber title strip ────────────────
    auto* head = new QFrame(this);
    head->setFixedHeight(56);
    head->setStyleSheet(QStringLiteral(
        "background: #4a4642; border-bottom: 1px solid #3a3632;"));
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
    if (auto types = m_appController->controllerTypes(emuId); !types.isEmpty()) {
        title->setText(types.first().toMap().value("displayName").toString().toUpper());
    }
    headLay->addSpacing(14);
    headLay->addWidget(title);
    headLay->addStretch(1);

    root->addWidget(head);

    // ─── Body: ControllerBindingsView ─────────────────
    m_view = new ControllerBindingsView(inputManager, appController, emuId, /*port=*/1, this);
    root->addWidget(m_view, 1);

    // ─── Wiring ───────────────────────────────────────
    connect(m_view, &ControllerBindingsView::rebindRequested, this,
            &ControllerMappingPage::onRebindRequested);
    connect(m_view, &ControllerBindingsView::clearRequested, this,
            &ControllerMappingPage::onClearRequested);

    // Auto-Map (Y / M)
    auto* yShort = new QShortcut(QKeySequence(Qt::Key_M), this);
    connect(yShort, &QShortcut::activated, this, &ControllerMappingPage::onAutoMapRequested);

    // Capture-completion routing.
    connect(m_inputManager, &SdlInputManager::bindingCaptured, this,
        [this](int devIdx, const QString& element, bool isAxis, bool positive){
            if (m_capturingKey.isEmpty()) return;
            const QString formatted = m_appController->formatCapturedBinding(
                m_emuId, devIdx, element, isAxis, positive);
            m_appController->saveBindingForPort(m_emuId, /*port=*/1,
                                                  m_capturingKey, formatted);
            m_capturingKey.clear();
            m_view->reloadBindings();
        });
    connect(m_inputManager, &SdlInputManager::keyboardCaptured, this,
        [this](const QString& keyString){
            if (m_capturingKey.isEmpty()) return;
            m_appController->saveBindingForPort(m_emuId, /*port=*/1,
                                                  m_capturingKey, keyString);
            m_capturingKey.clear();
            m_view->reloadBindings();
        });
}

void ControllerMappingPage::onRebindRequested(const BindingDef& b) {
    m_capturingKey = b.key;
    m_inputManager->startCapture();
}

void ControllerMappingPage::onClearRequested(const BindingDef& b) {
    m_appController->clearBindingForPort(m_emuId, /*port=*/1, b.key);
    m_view->reloadBindings();
}

void ControllerMappingPage::onAutoMapRequested() {
    QMenu menu(this);
    menu.setStyleSheet(QStringLiteral(
        "QMenu { background: #4a4642; color: #f2efe8;"
        "        border: 1px solid #706c66; }"
        "QMenu::item { padding: 8px 24px; }"
        "QMenu::item:selected { background: #f59e0b; color: #1a1816; }"));

    menu.addAction("Keyboard", [this]() {
        m_appController->clearAllBindingsForPort(m_emuId, /*port=*/1);
        m_view->reloadBindings();
    });

    const QVariantList controllers = m_inputManager->connectedControllers();
    for (const auto& c : controllers) {
        const auto map = c.toMap();
        const int devIdx = map["deviceIndex"].toInt();
        const QString name = map["name"].toString();
        menu.addAction(QString("SDL-%1: %2").arg(devIdx).arg(name), [this, devIdx]() {
            m_appController->autoMapControllerForPort(m_emuId, /*port=*/1, devIdx);
            m_view->reloadBindings();
        });
    }
    menu.exec(QCursor::pos());
}
