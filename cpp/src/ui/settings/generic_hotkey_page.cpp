#include "generic_hotkey_page.h"

#include "core/libretro/libretro_hotkey_defs.h"
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
#include <QMouseEvent>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <cmath>

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
                                     QWidget* parent,
                                     bool dualColumn)
    : QWidget(parent),
      m_inputManager(inputManager),
      m_appController(appController),
      m_emuId(emuId),
      m_dualColumn(dualColumn) {
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
        if (--m_captureCountdown <= 0) {
            stopCapture(true);
            return;
        }
        // Refresh "<captured> [N]" / "Press a button... [N]" each tick so the
        // countdown stays visible — mirrors legacy timer body.
        if (auto it = m_rowByKey.constFind(m_capturingKey);
            it != m_rowByKey.constEnd()) {
            const QString prefix = m_capturedBindings.isEmpty()
                ? QStringLiteral("Press a button...")
                : m_capturedBindings.join(QStringLiteral(" + "));
            (*it)->setCapturingText(
                prefix + QStringLiteral(" [%1]").arg(m_captureCountdown));
        }
    });

    if (m_inputManager) {
        connect(m_inputManager, &SdlInputManager::bindingCaptured,
                this, &GenericHotkeyPage::onBindingCaptured);
        connect(m_inputManager, &SdlInputManager::keyboardCaptured,
                this, &GenericHotkeyPage::onKeyboardCaptured);
        connect(m_inputManager, &SdlInputManager::captureButtonReleased,
                this, [this]{
                    // All SDL controller buttons released. Commit if no
                    // keyboard keys are still held either.
                    m_controllerHeld = false;
                    maybeCommitOnRelease();
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
    contentLayout->setSpacing(4);

    QString currentGroup;
    QFrame* currentCard = nullptr;
    QVBoxLayout* currentCardLayout = nullptr;
    bool firstGroup = true;

    for (const auto& def : m_entries) {
        if (def.group != currentGroup) {
            currentGroup = def.group;
            // For the first group in dual-column mode, render the section
            // title on the SAME horizontal row as the KEYBOARD / CONTROLLER
            // column labels — so the user sees one header bar instead of
            // a section title stacked above the column labels.
            if (m_dualColumn && firstGroup) {
                auto* headerRow = new QWidget(content);
                auto* headerLayout = new QHBoxLayout(headerRow);
                headerLayout->setContentsMargins(0, 8, 0, 4);
                headerLayout->setSpacing(12);

                auto* sectionLbl = new QLabel(currentGroup.toUpper(), headerRow);
                sectionLbl->setStyleSheet(SettingsDialogTheme::sectionHeaderQss());
                sectionLbl->setFixedWidth(220);
                headerLayout->addWidget(sectionLbl);

                const QString columnStyle = QStringLiteral(
                    "color:%1; font-size:11px; font-weight:600; letter-spacing:2px;"
                    "background:transparent;")
                    .arg(SettingsDialogTheme::accent().name());
                auto* kbdHeader = new QLabel(QStringLiteral("KEYBOARD"), headerRow);
                kbdHeader->setAlignment(Qt::AlignCenter);
                kbdHeader->setStyleSheet(columnStyle);
                headerLayout->addWidget(kbdHeader, 1);
                auto* padHeader = new QLabel(QStringLiteral("CONTROLLER"), headerRow);
                padHeader->setAlignment(Qt::AlignCenter);
                padHeader->setStyleSheet(columnStyle);
                headerLayout->addWidget(padHeader, 1);

                contentLayout->addWidget(headerRow);
            } else {
                contentLayout->addSpacing(4);
                contentLayout->addWidget(new SettingsSectionHeader(currentGroup, content));
            }
            firstGroup = false;
            currentCard = new QFrame(content);
            currentCard->setObjectName(QStringLiteral("SettingsCard"));
            currentCard->setStyleSheet(SettingsDialogTheme::cardQss());
            currentCardLayout = new QVBoxLayout(currentCard);
            currentCardLayout->setContentsMargins(8, 8, 8, 8);
            currentCardLayout->setSpacing(2);
            contentLayout->addWidget(currentCard);
        }

        auto* row = new HotkeyBindingRow(def, m_dualColumn, currentCard);
        connect(row, &HotkeyBindingRow::focused,
                this, &GenericHotkeyPage::onRowFocused);
        connect(row, &HotkeyBindingRow::rebindRequested,
                this, [this](const HotkeyDef& d){ startCapture(d, false); });
        connect(row, &HotkeyBindingRow::appendRebindRequested,
                this, [this](const HotkeyDef& d){ startCapture(d, true); });
        connect(row, &HotkeyBindingRow::clearRequested,
                this, [this](const HotkeyDef& d){
                    if (m_dualColumn) {
                        // Clear only the currently-active column's portion.
                        QString kbd, pad;
                        libretro_hotkeys::splitBindingByType(
                            m_currentValues.value(d.key), &kbd, &pad);
                        if (m_focusedRow && m_focusedRow->currentColumn()
                                == HotkeyBindingRow::ColController) {
                            pad.clear();
                        } else {
                            kbd.clear();
                        }
                        const QString merged =
                            libretro_hotkeys::mergeBindingsByType(kbd, pad);
                        m_appController->saveHotkey(m_emuId, d.section, d.key,
                                                    merged);
                        m_currentValues[d.key] = merged;
                    } else {
                        m_appController->clearHotkey(m_emuId, d.section, d.key);
                        m_currentValues[d.key].clear();
                    }
                    refreshRowDisplay(d.key);
                });
        connect(row, &HotkeyBindingRow::columnChanged, this,
                [this](const HotkeyDef& d, HotkeyBindingRow::Column col){
                    // Broadcast: column is a page-level state. When the
                    // user presses Left/Right on the focused row, mirror
                    // the same column highlight across every other row.
                    for (HotkeyBindingRow* other : m_rowByKey)
                        if (other) other->setColumn(col);
                    emit bindingFocused(d, currentDisplayFor(d.key));
                });
        connect(row, &HotkeyBindingRow::navigateRequested,
                this, &GenericHotkeyPage::navigateFromRow);
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
        refreshRowDisplay(key);
    }
}

void GenericHotkeyPage::refreshRowDisplay(const QString& iniKey) {
    auto it = m_rowByKey.constFind(iniKey);
    if (it == m_rowByKey.constEnd()) return;
    if (m_dualColumn) {
        QString kbdRaw, padRaw;
        libretro_hotkeys::splitBindingByType(m_currentValues.value(iniKey),
                                              &kbdRaw, &padRaw);
        const QString kbdDisplay = displayMultiBinding(kbdRaw, m_inputManager);
        const QString padDisplay = displayMultiBinding(padRaw, m_inputManager);
        (*it)->setDualBindingDisplay(kbdDisplay, padDisplay);
    } else {
        (*it)->setBindingDisplay(currentDisplayFor(iniKey));
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
    if (m_dualColumn) {
        QString kbd, pad;
        libretro_hotkeys::splitBindingByType(m_currentValues.value(d.key),
                                              &kbd, &pad);
        if (m_focusedRow->currentColumn() == HotkeyBindingRow::ColController)
            pad.clear();
        else
            kbd.clear();
        const QString merged = libretro_hotkeys::mergeBindingsByType(kbd, pad);
        m_appController->saveHotkey(m_emuId, d.section, d.key, merged);
        m_currentValues[d.key] = merged;
    } else {
        m_appController->clearHotkey(m_emuId, d.section, d.key);
        m_currentValues[d.key].clear();
    }
    refreshRowDisplay(d.key);
    emit bindingFocused(d, currentDisplayFor(d.key));
}

void GenericHotkeyPage::restoreFocusedToDefault() {
    if (!m_focusedRow || !m_appController) return;
    const HotkeyDef d = m_focusedRow->def();
    if (m_dualColumn) {
        // Restore only the focused column's portion to the default's portion
        // of the same type, leaving the opposite column intact.
        QString defKbd, defPad;
        libretro_hotkeys::splitBindingByType(d.defaultValue, &defKbd, &defPad);
        QString curKbd, curPad;
        libretro_hotkeys::splitBindingByType(m_currentValues.value(d.key),
                                              &curKbd, &curPad);
        if (m_focusedRow->currentColumn() == HotkeyBindingRow::ColController)
            curPad = defPad;
        else
            curKbd = defKbd;
        const QString merged = libretro_hotkeys::mergeBindingsByType(curKbd, curPad);
        m_appController->saveHotkey(m_emuId, d.section, d.key, merged);
        m_currentValues[d.key] = merged;
    } else {
        m_appController->saveHotkey(m_emuId, d.section, d.key, d.defaultValue);
        m_currentValues[d.key] = d.defaultValue;
    }
    refreshRowDisplay(d.key);
    emit bindingFocused(d, currentDisplayFor(d.key));
}

void GenericHotkeyPage::focusFirstRow() {
    if (m_entries.isEmpty()) return;
    if (auto it = m_rowByKey.constFind(m_entries.first().key);
        it != m_rowByKey.constEnd()) {
        (*it)->setFocus(Qt::OtherFocusReason);
    }
}

void GenericHotkeyPage::navigateFromRow(int direction) {
    // Walk m_entries (adapter declaration order) from the currently focused
    // row, wrapping at both ends so the user can hold the d-pad without
    // dead zones. Driven by HotkeyBindingRow::navigateRequested so it
    // intercepts arrow keys before the parent QScrollArea can scroll.
    if (!m_focusedRow || m_entries.isEmpty()) return;
    const QString currentKey = m_focusedRow->def().key;
    int idx = -1;
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].key == currentKey) { idx = i; break; }
    }
    if (idx < 0) return;
    const int next = (idx + direction + m_entries.size()) % m_entries.size();
    auto it = m_rowByKey.constFind(m_entries[next].key);
    if (it == m_rowByKey.constEnd()) return;
    HotkeyBindingRow* nextRow = *it;
    nextRow->setFocus(Qt::OtherFocusReason);
    // Scroll the now-focused row into view (mirrors generic_settings_page's
    // arrow-nav handler). Walk up the parent chain rather than caching the
    // QScrollArea — the page makes no assumption about its container.
    for (QWidget* p = nextRow->parentWidget(); p; p = p->parentWidget()) {
        if (auto* sa = qobject_cast<QScrollArea*>(p)) {
            sa->ensureWidgetVisible(nextRow, 20, 40);
            break;
        }
    }
}

void GenericHotkeyPage::keyPressEvent(QKeyEvent* e) {
    // Fallback path: when focus has somehow landed on the page itself
    // (rather than a row), forward Up/Down through navigateFromRow so
    // the user is never stuck. The primary path is the row's own
    // keyPressEvent → navigateRequested signal.
    if (m_focusedRow && (e->key() == Qt::Key_Up || e->key() == Qt::Key_Down)) {
        navigateFromRow(e->key() == Qt::Key_Down ? +1 : -1);
        return;
    }
    QWidget::keyPressEvent(e);
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
    m_heldKeyboardKeys.clear();
    m_controllerHeld = false;
    // Record which column is being captured so we can filter inputs by type
    // and merge the result back into the opposite column's portion.
    m_capturingColumn = (m_dualColumn && m_focusedRow)
                            ? int(m_focusedRow->currentColumn())
                            : int(HotkeyBindingRow::ColKeyboard);

    if (auto it = m_rowByKey.constFind(def.key); it != m_rowByKey.constEnd())
        (*it)->setCapturing(true);

    // Multi-capture mode lets SDL accumulate several controller events for
    // chord support — mirrors legacy hotkey_settings_page.
    if (m_inputManager) {
        m_inputManager->setCaptureMode(true);
        m_inputManager->startCapture();
    }

    // Grab keyboard so the page receives raw key events while capturing,
    // and install the event filter on ourselves (matches legacy semantics).
    grabKeyboard();
    installEventFilter(this);

    m_captureTimer->start();
}

void GenericHotkeyPage::maybeCommitOnRelease() {
    // Commit only when EVERY input that was pressed during this capture
    // session has been released. While anything is still held, stay open
    // so the user can press more keys / buttons.
    if (m_capturingKey.isEmpty()) return;
    if (m_capturedBindings.isEmpty()) return;
    if (!m_heldKeyboardKeys.isEmpty()) return;
    if (m_controllerHeld) return;
    stopCapture(true);
}

void GenericHotkeyPage::stopCapture(bool save) {
    m_captureTimer->stop();
    // SdlInputManager exposes cancelCapture() (no public stopCapture) — both
    // legacy code paths used cancelCapture() to tear down the capture state.
    if (m_inputManager) m_inputManager->cancelCapture();
    releaseKeyboard();
    removeEventFilter(this);

    // Reset the row visual immediately, but keep m_capturingKey / m_appendMode
    // alive so finishCapture() can consume them (legacy snapshots both before
    // delegating to finishCapture).
    if (auto it = m_rowByKey.constFind(m_capturingKey); it != m_rowByKey.constEnd())
        (*it)->setCapturing(false);

    if (save && !m_capturedBindings.isEmpty()) {
        finishCapture(m_capturedBindings.join(QStringLiteral(" & ")));
    } else {
        loadBindings();  // refresh display from stored value
    }

    m_capturingKey.clear();
    m_appendMode = false;
    m_capturedBindings.clear();
}

void GenericHotkeyPage::onBindingCaptured(int deviceIndex, const QString& element,
                                          bool isAxis, bool positive) {
    if (m_capturingKey.isEmpty()) return;
    // Dual-column mode: ignore controller events when the keyboard column
    // is being captured (each column accepts only its own input type).
    if (m_dualColumn && m_capturingColumn != int(HotkeyBindingRow::ColController))
        return;

    // Route through the adapter so PCSX2 / DuckStation / PPSSPP each produce
    // their native binding format (PPSSPP uses numeric "10-19", not "SDL-x/y").
    const QString formatted = m_appController
        ? m_appController->formatCapturedBinding(
              m_emuId, deviceIndex, element, isAxis, positive)
        : QString();
    if (formatted.isEmpty()) return;

    // Combo detection: if a previous gamepad button is still held when a
    // NEW button is pressed (m_controllerHeld true, set on first press,
    // cleared only on SDL's all-released), fuse them into a single combo
    // binding ("Gamepad<P>/<mod>+<btn>") rather than recording two
    // independent bindings. Without this, pressing the modifier alone
    // later would fire the hotkey on its own. Triggers (L2/R2) fuse too —
    // "Select+R2" must become one combo, not a Select-alone + R2-alone pair
    // (the only axis bindings that reach here are triggers; analog sticks are
    // filtered out upstream by formatCapturedBinding).
    bool fused = false;
    if (m_controllerHeld && formatted.startsWith(QStringLiteral("Gamepad"))) {
        for (int i = m_capturedBindings.size() - 1; i >= 0; --i) {
            const QString& prev = m_capturedBindings[i];
            if (!prev.startsWith(QStringLiteral("Gamepad"))) continue;
            if (prev.contains(QLatin1Char('+'))) continue;  // already a combo
            // prev = "Gamepad<P>/<mod>"  →  combo = "Gamepad<P>/<mod>+<btn>"
            const int slash = formatted.indexOf(QLatin1Char('/'));
            if (slash < 0) break;
            const QString actionPart = formatted.mid(slash + 1);
            m_capturedBindings[i] = prev + QStringLiteral("+") + actionPart;
            fused = true;
            break;
        }
    }
    if (!fused && !m_capturedBindings.contains(formatted))
        m_capturedBindings.append(formatted);
    m_controllerHeld = true;

    // Refresh the countdown while inputs are held; commit waits for all
    // releases (handled in maybeCommitOnRelease).
    m_captureCountdown = 5;

    // Refresh the capturing button with "<captured> [N]" — legacy parity.
    if (auto it = m_rowByKey.constFind(m_capturingKey);
        it != m_rowByKey.constEnd()) {
        const QString display = m_capturedBindings.join(QStringLiteral(" + "));
        (*it)->setCapturingText(
            display + QStringLiteral(" [%1]").arg(m_captureCountdown));
    }
}

void GenericHotkeyPage::onKeyboardCaptured(const QString& keyString) {
    // Keyboard capture is handled by eventFilter() against the raw Qt key
    // event so we can use modifiers and adapter formatting; SDL keyboard
    // signals are intentionally ignored here (legacy behavior).
    Q_UNUSED(keyString);
}

void GenericHotkeyPage::finishCapture(const QString& formatted) {
    if (!m_appController) return;

    // Resolve the row by INI key — m_focusedRow may have been moved by other
    // UI events, but m_capturingKey was set at startCapture() time.
    const QString key = m_capturingKey;
    auto it = m_rowByKey.constFind(key);
    if (it == m_rowByKey.constEnd()) return;
    HotkeyBindingRow* row = *it;
    const HotkeyDef d = row->def();
    const QString existing = m_currentValues.value(key);

    QString value;
    if (m_dualColumn) {
        // Merge captured (typed) tokens into the existing string, replacing
        // only the active column's portion. Opposite column is preserved.
        QString existingKbd, existingPad;
        libretro_hotkeys::splitBindingByType(existing, &existingKbd, &existingPad);
        if (m_capturingColumn == int(HotkeyBindingRow::ColController)) {
            const QString newPad = m_appendMode && !existingPad.isEmpty()
                ? existingPad + QStringLiteral(" & ") + formatted
                : formatted;
            value = libretro_hotkeys::mergeBindingsByType(existingKbd, newPad);
        } else {
            const QString newKbd = m_appendMode && !existingKbd.isEmpty()
                ? existingKbd + QStringLiteral(" & ") + formatted
                : formatted;
            value = libretro_hotkeys::mergeBindingsByType(newKbd, existingPad);
        }
    } else {
        const bool doAppend = m_appendMode && !existing.isEmpty();
        value = doAppend ? existing + QStringLiteral(" & ") + formatted
                          : formatted;
    }

    m_appController->saveHotkey(m_emuId, d.section, key, value);
    m_currentValues[key] = value;
    refreshRowDisplay(key);
    emit bindingFocused(d, currentDisplayFor(key));
}

bool GenericHotkeyPage::eventFilter(QObject* obj, QEvent* event) {
    if (m_capturingKey.isEmpty()) return QWidget::eventFilter(obj, event);

    // Redraws the capturing button with "<captured> [N]" countdown text.
    auto refreshCaptureDisplay = [this]() {
        if (auto it = m_rowByKey.constFind(m_capturingKey);
            it != m_rowByKey.constEnd()) {
            const QString display = m_capturedBindings.join(QStringLiteral(" + "));
            (*it)->setCapturingText(
                display + QStringLiteral(" [%1]").arg(m_captureCountdown));
        }
    };

    if (event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->isAutoRepeat()) return true;

        const int key = keyEvent->key();

        // Dual-column mode: drop keyboard events when the controller column
        // is being captured (each column accepts only its own input type).
        if (m_dualColumn && m_capturingColumn == int(HotkeyBindingRow::ColController))
            return true;

        // Escape is bindable — hold/release semantics apply to it like any
        // other key. To cancel an accidental rebind, right-click the row
        // (handled below) or wait for the countdown timer to expire.
        if (isModifierKey(key)) return true;

        // Adapter-formatted keyboard binding (e.g. PCSX2 "Keyboard/D",
        // PPSSPP "1-32") — empty string means unsupported, swallow event.
        const QString binding = m_appController
            ? m_appController->formatCapturedKeyboard(
                  m_emuId, key, static_cast<int>(keyEvent->modifiers()))
            : QString();
        if (binding.isEmpty()) return true;

        if (!m_capturedBindings.contains(binding))
            m_capturedBindings.append(binding);
        m_heldKeyboardKeys.insert(key);

        // Hold-style multi-input: while ANY key is still held, refresh the
        // countdown so the session stays open and the user can press more
        // keys / buttons. The countdown commits only if everything has been
        // released and the timer eventually expires (safety net).
        m_captureCountdown = 5;
        refreshCaptureDisplay();
        return true;
    }

    if (event->type() == QEvent::KeyRelease) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->isAutoRepeat()) return true;
        if (isModifierKey(keyEvent->key())) return true;

        m_heldKeyboardKeys.remove(keyEvent->key());
        maybeCommitOnRelease();
        return true;
    }

    if (event->type() == QEvent::MouseButtonPress
        || event->type() == QEvent::MouseButtonDblClick) {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        // Ignore left click — that's how capture was started.
        if (mouseEvent->button() == Qt::LeftButton) return true;

        const QString binding = m_appController
            ? m_appController->formatCapturedMouse(
                  m_emuId, static_cast<int>(mouseEvent->button()))
            : QString();
        if (binding.isEmpty()) return true;

        if (!m_capturedBindings.contains(binding))
            m_capturedBindings.append(binding);

        refreshCaptureDisplay();
        return true;
    }

    if (event->type() == QEvent::MouseButtonRelease) {
        if (!m_capturedBindings.isEmpty()) stopCapture(true);
        return true;
    }

    if (event->type() == QEvent::Wheel) {
        auto* wheelEvent = static_cast<QWheelEvent*>(event);
        const QPoint delta = wheelEvent->angleDelta();
        // Direction encoding: 0=up, 1=down, 2=left, 3=right.
        int direction = -1;
        if (std::abs(delta.y()) > std::abs(delta.x())) {
            direction = delta.y() > 0 ? 0 : 1;
        } else if (delta.x() != 0) {
            direction = delta.x() > 0 ? 3 : 2;
        }

        if (direction >= 0 && m_appController) {
            const QString binding = m_appController->formatCapturedWheel(m_emuId, direction);
            if (!binding.isEmpty()) {
                m_capturedBindings.clear();  // wheel replaces, doesn't accumulate
                m_capturedBindings.append(binding);
                stopCapture(true);
            }
        }
        return true;
    }

    return QWidget::eventFilter(obj, event);
}
