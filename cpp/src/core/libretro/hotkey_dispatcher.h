#pragma once
#include <QObject>
#include <QString>
#include <functional>

class HotkeyDispatcher : public QObject {
    Q_OBJECT
public:
    struct Callbacks {
        std::function<void(int)>  saveStateSlot;     // (slot)
        std::function<void(int)>  loadStateSlot;     // (slot)
        std::function<int()>      getCurrentSlot;
        std::function<void(int)>  setCurrentSlot;
        std::function<void()>     toggleFastForward;
        std::function<void(bool)> setFastForward;    // hold-style on/off
        std::function<void()>     togglePause;
        std::function<void()>     reset;
        std::function<void()>     openMenu;
        std::function<void()>     toggleMute;
        std::function<void(int)>  adjustVolume;      // delta percent (+10 / -10)
    };

    explicit HotkeyDispatcher(Callbacks cb, QObject* parent = nullptr);

public slots:
    void onActionPressed(const QString& actionKey);
    void onActionReleased(const QString& actionKey);

private:
    Callbacks m_cb;
};
