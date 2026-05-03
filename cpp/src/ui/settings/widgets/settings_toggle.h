#pragma once
#include <QAbstractButton>

class QKeyEvent;

class SettingsToggle : public QAbstractButton {
    Q_OBJECT
public:
    explicit SettingsToggle(QWidget* parent = nullptr);
    QSize sizeHint() const override { return QSize(34, 18); }
protected:
    void paintEvent(QPaintEvent*) override;
    void keyPressEvent(QKeyEvent* e) override;
};
