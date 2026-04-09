#include "bindings_widget_base.h"
#include "binding_widget_common.h"
#include "binding_display.h"
#include "core/sdl_input_manager.h"
#include "ui/app_controller.h"

#include <QResizeEvent>

BindingsWidgetBase::BindingsWidgetBase(SdlInputManager* inputManager,
                                       AppController* appController,
                                       const QString& emuId,
                                       int port,
                                       QWidget* parent)
    : QWidget(parent)
    , m_inputManager(inputManager)
    , m_appController(appController)
    , m_emuId(emuId)
    , m_port(port)
{
    setStyleSheet(QString("background: %1;").arg(kBg));

    connect(m_inputManager, &SdlInputManager::bindingCaptured,
            this, &BindingsWidgetBase::onBindingCaptured);
    connect(m_inputManager, &SdlInputManager::keyboardCaptured,
            this, &BindingsWidgetBase::onKeyboardCaptured);
}

void BindingsWidgetBase::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    relayout();
}

void BindingsWidgetBase::setupBtn(BindBtn* b, const QString& label) {
    b->setStyleSheet(kBtnStyle);
    b->setCursor(Qt::PointingHandCursor);
    b->setText("Not bound");
    b->setFixedHeight(kBtnH);
    m_bindingButtons[label] = b;
    connect(b, &QPushButton::clicked, this, [this, label]() { startCapture(label); });
    b->onRightClick = [this, label]() {
        auto* btn = m_bindingButtons[label];
        m_appController->clearBindingForPort(m_emuId, m_port,
            btn->property("iniKey").toString());
        loadBindings();
    };
}

void BindingsWidgetBase::loadBindings() {
    QVariantList bindings = m_appController->controllerBindingsForPort(m_emuId, m_port);
    for (const auto& b : bindings) {
        auto map = b.toMap();
        QString label = map["label"].toString();
        QString currentValue = map["currentValue"].toString();
        auto it = m_bindingButtons.find(label);
        if (it != m_bindingButtons.end()) {
            QPushButton* btn = it.value();
            auto detailedType = SdlInputManager::TypeStandard;
            int devIdx = deviceIndexFromBinding(currentValue);
            if (devIdx >= 0)
                detailedType = m_inputManager->detailedControllerTypeForDevice(devIdx);
            QString display = displayBinding(currentValue, detailedType);
            btn->setText(display.isEmpty() ? "Not bound" : display);
            btn->setStyleSheet(kBtnStyle);
            btn->setProperty("iniKey", map["key"].toString());
        }
    }
}

void BindingsWidgetBase::startCapture(const QString& label) {
    if (!m_capturingLabel.isEmpty()) loadBindings();
    m_capturingLabel = label;
    auto it = m_bindingButtons.find(label);
    if (it != m_bindingButtons.end()) {
        it.value()->setText("Press a button...");
        it.value()->setStyleSheet(kCapturingStyle);
    }
    m_inputManager->startCapture();
}

void BindingsWidgetBase::onBindingCaptured(int deviceIndex, const QString& element,
                                            bool isAxis, bool positive) {
    if (m_capturingLabel.isEmpty()) return;
    finishCapture(m_appController->formatCapturedBinding(m_emuId, deviceIndex, element, isAxis, positive));
}

void BindingsWidgetBase::onKeyboardCaptured(const QString& keyString) {
    if (m_capturingLabel.isEmpty()) return;
    finishCapture(keyString);
}

void BindingsWidgetBase::finishCapture(const QString& formatted) {
    auto it = m_bindingButtons.find(m_capturingLabel);
    if (it != m_bindingButtons.end()) {
        auto* btn = it.value();
        m_appController->saveBindingForPort(m_emuId, m_port,
                                            btn->property("iniKey").toString(), formatted);
    }
    m_capturingLabel.clear();
    loadBindings();
}
