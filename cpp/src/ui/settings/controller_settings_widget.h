#pragma once

#include <QWidget>
#include <QMap>
#include <QSlider>
#include <QComboBox>
#include <QLabel>

class AppController;

/**
 * ControllerSettingsWidget — renders controller settings (sliders/combos)
 * from the adapter's controllerSettingDefsForType() data.
 */
class ControllerSettingsWidget : public QWidget {
    Q_OBJECT
public:
    ControllerSettingsWidget(AppController* appController,
                             const QString& emuId,
                             int port,
                             QWidget* parent = nullptr);

private:
    void buildUI();
    void onSliderChanged(const QString& key, int value);
    void onComboChanged(const QString& key, int index);

    AppController* m_appController;
    QString m_emuId;
    int m_port;

    struct SliderRow {
        QSlider* slider;
        QLabel* valueLabel;
        QString suffix;
    };

    struct ComboRow {
        QComboBox* combo;
        QVector<QString> values;
    };

    QMap<QString, SliderRow> m_sliders;
    QMap<QString, ComboRow> m_combos;
};
