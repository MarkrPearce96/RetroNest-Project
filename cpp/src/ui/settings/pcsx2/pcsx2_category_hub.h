#pragma once
#include <QWidget>

class Pcsx2Card;

class Pcsx2CategoryHub : public QWidget {
    Q_OBJECT
public:
    explicit Pcsx2CategoryHub(QWidget* parent = nullptr);
signals:
    void categoryActivated(QString category);
    void openNativeRequested();
private:
    Pcsx2Card* makeCard(const QString& icon, const QString& title,
                        const QString& descriptor, int settingCount,
                        const QString& categoryKey);
};
