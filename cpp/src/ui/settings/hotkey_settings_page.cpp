#include "hotkey_settings_page.h"
#include "binding_widget_common.h"
#include "binding_display.h"
#include "core/sdl_input_manager.h"
#include "ui/app_controller.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QKeySequence>
#include <QLabel>
#include <QScrollArea>
#include <QWheelEvent>
#include <cmath>

// ── Helper: find entry index by INI key ────────────────────────────
static int findEntryIndex(const QVector<HotkeySettingsPage::HotkeyEntry>& entries,
                          const QString& key) {
    for (int i = 0; i < entries.size(); ++i)
        if (entries[i].key == key) return i;
    return -1;
}

// ── Helper: format multi-part binding for display ──────────────────
// INI stores "SDL-0/Back & SDL-0/RightShoulder", display as "SDL-0 Create + SDL-0 R1"
static QString displayMultiBinding(const QString& raw, SdlInputManager* inputMgr) {
    if (raw.isEmpty()) return {};
    const QStringList parts = raw.split(" & ", Qt::SkipEmptyParts);
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
    return displayed.join(" + ");
}

// ── Helper: build tooltip for a binding button ─────────────────────
static void updateButtonTooltip(QPushButton* btn, const QString& displayText) {
    QString tip = displayText.isEmpty() ? "Not bound" : displayText;
    tip += "\n\nLeft click to assign a new button\n"
           "Shift + left click for additional bindings\n"
           "Right click to clear binding";
    btn->setToolTip(tip);
}

// ── Modifier keys that should NOT terminate/commit a capture ───────
// They accumulate into the modifiers() mask of the next real key event.
static bool isModifierKey(int key) {
    return key == Qt::Key_Shift || key == Qt::Key_Control
        || key == Qt::Key_Alt   || key == Qt::Key_Meta;
}

// ═════════════════════════════════════════════════════════════════════
// Constructor
// ═════════════════════════════════════════════════════════════════════

HotkeySettingsPage::HotkeySettingsPage(SdlInputManager* inputManager,
                                       AppController* appController,
                                       const QString& emuId,
                                       QWidget* parent)
    : QDialog(parent)
    , m_inputManager(inputManager)
    , m_appController(appController)
    , m_emuId(emuId)
{
    setWindowTitle("Hotkey Settings");
    setMinimumSize(900, 600);
    resize(900, 600);
    setStyleSheet(QString(
        "QDialog { background: %1; }"
        "QToolTip { background: %1; color: %2; border: 1px solid %3; padding: 6px; font-size: 12px; }"
    ).arg(kBg, kTextPrimary, kBoxBorder));

    // Load hotkey data
    QVariantList bindings = m_appController->hotkeyBindings(m_emuId);
    for (const auto& b : bindings) {
        auto map = b.toMap();
        HotkeyEntry entry;
        entry.label = map["label"].toString();
        entry.group = map["group"].toString();
        if (entry.group.isEmpty()) entry.group = "General";
        entry.section = map["section"].toString();
        entry.key = map["key"].toString();
        entry.currentValue = map["currentValue"].toString();
        m_entries.append(entry);

        if (!m_categories.contains(entry.group))
            m_categories.append(entry.group);
    }

    connect(m_inputManager, &SdlInputManager::bindingCaptured,
            this, &HotkeySettingsPage::onBindingCaptured);
    connect(m_inputManager, &SdlInputManager::keyboardCaptured,
            this, &HotkeySettingsPage::onKeyboardCaptured);
    connect(m_inputManager, &SdlInputManager::captureButtonReleased,
            this, [this]() {
                if (!m_capturingKey.isEmpty() && !m_capturedBindings.isEmpty())
                    stopCapture(true);
            });

    buildUI();

    if (m_categoryList->count() > 0)
        m_categoryList->setCurrentRow(0);
}

// ═════════════════════════════════════════════════════════════════════
// Build UI
// ═════════════════════════════════════════════════════════════════════

void HotkeySettingsPage::buildUI() {
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    // ── Main area: sidebar + content ─────────────────────────────────
    auto* mainArea = new QHBoxLayout;
    mainArea->setContentsMargins(0, 0, 0, 0);
    mainArea->setSpacing(0);

    // Sidebar
    m_categoryList = new QListWidget;
    m_categoryList->setFixedWidth(160);
    m_categoryList->setStyleSheet(QString(
        "QListWidget { background: #1e1e3a; border: none; border-right: 1px solid %1; }"
        "QListWidget::item { padding: 10px 14px; color: %2; font-size: 13px; }"
        "QListWidget::item:selected { background: %3; color: #ffffff; font-weight: bold; }"
    ).arg(kBoxBorder, kTextSecondary, kAccent));

    for (const auto& cat : m_categories)
        m_categoryList->addItem(cat);

    connect(m_categoryList, &QListWidget::currentRowChanged,
            this, &HotkeySettingsPage::showCategory);

    mainArea->addWidget(m_categoryList);

    // Content panel
    auto* contentPanel = new QWidget;
    contentPanel->setStyleSheet(QString("background: %1;").arg(kBg));
    auto* contentLayout = new QVBoxLayout(contentPanel);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);

    // ── Build a page for each category ───────────────────────────────
    for (int ci = 0; ci < m_categories.size(); ++ci) {
        const QString& category = m_categories[ci];

        auto* scrollArea = new QScrollArea;
        scrollArea->setWidgetResizable(true);
        scrollArea->setFrameShape(QFrame::NoFrame);
        scrollArea->setStyleSheet(QString(
            "QScrollArea { background: %1; border: none; }"
            "QScrollBar:vertical { background: %1; width: 8px; }"
            "QScrollBar::handle:vertical { background: #3a3a60; border-radius: 4px; min-height: 30px; }"
            "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        ).arg(kBg));

        auto* pageContent = new QWidget;
        pageContent->setStyleSheet(QString("background: %1;").arg(kBg));
        auto* pageLayout = new QVBoxLayout(pageContent);
        pageLayout->setContentsMargins(24, 20, 24, 16);
        pageLayout->setSpacing(4);

        // Category title
        auto* titleLabel = new QLabel(category);
        titleLabel->setStyleSheet(QString("color: %1; font-size: 15px; font-weight: bold; background: transparent;")
                                      .arg(kTextPrimary));
        pageLayout->addWidget(titleLabel);

        // Subtitle
        auto* subtitleLabel = new QLabel("Click a binding to remap. Right-click to clear.");
        subtitleLabel->setStyleSheet(QString("color: %1; font-size: 12px; background: transparent;")
                                        .arg(kTextSecondary));
        pageLayout->addWidget(subtitleLabel);

        pageLayout->addSpacing(8);

        // Grid of hotkey rows
        auto* grid = new QGridLayout;
        grid->setContentsMargins(0, 0, 0, 0);
        grid->setHorizontalSpacing(16);
        grid->setVerticalSpacing(6);

        int row = 0;
        for (const auto& entry : m_entries) {
            if (entry.group != category) continue;

            auto* label = new QLabel(entry.label);
            label->setFixedWidth(220);
            label->setStyleSheet(QString("color: %1; font-size: 13px; background: transparent;").arg(kTextPrimary));

            auto* btn = new BindBtn();
            btn->setFixedHeight(kBtnH);
            btn->setCursor(Qt::PointingHandCursor);
            btn->setStyleSheet(kBtnStyle);
            btn->setText("Not bound");
            m_bindingButtons[entry.key] = btn;

            connect(btn, &QPushButton::clicked, this, [this, k = entry.key]() { startCapture(k, false); });
            btn->onShiftClick = [this, k = entry.key]() { startCapture(k, true); };
            btn->onRightClick = [this, k = entry.key, s = entry.section]() {
                m_appController->clearHotkey(m_emuId, s, k);
                int idx = findEntryIndex(m_entries, k);
                if (idx >= 0) m_entries[idx].currentValue = "";
                loadBindings();
            };

            grid->addWidget(label, row, 0);
            grid->addWidget(btn, row, 1);
            row++;
        }

        pageLayout->addLayout(grid);
        pageLayout->addStretch(1);

        scrollArea->setWidget(pageContent);
        m_categoryPages.append(scrollArea);
        contentLayout->addWidget(scrollArea);

        if (ci != 0)
            scrollArea->setVisible(false);
    }

    mainArea->addWidget(contentPanel, 1);
    outerLayout->addLayout(mainArea, 1);

    // ── Bottom bar ───────────────────────────────────────────────────
    auto* bottomBar = new QWidget;
    bottomBar->setStyleSheet(QString("background: %1; border-top: 1px solid %2;").arg(kBoxColor, kBoxBorder));
    auto* bottomLayout = new QHBoxLayout(bottomBar);
    bottomLayout->setContentsMargins(16, 10, 16, 10);
    bottomLayout->setSpacing(10);

    bottomLayout->addStretch(1);

    auto* restoreBtn = new QPushButton("Restore Defaults");
    restoreBtn->setFixedHeight(36);
    restoreBtn->setCursor(Qt::PointingHandCursor);
    restoreBtn->setStyleSheet(QString(
        "QPushButton { background: %1; color: %2; border: 1px solid %3;"
        "  border-radius: 6px; font-size: 13px; padding: 6px 16px; }"
        "QPushButton:hover { background: %4; }"
    ).arg(kBtnDefault, kTextPrimary, kBoxBorder, kBtnHover));
    connect(restoreBtn, &QPushButton::clicked, this, &HotkeySettingsPage::onResetDefaults);
    bottomLayout->addWidget(restoreBtn);

    auto* closeBtn = new QPushButton("Close");
    closeBtn->setFixedHeight(36);
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setStyleSheet(QString(
        "QPushButton { background: %1; color: #ffffff; border: none;"
        "  border-radius: 6px; font-size: 13px; font-weight: bold; padding: 6px 24px; }"
        "QPushButton:hover { background: #7c6cf7; }"
    ).arg(kAccent));
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    bottomLayout->addWidget(closeBtn);

    outerLayout->addWidget(bottomBar);

    // Populate binding text
    loadBindings();
}

// ═════════════════════════════════════════════════════════════════════
// Load / refresh bindings
// ═════════════════════════════════════════════════════════════════════

void HotkeySettingsPage::loadBindings() {
    const QVariantList bindings = m_appController->hotkeyBindings(m_emuId);
    for (const auto& b : bindings) {
        const auto map = b.toMap();
        const int idx = findEntryIndex(m_entries, map["key"].toString());
        if (idx >= 0)
            m_entries[idx].currentValue = map["currentValue"].toString();
    }

    for (auto it = m_bindingButtons.constBegin(); it != m_bindingButtons.constEnd(); ++it) {
        QPushButton* btn = it.value();

        const int idx = findEntryIndex(m_entries, it.key());
        const QString val = (idx >= 0) ? m_entries[idx].currentValue : QString();
        const QString display = displayMultiBinding(val, m_inputManager);

        btn->setText(display.isEmpty() ? "Not bound" : display);
        btn->setStyleSheet(kBtnStyle);
        updateButtonTooltip(btn, display);
    }
}

// ═════════════════════════════════════════════════════════════════════
// Category navigation
// ═════════════════════════════════════════════════════════════════════

void HotkeySettingsPage::showCategory(int index) {
    for (int i = 0; i < m_categoryPages.size(); i++)
        m_categoryPages[i]->setVisible(i == index);
}

// ═════════════════════════════════════════════════════════════════════
// Capture flow
// ═════════════════════════════════════════════════════════════════════

void HotkeySettingsPage::startCapture(const QString& key, bool append) {
    // Cancel previous capture if any
    if (!m_capturingKey.isEmpty())
        stopCapture(false);

    m_capturingKey = key;
    m_appendMode = append;
    m_capturedBindings.clear();
    m_captureCountdown = 5;

    auto* btn = m_bindingButtons.value(key);
    if (btn) {
        btn->setText(QString("Press a button... [%1]").arg(m_captureCountdown));
        btn->setStyleSheet(kCapturingStyle);
    }

    // Grab keyboard for direct key capture
    grabKeyboard();
    installEventFilter(this);

    // Also start SDL capture for controller input (multi-capture for chord support)
    m_inputManager->setCaptureMode(true);
    m_inputManager->startCapture();

    // Start countdown timer
    if (!m_captureTimer) {
        m_captureTimer = new QTimer(this);
        connect(m_captureTimer, &QTimer::timeout, this, [this]() {
            m_captureCountdown--;
            if (m_captureCountdown <= 0) {
                stopCapture(true);
                return;
            }
            auto* btn = m_bindingButtons.value(m_capturingKey);
            if (btn)
                btn->setText(QString("Press a button... [%1]").arg(m_captureCountdown));
        });
    }
    m_captureTimer->start(1000);
}

void HotkeySettingsPage::stopCapture(bool save) {
    if (m_captureTimer)
        m_captureTimer->stop();

    releaseKeyboard();
    removeEventFilter(this);
    m_inputManager->cancelCapture();

    if (save && !m_capturedBindings.isEmpty()) {
        finishCapture(m_capturedBindings.join(" & "));
        return;
    }

    // Cancelled (or nothing captured) — restore the button to the stored value.
    if (auto* btn = m_bindingButtons.value(m_capturingKey)) {
        const int idx = findEntryIndex(m_entries, m_capturingKey);
        const QString val = (idx >= 0) ? m_entries[idx].currentValue : QString();
        const QString display = displayMultiBinding(val, m_inputManager);
        btn->setText(display.isEmpty() ? "Not bound" : display);
        btn->setStyleSheet(kBtnStyle);
        updateButtonTooltip(btn, display);
    }
    m_capturingKey.clear();
    m_appendMode = false;
}

bool HotkeySettingsPage::eventFilter(QObject* obj, QEvent* event) {
    if (m_capturingKey.isEmpty())
        return QDialog::eventFilter(obj, event);

    // Redraws the capturing button with "binding [N]" countdown text.
    auto refreshCaptureDisplay = [this]() {
        auto* btn = m_bindingButtons.value(m_capturingKey);
        if (!btn) return;
        const QString display = m_capturedBindings.join(" + ");
        btn->setText(display + QString(" [%1]").arg(m_captureCountdown));
    };

    if (event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->isAutoRepeat()) return true;

        const int key = keyEvent->key();

        // Escape cancels capture
        if (key == Qt::Key_Escape) {
            stopCapture(false);
            return true;
        }

        if (isModifierKey(key))
            return true;

        // Build keyboard binding string via the adapter so each emulator can
        // produce its own format (e.g. PCSX2 "Keyboard/D", PPSSPP "1-32").
        const QString binding = m_appController->formatCapturedKeyboard(
            m_emuId, key, static_cast<int>(keyEvent->modifiers()));
        if (binding.isEmpty())
            return true;  // unsupported key for this emulator — swallow the event

        if (!m_capturedBindings.contains(binding))
            m_capturedBindings.append(binding);

        refreshCaptureDisplay();
        return true; // DON'T stop here — wait for release
    }

    if (event->type() == QEvent::KeyRelease) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->isAutoRepeat()) return true;
        if (isModifierKey(keyEvent->key())) return true;

        // A non-modifier key was released — commit
        if (!m_capturedBindings.isEmpty())
            stopCapture(true);
        return true;
    }

    // Mouse button press — accumulate
    if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonDblClick) {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        // Ignore left click — that's how capture was started
        if (mouseEvent->button() == Qt::LeftButton) return true;

        const QString binding = m_appController->formatCapturedMouse(
            m_emuId, static_cast<int>(mouseEvent->button()));
        if (binding.isEmpty())
            return true;  // unsupported button for this emulator

        if (!m_capturedBindings.contains(binding))
            m_capturedBindings.append(binding);

        refreshCaptureDisplay();
        return true;
    }

    // Mouse button release — commit
    if (event->type() == QEvent::MouseButtonRelease) {
        if (!m_capturedBindings.isEmpty())
            stopCapture(true);
        return true;
    }

    // Mouse wheel — capture and commit immediately (like PCSX2)
    if (event->type() == QEvent::Wheel) {
        auto* wheelEvent = static_cast<QWheelEvent*>(event);
        QPoint delta = wheelEvent->angleDelta();

        // Direction encoding: 0=up, 1=down, 2=left, 3=right
        int direction = -1;
        if (std::abs(delta.y()) > std::abs(delta.x())) {
            direction = delta.y() > 0 ? 0 : 1;
        } else if (delta.x() != 0) {
            direction = delta.x() > 0 ? 3 : 2;
        }

        if (direction >= 0) {
            QString binding = m_appController->formatCapturedWheel(m_emuId, direction);
            if (!binding.isEmpty()) {
                m_capturedBindings.clear(); // wheel replaces, doesn't accumulate
                m_capturedBindings.append(binding);
                stopCapture(true);
            }
        }
        return true;
    }

    return QDialog::eventFilter(obj, event);
}

void HotkeySettingsPage::onBindingCaptured(int deviceIndex, const QString& element,
                                            bool isAxis, bool positive) {
    if (m_capturingKey.isEmpty()) return;

    const QString formatted = m_appController->formatCapturedBinding(
        m_emuId, deviceIndex, element, isAxis, positive);
    if (!m_capturedBindings.contains(formatted))
        m_capturedBindings.append(formatted);

    // Show captured bindings + countdown on the capturing button. Mirrors
    // the refreshCaptureDisplay lambda in eventFilter() — kept inline to
    // avoid widening HotkeySettingsPage's private interface.
    if (auto* btn = m_bindingButtons.value(m_capturingKey)) {
        const QString display = m_capturedBindings.join(" + ");
        btn->setText(display + QString(" [%1]").arg(m_captureCountdown));
    }

    // For controller buttons, don't stop immediately — allow chord accumulation.
    // The timer will finalize, or the user can click elsewhere.
}

void HotkeySettingsPage::onKeyboardCaptured(const QString& keyString) {
    // Keyboard capture handled by eventFilter() — ignore SDL keyboard events
    Q_UNUSED(keyString);
}

void HotkeySettingsPage::finishCapture(const QString& formatted) {
    const QString key = m_capturingKey;
    const bool append = m_appendMode;
    m_capturingKey.clear();
    m_appendMode = false;

    const int idx = findEntryIndex(m_entries, key);
    if (idx >= 0) {
        // Append with " & " separator (PCSX2 chord format) when shift-clicking.
        const bool doAppend = append && !m_entries[idx].currentValue.isEmpty();
        const QString value = doAppend
            ? m_entries[idx].currentValue + " & " + formatted
            : formatted;
        m_entries[idx].currentValue = value;
        m_appController->saveHotkey(m_emuId, m_entries[idx].section, key, value);
    }
    loadBindings();
}

// ═════════════════════════════════════════════════════════════════════
// Reset defaults
// ═════════════════════════════════════════════════════════════════════

void HotkeySettingsPage::onResetDefaults() {
    m_appController->resetHotkeys(m_emuId);
    loadBindings();
}
