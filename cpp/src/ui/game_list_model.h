#pragma once

#include "core/database.h"
#include <QAbstractListModel>
#include <QVector>

class GameListModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        TitleRole,
        RomPathRole,
        SystemRole,
        EmulatorIdRole,
        CoverPathRole,
        DescriptionRole,
        DeveloperRole,
        PublisherRole,
        ReleaseDateRole,
        GenresRole,
        RatingRole,
        PlayersRole,
        LastPlayedRole,
        PlayCountRole,
        FavoriteRole,
        ScreenshotPathRole,
        TitlescreenPathRole,
        MarqueePathRole,
        FanartPathRole,
        Box3dPathRole,
        BackcoverPathRole,
        MiximagePathRole,
        PhysicalmediaPathRole,
        ManualPathRole,
        VideoPathRole,
        DiscCountRole,
    };

    explicit GameListModel(Database* db, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const;

    Q_INVOKABLE void loadAll();
    Q_INVOKABLE void loadBySystem(const QString& system);
    Q_INVOKABLE void reload();

    Q_INVOKABLE QString coverImagePath(int row) const;
    Q_INVOKABLE int indexForGameId(int gameId) const;

    void setMediaDir(const QString& dir);

signals:
    void countChanged();

private:
    QString resolveCoverPath(const GameRecord& game) const;

    Database* m_db;
    QVector<GameRecord> m_games;
    QString m_currentSystem;
    QString m_mediaDir;
};
