#pragma once
#include <QWidget>

class Pcsx2Card;
class QPushButton;

class PpssppCategoryHub : public QWidget {
    Q_OBJECT
public:
    explicit PpssppCategoryHub(QWidget* parent = nullptr);

protected:
    bool eventFilter(QObject* obj, QEvent* e) override;

signals:
    void categoryActivated(QString category);
    void openNativeRequested();

private:
    Pcsx2Card* makeCard(const QString& icon, const QString& title,
                        const QString& descriptor, int settingCount,
                        const QString& categoryKey);
    QPushButton* m_nativeBtn = nullptr;
};
