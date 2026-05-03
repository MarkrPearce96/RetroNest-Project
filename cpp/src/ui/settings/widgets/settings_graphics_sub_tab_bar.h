#pragma once
#include <QWidget>
#include <QVector>

// Four-box sub-tab bar used by Pcsx2GraphicsPage. Each tab is a
// vertical icon-over-label stack rendered inside a rounded box.
// Keyboard-focusable; Left/Right changes selection, Enter activates.
class SettingsGraphicsSubTabBar : public QWidget {
    Q_OBJECT
public:
    explicit SettingsGraphicsSubTabBar(QWidget* parent = nullptr);

    // Each call adds a tab at the next index. Icon is a short emoji
    // string; label is the tab title shown beneath.
    void addTab(const QString& icon, const QString& label);

    int currentIndex() const { return m_current; }
    int tabCount() const { return m_tabs.size(); }
    void setCurrentIndex(int idx);

    QSize sizeHint() const override;

signals:
    // Emitted whenever the selection changes — either via keyboard or
    // mouse click. Consumer swaps a QStackedWidget's current index.
    void tabActivated(int index);

protected:
    void paintEvent(QPaintEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void focusInEvent(QFocusEvent* e) override;
    void focusOutEvent(QFocusEvent* e) override;

private:
    struct Tab { QString icon; QString label; };
    QVector<Tab> m_tabs;
    int m_current = 0;

    QRect tabRectAt(int idx) const;
};
