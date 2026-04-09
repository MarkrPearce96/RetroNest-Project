#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

struct ThemeInfo {
    QString id;          // directory name
    QString name;
    QString version;
    QString author;
    QString description;
    QString previewImage; // absolute path
    QString path;         // absolute path to theme directory
    QMap<QString, QString> pages; // pageName → filename
};

class ThemeManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList availableThemes READ availableThemes NOTIFY themesChanged)
    Q_PROPERTY(QString currentThemeId READ currentThemeId WRITE setCurrentThemeId NOTIFY currentThemeChanged)

public:
    explicit ThemeManager(QObject* parent = nullptr);

    void scanThemes(const QString& themesDir);

    QVariantList availableThemes() const;

    QString currentThemeId() const;
    void setCurrentThemeId(const QString& id);

    Q_INVOKABLE QUrl resolve(const QString& pageName) const;
    Q_INVOKABLE QVariantMap themeInfo() const;

signals:
    void themesChanged();
    void currentThemeChanged();

private:
    const ThemeInfo* currentTheme() const;

    QVector<ThemeInfo> m_themes;
    QString m_currentThemeId;
    QString m_themesDir;
};
