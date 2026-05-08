#pragma once
#include <QHash>
#include <QObject>
#include <QReadWriteLock>
#include <QString>
#include <QVector>
#include <QPair>
#include <atomic>

/**
 * FrontendSettingsStore — per-core JSON sidecar for settings that are
 * RetroNest frontend concerns, not libretro-core-declared options.
 *
 * Parallel to OptionsStore (options.json) but simpler: defaults are
 * plain (key, defaultValue) pairs; there is no "accepted values" list
 * to validate against. Emits frontendSettingChanged() on every set()
 * so consumers (GameSession, QML bindings) can react live.
 */
class FrontendSettingsStore : public QObject {
    Q_OBJECT
public:
    explicit FrontendSettingsStore(QObject* parent = nullptr);

    /** Load from jsonPath, seeding missing keys from defaults.
     *  Pass ":memory:" for a no-write-to-disk test mode. */
    bool load(const QString& jsonPath,
              const QVector<QPair<QString, QString>>& defaults);

    /** Persist current values to disk. */
    bool save() const;

    /** Return the stored value for key, or empty string if unknown. */
    QString get(const QString& key) const;

    /** Set key to value and persist. Emits frontendSettingChanged if
     *  the value changed. No-ops silently for unknown keys. */
    void set(const QString& key, const QString& value);

    /** Atomically check and clear the dirty flag. */
    bool consumeDirty();

signals:
    void frontendSettingChanged(const QString& key, const QString& value);

private:
    mutable QReadWriteLock m_lock;
    QString m_path;
    QHash<QString, QString> m_values;
    std::atomic<bool> m_dirty{false};
};
