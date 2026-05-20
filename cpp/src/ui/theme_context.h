#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>

class AppController;
class GameListModel;
class Database;

class ThemeContext : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList systems READ systems NOTIFY systemsChanged)
    Q_PROPERTY(QVariantMap systemNames READ systemNames NOTIFY systemsChanged)
    Q_PROPERTY(QVariantMap systemGameCounts READ systemGameCounts NOTIFY systemsChanged)
    Q_PROPERTY(QVariantMap systemFavoriteCounts READ systemFavoriteCounts NOTIFY gamesChanged)
    Q_PROPERTY(QString currentSystem READ currentSystem WRITE setCurrentSystem NOTIFY currentSystemChanged)
    Q_PROPERTY(QObject* gameModel READ gameModel CONSTANT)
    Q_PROPERTY(bool gameRunning READ isGameRunning NOTIFY gameRunningChanged)
    Q_PROPERTY(int currentFocusedGameId READ currentFocusedGameId
               WRITE setCurrentFocusedGameId
               NOTIFY currentFocusedGameIdChanged)

public:
    ThemeContext(AppController* app, GameListModel* model, Database* db, QObject* parent = nullptr);

    QStringList systems() const;
    QVariantMap systemNames() const;
    QVariantMap systemGameCounts() const;
    QVariantMap systemFavoriteCounts() const;
    QString currentSystem() const;
    void setCurrentSystem(const QString& system);
    QObject* gameModel() const;
    int currentFocusedGameId() const;
    void setCurrentFocusedGameId(int id);

    // Navigation
    Q_INVOKABLE void navigateToSystem(const QString& systemId);
    Q_INVOKABLE void navigateBack();

    bool isGameRunning() const;

    // Game operations
    Q_INVOKABLE void launchGame(int gameId, const QString& romPath, const QString& emuId);
    Q_INVOKABLE void launchGameDirect(int gameId, const QString& romPath, const QString& emuId);
    Q_INVOKABLE void launchGameResume(int gameId, const QString& romPath, const QString& emuId);
    Q_INVOKABLE void scrapeGame(int gameId);
    Q_INVOKABLE void removeGame(int gameId);
    Q_INVOKABLE QVariantMap gameDetails(int gameId) const;
    Q_INVOKABLE QVariantMap gameDetailsByIndex(int index) const;
    Q_INVOKABLE void toggleFavorite(int gameId);
    Q_INVOKABLE void openGameActions(int gameId);
    Q_INVOKABLE void scrapeGameWithProgress(int gameId);
    Q_INVOKABLE bool hasScraperCredentials() const;
    Q_INVOKABLE bool hasRACredentials() const;
    Q_INVOKABLE void raRequestGameIdLookup(const QString& title, const QString& system = {});
    Q_INVOKABLE void openGameRomFolder(int gameId);

    // Async game control
    Q_INVOKABLE void stopGame();
    Q_INVOKABLE void saveAndStopGame(int slot);
    Q_INVOKABLE bool hasResumeState(const QString& romPath, const QString& emuId);
    Q_INVOKABLE void clearResumeState(const QString& romPath, const QString& emuId);

    // Library operations
    Q_INVOKABLE void importRoms();
    Q_INVOKABLE void scanRomFolders();
    Q_INVOKABLE void openRomFolder();

    // Refresh
    Q_INVOKABLE void refreshSystems();

signals:
    void navigateToSystemRequested(const QString& systemId);
    void navigateBackRequested();
    void systemsChanged();
    void currentSystemChanged();
    void gamesChanged();
    void gameActionsRequested(int gameId);
    void scrapeGameRequested(int gameId);
    void gameRunningChanged();
    void currentFocusedGameIdChanged();
    void gameStarted();
    void gameFinished(int exitCode, bool crashed);
    void resumeStateFound(int gameId, const QString& romPath, const QString& emuId);
    void raGameIdLookupReady(const QString& title, int raGameId);

private:
    static QString systemDisplayName(const QString& systemId);

    AppController* m_app;
    GameListModel* m_gameModel;
    Database* m_db;
    QString m_currentSystem;
    int m_currentFocusedGameId = -1;
};
