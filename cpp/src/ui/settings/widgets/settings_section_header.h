#pragma once
#include <QLabel>

class SettingsSectionHeader : public QLabel {
    Q_OBJECT
public:
    explicit SettingsSectionHeader(const QString& text, QWidget* parent = nullptr);
protected:
    void paintEvent(QPaintEvent* e) override;
};
