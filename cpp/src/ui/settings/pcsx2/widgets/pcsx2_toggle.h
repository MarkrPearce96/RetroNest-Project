#pragma once
#include <QAbstractButton>

class Pcsx2Toggle : public QAbstractButton {
    Q_OBJECT
public:
    explicit Pcsx2Toggle(QWidget* parent = nullptr);
    QSize sizeHint() const override { return QSize(34, 18); }
protected:
    void paintEvent(QPaintEvent*) override;
};
