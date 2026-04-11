#pragma once
#include <QLabel>

class Pcsx2SectionHeader : public QLabel {
    Q_OBJECT
public:
    explicit Pcsx2SectionHeader(const QString& text, QWidget* parent = nullptr);
protected:
    void paintEvent(QPaintEvent* e) override;
};
