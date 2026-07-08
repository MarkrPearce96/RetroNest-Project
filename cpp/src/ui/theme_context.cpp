#include "theme_context.h"
#include "app_controller.h"
#include "game_list_model.h"
#include "core/database.h"

#include <QHash>
#include <QDesktopServices>
#include <QUrl>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include "core/paths.h"
#include "core/system_registry.h"

ThemeContext::ThemeContext(AppController* app, GameListModel* model, Database* db, QObject* parent)
    : QObject(parent), m_app(app), m_gameModel(model), m_db(db)
{
    connect(m_app, &AppController::systemsChanged, this, &ThemeContext::systemsChanged);
    connect(m_app, &AppController::gamesChanged, this, [this]() {
        reloadGamesAndNotify();
    });
    connect(m_app, &AppController::gameRunningChanged, this, &ThemeContext::gameRunningChanged);
    connect(m_app, &AppController::gameStarted, this, &ThemeContext::gameStarted);
    connect(m_app, &AppController::gameFinished, this, [this](int exitCode, bool crashed) {
        m_gameModel->reload();
        emit gameFinished(exitCode, crashed);
    });
    connect(m_app, &AppController::raGameIdLookupReady, this, &ThemeContext::raGameIdLookupReady);
}

QStringList ThemeContext::systems() const {
    return m_db->allSystems();
}

QVariantMap ThemeContext::systemNames() const {
    QVariantMap map;
    for (const auto& sys : m_db->allSystems())
        map.insert(sys, systemDisplayName(sys));
    return map;
}

QVariantMap ThemeContext::systemGameCounts() const {
    QVariantMap result;
    const auto counts = m_db->systemGameCounts();
    for (auto it = counts.begin(); it != counts.end(); ++it)
        result.insert(it.key(), it.value());
    return result;
}

QVariantMap ThemeContext::systemFavoriteCounts() const {
    QVariantMap result;
    const auto counts = m_db->systemFavoriteCounts();
    for (auto it = counts.begin(); it != counts.end(); ++it)
        result.insert(it.key(), it.value());
    return result;
}

QString ThemeContext::currentSystem() const {
    return m_currentSystem;
}

void ThemeContext::setCurrentSystem(const QString& system) {
    if (m_currentSystem == system) return;
    m_currentSystem = system;
    m_gameModel->loadBySystem(system);
    emit currentSystemChanged();
}

QObject* ThemeContext::gameModel() const {
    return m_gameModel;
}

void ThemeContext::navigateToSystem(const QString& systemId) {
    setCurrentSystem(systemId);
    emit navigateToSystemRequested(systemId);
}

void ThemeContext::navigateBack() {
    m_currentSystem.clear();
    emit navigateBackRequested();
}

void ThemeContext::launchGame(int gameId, const QString& romPath, const QString& emuId) {
    // Check for a resume save state before launching
    if (m_app->hasResumeState(romPath, emuId)) {
        emit resumeStateFound(gameId, romPath, emuId);
        return;
    }

    m_db->recordGameLaunch(gameId);
    m_app->launchGame(gameId, romPath, emuId);
    // Don't reload here — game is now async, reload on finish
}

void ThemeContext::launchGameDirect(int gameId, const QString& romPath, const QString& emuId) {
    m_db->recordGameLaunch(gameId);
    m_app->launchGame(gameId, romPath, emuId);
}

void ThemeContext::launchGameResume(int gameId, const QString& romPath, const QString& emuId) {
    m_db->recordGameLaunch(gameId);

    // Restoration is CoreRuntime's job: startLibretro passes the adapter's
    // findResumeFile() as cfg.resumeStatePath and the core unserializes it
    // post-load. A "fresh" launch differs only by QML clearing the resume
    // file first — no launch args are involved.
    const QString stateFile = m_app->resumeStateFile(romPath, emuId);
    if (!stateFile.isEmpty() && QFileInfo::exists(stateFile))
        qInfo() << "[ThemeContext] Resuming with state file:" << stateFile;
    else
        qWarning() << "[ThemeContext] Resume state file not found, launching normally";
    m_app->launchGame(gameId, romPath, emuId);
}

bool ThemeContext::isGameRunning() const {
    return m_app->isGameRunning();
}

void ThemeContext::stopGame() {
    m_app->stopGame();
}

void ThemeContext::saveAndStopGame(int slot) {
    m_app->saveAndStopGame(slot);
}

bool ThemeContext::hasResumeState(const QString& romPath, const QString& emuId) {
    return m_app->hasResumeState(romPath, emuId);
}

void ThemeContext::clearResumeState(const QString& romPath, const QString& emuId) {
    m_app->clearResumeState(romPath, emuId);
}

void ThemeContext::scrapeGame(int gameId) {
    // Async — the reload happens when AppController emits gamesChanged on
    // scrape completion (constructor connection → reloadGamesAndNotify).
    // Reloading here would only re-emit stale pre-scrape data.
    m_app->scrapeGame(gameId);
}

void ThemeContext::removeGame(int gameId) {
    // ROM-file deletion is GameService's job via its deleteRomFile flag;
    // AppController::removeGame emits gamesChanged, which drives the model
    // reload + systems refresh through reloadGamesAndNotify().
    m_app->removeGame(gameId, /*deleteRomFile=*/true);
}

QVariantMap ThemeContext::gameDetailsByIndex(int index) const {
    QModelIndex mi = m_gameModel->index(index, 0);
    if (!mi.isValid()) return {};
    int gameId = m_gameModel->data(mi, GameListModel::IdRole).toInt();
    return gameDetails(gameId);
}

QVariantMap ThemeContext::gameDetails(int gameId) const {
    // One GameRecord → QVariantMap mapping, shared with the game-list
    // model's role keys (review P7).
    return GameListModel::recordToMap(m_db->gameById(gameId));
}

void ThemeContext::toggleFavorite(int gameId) {
    m_db->toggleFavorite(gameId);
    reloadGamesAndNotify();
}

void ThemeContext::reloadGamesAndNotify() {
    m_gameModel->reload();
    refreshSystems();
    emit gamesChanged();
}

void ThemeContext::importRoms() {
    m_app->importRoms();
}

void ThemeContext::scanRomFolders() {
    m_app->scanRomFolders();
}

void ThemeContext::openRomFolder() {
    QString dir = Paths::romsDir();
    QDir().mkpath(dir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}

void ThemeContext::openGameActions(int gameId) {
    emit gameActionsRequested(gameId);
}

bool ThemeContext::hasScraperCredentials() const {
    return m_app->hasScraperCredentials();
}

bool ThemeContext::hasRACredentials() const {
    return m_app->hasRACredentials();
}

void ThemeContext::raRequestGameIdLookup(const QString& title, const QString& system) {
    m_app->raRequestGameIdLookup(title, system);
}

void ThemeContext::scrapeGameWithProgress(int gameId) {
    if (!m_app->hasScraperCredentials()) {
        qWarning() << "ThemeContext::scrapeGameWithProgress: no scraper credentials configured";
        return;
    }
    emit scrapeGameRequested(gameId);
    m_app->scrapeGame(gameId);
}

void ThemeContext::openGameRomFolder(int gameId) {
    GameRecord g = m_db->gameById(gameId);
    if (g.rom_path.isEmpty()) return;
    QString dir = QFileInfo(g.rom_path).absolutePath();
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}

void ThemeContext::refreshSystems() {
    emit systemsChanged();
}

QString ThemeContext::systemDisplayName(const QString& systemId) {
    // Packet 7 stage 3: names come from manifests/systems.json.
    return SystemRegistry::displayName(systemId);
}

int ThemeContext::currentFocusedGameId() const {
    return m_currentFocusedGameId;
}

void ThemeContext::setCurrentFocusedGameId(int id) {
    if (m_currentFocusedGameId == id) return;
    m_currentFocusedGameId = id;
    emit currentFocusedGameIdChanged();
}
