#pragma once
#include <QObject>
#include <QString>
#include "core_loader.h"

struct rc_client_t;

/**
 * In-process RetroAchievements client. Lives on the core thread; emits
 * achievement-unlock signals queued back to RAService on the main thread.
 *
 * The HTTP layer (login + achievement-set fetch + unlock posts) is
 * stubbed in v1. Real network integration is a follow-up task — see
 * TODO(rcheevos-http) in the .cpp.
 */
class RcheevosRuntime : public QObject {
    Q_OBJECT
public:
    explicit RcheevosRuntime(QObject* parent = nullptr);
    ~RcheevosRuntime() override;

    /** Begin a session for the loaded game. Synchronous: builds the
     *  rcheevos memory descriptor list from the core's memory regions and
     *  registers it. Returns false (and emits loginRequired) if no token
     *  is available. */
    bool beginSession(const CoreSymbols& syms,
                      const QString& romPath,
                      int raConsoleId,
                      const QString& token,
                      bool hardcore);
    void endSession();
    /** Per-frame tick. Cheap when no session is active. */
    void frame();

signals:
    void achievementUnlocked(const QString& id, const QString& title,
                             const QString& description);
    void loginRequired();

private:
    rc_client_t* m_client = nullptr;
    bool m_inSession = false;
};
