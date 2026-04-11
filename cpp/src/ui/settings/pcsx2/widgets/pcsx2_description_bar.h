#pragma once
#include <QFrame>
#include "core/setting_def.h"

class QLabel;

class Pcsx2DescriptionBar : public QFrame {
    Q_OBJECT
public:
    explicit Pcsx2DescriptionBar(QWidget* parent = nullptr);
    void setSetting(const SettingDef& def);
    void clear();
    // test hooks
    QString descText() const;
    QString recommendedText() const;
private:
    QLabel* m_text = nullptr;
    QLabel* m_rec = nullptr;
};
