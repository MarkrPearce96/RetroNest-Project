#include "generic_hotkey_page.h"

#include "core/sdl_input_manager.h"
#include "ui/app_controller.h"
#include "ui/settings/binding_display.h"
#include "ui/settings/settings_dialog_theme.h"
#include "ui/settings/widgets/hotkey_binding_row.h"
#include "ui/settings/widgets/settings_section_header.h"

#include <QEvent>
#include <QFrame>
#include <QKeyEvent>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>

namespace {

// Format multi-part binding for display.
// INI stores "SDL-0/Back & SDL-0/RightShoulder", display as "SDL-0 Create + SDL-0 R1".
QString displayMultiBinding(const QString& raw, SdlInputManager* inputMgr) {
    if (raw.isEmpty()) return {};
    const QStringList parts = raw.split(QStringLiteral(" & "), Qt::SkipEmptyParts);
    QStringList displayed;
    for (const auto& part : parts) {
        const QString trimmed = part.trimmed();
        auto detailedType = SdlInputManager::TypeStandard;
        const int devIdx = deviceIndexFromBinding(trimmed);
        if (devIdx >= 0 && inputMgr)
            detailedType = inputMgr->detailedControllerTypeForDevice(devIdx);
        const QString d = displayBinding(trimmed, detailedType);
        if (!d.isEmpty()) displayed.append(d);
    }
    return displayed.join(QStringLiteral(" + "));
}

bool isModifierKey(int key) {
    return key == Qt::Key_Shift || key == Qt::Key_Control
        || key == Qt::Key_Alt   || key == Qt::Key_Meta;
}

} // namespace

GenericHotkeyPage::GenericHotkeyPage(SdlInputManager* inputManager,
                                     AppController* appController,
                                     const QString& emuId,
                                     QWidget* parent)
    : QWidget(parent),
      m_inputManager(inputManager),
      m_appController(appController),
      m_emuId(emuId) {
    setStyleSheet(QStringLiteral("background:%1;")
                      .arg(SettingsDialogTheme::windowBg().name()));

    // Ingest hotkey data from the adapter (via AppController -> ConfigService).
    // Guarding nullptr keeps the test layer simple: tests can construct the
    // page with nullptr and assert the empty-state branch.
    const QVariantList bindings = m_appController
        ? m_appController->hotkeyBindings(m_emuId)
        : QVariantList{};
    for (const auto& v : bindings) {
        const auto map = v.toMap();
        HotkeyDef def;
        def.label = map.value(QStringLiteral("label")).toString();
        def.group = map.value(QStringLiteral("group")).toString();
        if (def.group.isEmpty()) def.group = QStringLiteral("General");
        def.section = map.value(QStringLiteral("section")).toString();
        def.key = map.value(QStringLiteral("key")).toString();
        def.defaultValue = map.value(QStringLiteral("defaultValue")).toString();
        m_entries.append(def);
        m_currentValues.insert(def.key,
                               map.value(QStringLiteral("currentValue")).toString());
    }

    m_captureTimer = new QTimer(this);
    m_captureTimer->setInterval(1000);
    connect(m_captureTimer, &QTimer::timeout, this, [this]{
        if (--m_captureCountdown <= 0) stopCapture(true);
    });

    if (m_inputManager) {
        connect(m_inputManager, &SdlInputManager::bindingCaptured,
                this, &GenericHotkeyPage::onBindingCaptured);
        connect(m_inputManager, &SdlInputManager::keyboardCaptured,
                this, &GenericHotkeyPage::onKeyboardCaptured);
        connect(m_inputManager, &SdlInputManager::captureButtonReleased,
                this, [this]{
                    if (!m_capturingKey.isEmpty() && !m_capturedBindings.isEmpty())
                        stopCapture(true);
                });
    }

    buildLayout();
    loadBindings();
}

void GenericHotkeyPage::buildLayout() {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    if (m_entries.isEmpty()) {
        auto* empty = new QLabel(
            QStringLiteral("This emulator does not expose hotkey bindings."), this);
        empty->setAlignment(Qt::AlignCenter);
        empty->setStyleSheet(QStringLiteral("color:%1; font-size:14px;")
            .arg(SettingsDialogTheme::textSecondary().name()));
        outer->addWidget(empty, 1);
        return;
    }

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet(QStringLiteral(
        "QScrollArea { background:transparent; border:0; }"
        "QScrollArea > QWidget > QWidget { background:transparent; }"));

    auto* content = new QWidget(scroll);
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(24, 20, 24, 20);
    contentLayout->setSpacing(8);

    QString currentGroup;
    QFrame* currentCard = nullptr;
    QVBoxLayout* currentCardLayout = nullptr;

    for (const auto& def : m_entries) {
        if (def.group != currentGroup) {
            currentGroup = def.group;
            contentLayout->addSpacing(4);
            contentLayout->addWidget(new SettingsSectionHeader(currentGroup, content));
            currentCard = new QFrame(content);
            currentCard->setObjectName(QStringLiteral("SettingsCard"));
            currentCard->setStyleSheet(SettingsDialogTheme::cardQss());
            currentCardLayout = new QVBoxLayout(currentCard);
            currentCardLayout->setContentsMargins(8, 8, 8, 8);
            currentCardLayout->setSpacing(2);
            contentLayout->addWidget(currentCard);
        }

        auto* row = new HotkeyBindingRow(def, currentCard);
        connect(row, &HotkeyBindingRow::focused,
                this, &GenericHotkeyPage::onRowFocused);
        connect(row, &HotkeyBindingRow::rebindRequested,
                this, [this](const HotkeyDef& d){ startCapture(d, false); });
        connect(row, &HotkeyBindingRow::appendRebindRequested,
                this, [this](const HotkeyDef& d){ startCapture(d, true); });
        connect(row, &HotkeyBindingRow::clearRequested,
                this, [this](const HotkeyDef& d){
                    m_appController->clearHotkey(m_emuId, d.section, d.key);
                    m_currentValues[d.key].clear();
                    if (auto it = m_rowByKey.constFind(d.key); it != m_rowByKey.constEnd())
                        (*it)->setBindingDisplay(QString());
                });
        currentCardLayout->addWidget(row);
        m_rowByKey.insert(def.key, row);
    }

    contentLayout->addStretch(1);
    scroll->setWidget(content);
    outer->addWidget(scroll, 1);
}

void GenericHotkeyPage::loadBindings() {
    if (!m_appController) return;
    const QVariantList bindings = m_appController->hotkeyBindings(m_emuId);
    for (const auto& v : bindings) {
        const auto map = v.toMap();
        const QString key = map.value(QStringLiteral("key")).toString();
        m_currentValues[key] = map.value(QStringLiteral("currentValue")).toString();
        if (auto it = m_rowByKey.constFind(key); it != m_rowByKey.constEnd())
            (*it)->setBindingDisplay(currentDisplayFor(key));
    }
}

void GenericHotkeyPage::onRowFocused(const HotkeyDef& def) {
    m_focusedRow = m_rowByKey.value(def.key, nullptr);
    emit bindingFocused(def, currentDisplayFor(def.key));
}

void GenericHotkeyPage::rebindFocused() {
    if (m_focusedRow) startCapture(m_focusedRow->def(), false);
}

void GenericHotkeyPage::appendRebindFocused() {
    if (m_focusedRow) startCapture(m_focusedRow->def(), true);
}

void GenericHotkeyPage::clearFocused() {
    if (!m_focusedRow || !m_appController) return;
    const HotkeyDef d = m_focusedRow->def();
    m_appController->clearHotkey(m_emuId, d.section, d.key);
    m_currentValues[d.key].clear();
    m_focusedRow->setBindingDisplay(QString());
    emit bindingFocused(d, QString());
}

void GenericHotkeyPage::restoreDefaults() {
    if (!m_appController) return;
    m_appController->resetHotkeys(m_emuId);
    loadBindings();
    if (m_focusedRow) {
        const HotkeyDef d = m_focusedRow->def();
        emit bindingFocused(d, currentDisplayFor(d.key));
    }
}

QString GenericHotkeyPage::currentDisplayFor(const QString& iniKey) const {
    return displayMultiBinding(m_currentValues.value(iniKey), m_inputManager);
}

void GenericHotkeyPage::startCapture(const HotkeyDef& def, bool append) {
    if (!m_capturingKey.isEmpty()) stopCapture(false);

    m_capturingKey = def.key;
    m_appendMode = append;
    m_capturedBindings.clear();
    m_captureCountdown = 5;

    if (auto it = m_rowByKey.constFind(def.key); it != m_rowByKey.constEnd())
        (*it)->setCapturing(true);

    if (m_inputManager) m_inputManager->startCapture();
    if (auto* w = window()) w->installEventFilter(this);
    m_captureTimer->start();
}

void GenericHotkeyPage::stopCapture(bool save) {
    m_captureTimer->stop();
    // SdlInputManager exposes cancelCapture() (no public stopCapture) — both
    // legacy code paths used cancelCapture() to tear down the capture state.
    if (m_inputManager) m_inputManager->cancelCapture();
    if (auto* w = window()) w->removeEventFilter(this);

    const QString key = m_capturingKey;
    m_capturingKey.clear();

    if (auto it = m_rowByKey.constFind(key); it != m_rowByKey.constEnd())
        (*it)->setCapturing(false);

    if (save && !m_capturedBindings.isEmpty()) {
        finishCapture(m_capturedBindings.join(QStringLiteral(" & ")));
    } else {
        loadBindings();  // refresh display from stored value
    }

    m_capturedBindings.clear();
}

void GenericHotkeyPage::onBindingCaptured(int deviceIndex, const QString& element,
                                          bool isAxis, bool positive) {
    if (m_capturingKey.isEmpty()) return;

    QString formatted;
    if (isAxis) {
        formatted = QStringLiteral("SDL-%1/%2%3")
                        .arg(deviceIndex)
                        .arg(positive ? '+' : '-')
                        .arg(element);
    } else {
        formatted = QStringLiteral("SDL-%1/%2").arg(deviceIndex).arg(element);
    }

    if (m_appendMode) m_capturedBindings.append(formatted);
    else              m_capturedBindings = QStringList{formatted};

    if (!m_appendMode) stopCapture(true);
}

void GenericHotkeyPage::onKeyboardCaptured(const QString& keyString) {
    if (m_capturingKey.isEmpty()) return;

    if (m_appendMode) m_capturedBindings.append(keyString);
    else              m_capturedBindings = QStringList{keyString};

    if (!m_appendMode) stopCapture(true);
}

void GenericHotkeyPage::finishCapture(const QString& formatted) {
    if (!m_focusedRow || !m_appController) return;
    const HotkeyDef d = m_focusedRow->def();
    m_appController->saveHotkey(m_emuId, d.section, d.key, formatted);
    m_currentValues[d.key] = formatted;
    m_focusedRow->setBindingDisplay(currentDisplayFor(d.key));
    emit bindingFocused(d, currentDisplayFor(d.key));
}

bool GenericHotkeyPage::eventFilter(QObject* obj, QEvent* event) {
    if (m_capturingKey.isEmpty()) return QWidget::eventFilter(obj, event);

    if (event->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(event);
        if (isModifierKey(ke->key())) return false;  // wait for the real key
        if (ke->key() == Qt::Key_Escape) {
            stopCapture(false);
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}
