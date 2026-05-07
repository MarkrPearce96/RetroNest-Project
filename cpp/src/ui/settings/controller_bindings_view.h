#pragma once

#include "core/binding_def.h"

#include <QHash>
#include <QString>
#include <QVector>
#include <QWidget>

class AppController;
class SdlInputManager;
class QLabel;
class QGridLayout;

/**
 * ControllerBindingsView — schema-driven controller mapping page.
 *
 * Loads `adapter->controllerTypes()` (expects exactly one entry —
 * the emulator's primary controller), reads its `BindingDef` list
 * via `adapter->controllerBindingDefsForType(type)`, and renders:
 *
 *   • a centered SVG illustration of the controller
 *   • six fixed grid slots of `SettingsCard`-shaped binding cards
 *     (DPad / FaceButtons / LeftAnalog / RightAnalog / Shoulders / System)
 *   • a "Now editing: <Label> → <Value>" amber footer with colored
 *     gamepad action hints (A Rebind / B Clear / Y Auto-Map / X Close)
 *
 * When a card is focused, the rest of the controller dims and a soft
 * circular cutout brightens just the focused button (OpenEmu pattern).
 *
 * Card focus is forwarded as `bindingFocused(BindingDef)` to the host
 * dialog; activation (Enter / mouse click) becomes `rebindRequested`.
 *
 * Reusable across emulators: any adapter that declares one
 * `ControllerTypeDef` and annotates its `BindingDef`s with `cardSlot`
 * + spotlight coordinates gets this page for free.
 */
class ControllerBindingsView : public QWidget {
    Q_OBJECT
public:
    ControllerBindingsView(SdlInputManager* inputManager,
                           AppController* appController,
                           const QString& emuId,
                           int port,
                           QWidget* parent = nullptr);
    ~ControllerBindingsView() override;

    /// Re-read bindings from AppController and refresh card values.
    /// Call after rebind / clear / auto-map flows.
    void reloadBindings();

signals:
    /// Emitted when the user focuses a card (keyboard nav, mouse hover,
    /// or programmatic). `b.spotlightR == 0` means "no physical button".
    void bindingFocused(const BindingDef& b);

    /// Emitted when the user activates a card (A button / Enter / click).
    /// Host wires this to startCapture for rebinding.
    void rebindRequested(const BindingDef& b);

    /// Emitted when the user requests a clear on the focused card
    /// (B button while card has focus).
    void clearRequested(const BindingDef& b);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    class ImageArea;        // forward — defined in .cpp
    class BindingCard;      // forward — defined in .cpp

    void buildSlots(const QVector<BindingDef>& bindings);
    void onCardFocused(const BindingDef& b);
    void updateNowEditing(const BindingDef& b, const QString& value);
    QString currentValueFor(const QString& key) const;

    SdlInputManager* m_inputManager;
    AppController*   m_appController;
    QString          m_emuId;
    int              m_port;

    ImageArea*       m_imageArea = nullptr;
    QLabel*          m_nowLabel  = nullptr;   // "NOW EDITING" small caps
    QLabel*          m_nowValue  = nullptr;   // "L2 → LTrigger" big readout

    QVector<BindingDef> m_bindings;
    // Map from key → currentValue, refreshed by reloadBindings().
    QHash<QString, QString> m_currentValues;
};
