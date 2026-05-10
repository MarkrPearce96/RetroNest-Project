#include "game_list_model.h"
#include "core/cover_resolver.h"
#include <QDir>
#include <QFileInfo>

GameListModel::GameListModel(Database* db, QObject* parent)
    : QAbstractListModel(parent), m_db(db)
{
}

int GameListModel::rowCount(const QModelIndex&) const {
    return m_games.size();
}

QVariant GameListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= m_games.size())
        return {};

    const auto& g = m_games[index.row()];
    switch (role) {
    case IdRole:          return g.id;
    case TitleRole:       return g.title;
    case RomPathRole:     return g.rom_path;
    case SystemRole:      return g.system;
    case EmulatorIdRole:  return g.emulator_id;
    case CoverPathRole:   return resolveCoverPath(g);
    case DescriptionRole: return g.description;
    case DeveloperRole:   return g.developer;
    case PublisherRole:   return g.publisher;
    case ReleaseDateRole: return g.release_date;
    case GenresRole:      return g.genres;
    case RatingRole:      return g.rating;
    case PlayersRole:     return g.players;
    case LastPlayedRole:  return g.last_played;
    case PlayCountRole:   return g.play_count;
    case FavoriteRole:    return g.favorite;
    case ScreenshotPathRole:    return g.screenshot_path;
    case TitlescreenPathRole:   return g.titlescreen_path;
    case MarqueePathRole:       return g.marquee_path;
    case FanartPathRole:        return g.fanart_path;
    case Box3dPathRole:         return g.box3d_path;
    case BackcoverPathRole:     return g.backcover_path;
    case MiximagePathRole:      return g.miximage_path;
    case PhysicalmediaPathRole: return g.physicalmedia_path;
    case ManualPathRole:        return g.manual_path;
    case VideoPathRole:         return g.video_path;
    case DiscCountRole:   return g.disc_count;
    default:              return {};
    }
}

QHash<int, QByteArray> GameListModel::roleNames() const {
    return {
        {IdRole,          "gameId"},
        {TitleRole,       "title"},
        {RomPathRole,     "romPath"},
        {SystemRole,      "system"},
        {EmulatorIdRole,  "emulatorId"},
        {CoverPathRole,   "coverPath"},
        {DescriptionRole, "description"},
        {DeveloperRole,   "developer"},
        {PublisherRole,   "publisher"},
        {ReleaseDateRole, "releaseDate"},
        {GenresRole,      "genres"},
        {RatingRole,      "rating"},
        {PlayersRole,     "players"},
        {LastPlayedRole,  "lastPlayed"},
        {PlayCountRole,   "playCount"},
        {FavoriteRole,    "favorite"},
        {ScreenshotPathRole,    "screenshotPath"},
        {TitlescreenPathRole,   "titlescreenPath"},
        {MarqueePathRole,       "marqueePath"},
        {FanartPathRole,        "fanartPath"},
        {Box3dPathRole,         "box3dPath"},
        {BackcoverPathRole,     "backcoverPath"},
        {MiximagePathRole,      "miximagePath"},
        {PhysicalmediaPathRole, "physicalmediaPath"},
        {ManualPathRole,        "manualPath"},
        {VideoPathRole,         "videoPath"},
        {DiscCountRole,         "discCount"},
    };
}

int GameListModel::count() const { return m_games.size(); }

void GameListModel::loadAll() {
    beginResetModel();
    m_games = m_db->allGames();
    m_currentSystem.clear();
    m_coverPathCache.clear();
    endResetModel();
    emit countChanged();
}

void GameListModel::loadBySystem(const QString& system) {
    beginResetModel();
    m_games = m_db->gamesBySystem(system);
    m_currentSystem = system;
    m_coverPathCache.clear();
    endResetModel();
    emit countChanged();
}

void GameListModel::reload() {
    if (m_currentSystem.isEmpty())
        loadAll();
    else
        loadBySystem(m_currentSystem);
}

void GameListModel::setMediaDir(const QString& dir) {
    m_mediaDir = dir;
}

QString GameListModel::coverImagePath(int row) const {
    if (row < 0 || row >= m_games.size()) return {};
    return resolveCoverPath(m_games[row]);
}

int GameListModel::indexForGameId(int gameId) const {
    for (int i = 0; i < m_games.size(); ++i) {
        if (m_games[i].id == gameId)
            return i;
    }
    return -1;
}

QString GameListModel::resolveCoverPath(const GameRecord& game) const {
    auto it = m_coverPathCache.constFind(game.id);
    if (it != m_coverPathCache.constEnd())
        return it.value();
    QString resolved = CoverResolver::resolve(game.cover_path, m_mediaDir,
                                              game.system, game.title, game.rom_path);
    m_coverPathCache.insert(game.id, resolved);
    return resolved;
}
