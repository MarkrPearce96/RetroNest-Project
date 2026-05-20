#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

class HotkeyService : public QObject {
    Q_OBJECT
public:
    explicit HotkeyService(QObject* parent = nullptr);

    QVariantList hotkeyBindings(const QString& emuId) const;
    void saveHotkey(const QString& emuId, const QString& section,
                    const QString& key, const QString& value);
    void clearHotkey(const QString& emuId, const QString& section,
                     const QString& key);
    void resetHotkeys(const QString& emuId);

signals:
    void statusMessage(const QString& msg);
};
