#pragma once
#include <QWidget>
class QPushButton;
class Pcsx2Card;

class DuckStationCategoryHub : public QWidget {
    Q_OBJECT
public:
    explicit DuckStationCategoryHub(QWidget* parent = nullptr);

signals:
    void categoryActivated(const QString& category);
    void openNativeRequested();

protected:
    bool eventFilter(QObject* obj, QEvent* e) override;

private:
    Pcsx2Card* makeCard(const QString& icon, const QString& title,
                        const QString& descriptor, int settingCount,
                        const QString& categoryKey);
    int countSettings(const QString& category) const;

    QPushButton* m_nativeBtn = nullptr;
    Pcsx2Card* m_stretchCard = nullptr;
};
