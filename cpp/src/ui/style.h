#pragma once

#include <QString>

/**
 * App-wide dark theme stylesheet.
 */
inline QString appStyleSheet() {
    return R"(
        * {
            font-family: ".AppleSystemUIFont", "Segoe UI", "Helvetica Neue", sans-serif;
        }

        QWidget#MainWindow {
            background-color: #1a1a2e;
        }

        /* ── Top Tab Bar ─────────────────────────────────── */

        QWidget#TopBar {
            background-color: #141428;
            border-bottom: 1px solid #2a2a4a;
        }

        QPushButton#TabButton {
            background: transparent;
            color: #6c6c9e;
            border: none;
            border-bottom: 2px solid transparent;
            padding: 12px 24px;
            font-size: 13px;
            font-weight: 600;
        }

        QPushButton#TabButton:hover {
            color: #aaaacc;
        }

        QPushButton#TabButtonActive {
            background: transparent;
            color: #ffffff;
            border: none;
            border-bottom: 2px solid #5b5bd6;
            padding: 12px 24px;
            font-size: 13px;
            font-weight: 600;
        }

        /* ── Sidebar ─────────────────────────────────────── */

        QWidget#Sidebar {
            background-color: #16162a;
            border-right: 1px solid #2a2a4a;
        }

        QLabel#SidebarHeader {
            color: #6c6c9e;
            font-size: 11px;
            font-weight: bold;
            padding: 12px 16px 6px 16px;
            text-transform: uppercase;
            letter-spacing: 1px;
        }

        QListWidget#SystemList {
            background: transparent;
            border: none;
            outline: none;
            padding: 4px 8px;
        }

        QListWidget#SystemList::item {
            color: #8888aa;
            padding: 10px 16px;
            border-radius: 8px;
            margin: 2px 0px;
            font-size: 13px;
            font-weight: 500;
        }

        QListWidget#SystemList::item:selected {
            background-color: #2a2a4e;
            color: #ffffff;
        }

        QListWidget#SystemList::item:hover:!selected {
            background-color: #222240;
            color: #bbbbdd;
        }

        QListWidget#SettingsList {
            background: transparent;
            border: none;
            outline: none;
            padding: 4px 8px;
        }

        QListWidget#SettingsList::item {
            color: #8888aa;
            padding: 10px 16px;
            border-radius: 8px;
            margin: 2px 0px;
            font-size: 13px;
            font-weight: 500;
        }

        QListWidget#SettingsList::item:selected {
            background-color: #2a2a4e;
            color: #ffffff;
        }

        QListWidget#SettingsList::item:hover:!selected {
            background-color: #222240;
            color: #bbbbdd;
        }

        /* ── Settings Content ────────────────────────────── */

        QWidget#SettingsContent {
            background-color: #1a1a2e;
        }

        QLabel#SettingsHeader {
            color: #ffffff;
            font-size: 18px;
            font-weight: bold;
            padding: 16px 24px 8px 24px;
        }

        QLabel#SettingsLabel {
            color: #ccccdd;
            font-size: 12px;
            font-weight: 600;
        }

        QLabel#SettingsDescription {
            color: #6c6c9e;
            font-size: 11px;
        }

        QLineEdit#SettingsInput {
            background-color: #2a2a4e;
            color: #ffffff;
            border: 1px solid #3a3a5e;
            border-radius: 4px;
            padding: 4px 8px;
            font-size: 12px;
        }

        QPushButton#SettingsButton {
            background-color: #2a2a4e;
            color: #aaaacc;
            border: 1px solid #3a3a5e;
            border-radius: 6px;
            padding: 8px 16px;
            font-size: 12px;
        }

        QPushButton#SettingsButton:hover {
            background-color: #3a3a5e;
            color: #ffffff;
        }

        QPushButton#InstallButton {
            background-color: #5b5bd6;
            color: #ffffff;
            border: none;
            border-radius: 6px;
            padding: 8px 20px;
            font-size: 12px;
            font-weight: 600;
        }

        QPushButton#InstallButton:hover {
            background-color: #6b6be6;
        }

        QPushButton#InstallButton:disabled {
            background-color: #2a2a4e;
            color: #4a4a6e;
            border: 1px solid #3a3a5e;
        }

        /* ── Tab Widgets (main window) ──────────────────── */

        QTabWidget::pane {
            background-color: #1e1e36;
            border: 1px solid #2a2a4a;
            border-top: none;
        }

        QTabBar::tab {
            background-color: transparent;
            color: #8888aa;
            padding: 8px 18px;
            margin-right: 2px;
            border: none;
            border-bottom: 2px solid transparent;
            font-size: 13px;
        }

        QTabBar::tab:selected {
            color: #ffffff;
            border-bottom: 2px solid #5b5bd6;
        }

        QTabBar::tab:hover:!selected {
            color: #bbbbdd;
            border-bottom: 2px solid #3a3a5e;
        }

        /* ── Form Controls ──────────────────────────────── */

        QComboBox {
            background-color: #252544;
            color: #ddddee;
            border: 1px solid #3a3a5e;
            border-radius: 6px;
            padding: 3px 10px;
            font-size: 12px;
            min-width: 140px;
        }

        QComboBox:hover {
            border-color: #e8922a;
        }

        QComboBox::drop-down {
            border: none;
            width: 22px;
        }

        QComboBox::down-arrow {
            image: none;
            border-left: 3px solid transparent;
            border-right: 3px solid transparent;
            border-top: 4px solid #8888aa;
            margin-right: 6px;
        }

        QComboBox QAbstractItemView {
            background-color: #252544;
            color: #ddddee;
            border: 1px solid #3a3a5e;
            border-radius: 6px;
            selection-background-color: #e8922a;
            selection-color: #ffffff;
            outline: none;
            padding: 4px 0;
        }

        QComboBox QAbstractItemView::item {
            padding: 4px 12px;
        }

        QComboBox QAbstractItemView::item:hover {
            background-color: #e8922a;
            color: #ffffff;
        }

        QSpinBox, QDoubleSpinBox {
            background-color: #252544;
            color: #ddddee;
            border: 1px solid #3a3a5e;
            border-radius: 4px;
            padding: 3px 6px;
            font-size: 12px;
            min-width: 70px;
        }

        QSpinBox:hover, QDoubleSpinBox:hover {
            border-color: #5b5bd6;
        }

        QSpinBox::up-button, QDoubleSpinBox::up-button {
            background-color: #2a2a4e;
            border: none;
            border-left: 1px solid #3a3a5e;
            border-bottom: 1px solid #3a3a5e;
            width: 20px;
        }

        QSpinBox::down-button, QDoubleSpinBox::down-button {
            background-color: #2a2a4e;
            border: none;
            border-left: 1px solid #3a3a5e;
            width: 20px;
        }

        QSpinBox::up-arrow, QDoubleSpinBox::up-arrow {
            border-left: 4px solid transparent;
            border-right: 4px solid transparent;
            border-bottom: 4px solid #8888aa;
        }

        QSpinBox::down-arrow, QDoubleSpinBox::down-arrow {
            border-left: 4px solid transparent;
            border-right: 4px solid transparent;
            border-top: 4px solid #8888aa;
        }

        QCheckBox {
            color: #ccccdd;
            spacing: 6px;
            font-size: 12px;
        }

        QCheckBox::indicator {
            width: 14px;
            height: 14px;
            border: 1px solid #3a3a5e;
            border-radius: 3px;
            background-color: #252544;
        }

        QCheckBox::indicator:checked {
            background-color: #5b5bd6;
            border-color: #5b5bd6;
        }

        QCheckBox::indicator:hover {
            border-color: #5b5bd6;
        }

        QLineEdit {
            background-color: #252544;
            color: #ddddee;
            border: 1px solid #3a3a5e;
            border-radius: 4px;
            padding: 3px 8px;
            font-size: 12px;
        }

        QLineEdit:hover, QLineEdit:focus {
            border-color: #5b5bd6;
        }

        QScrollArea {
            background-color: #1a1a2e;
            border: none;
        }

        QScrollArea > QWidget > QWidget {
            background-color: #1a1a2e;
        }

        /* ── Group Boxes ────────────────────────────────── */

        QGroupBox {
            background-color: #1e1e38;
            border: 1px solid #2a2a4a;
            border-radius: 8px;
            margin-top: 10px;
            padding: 14px 10px 8px 10px;
            font-size: 11px;
            font-weight: 500;
        }

        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 4px;
            color: #9898bb;
        }

        /* ── Toolbar ─────────────────────────────────────── */

        QWidget#Toolbar {
            background-color: #1a1a2e;
            border-bottom: 1px solid #2a2a4a;
        }

        QLabel#HeaderTitle {
            color: #ffffff;
            font-size: 20px;
            font-weight: bold;
        }

        QLabel#GameCount {
            color: #6c6c9e;
            font-size: 12px;
        }

        QPushButton#ToolButton {
            background-color: #2a2a4e;
            color: #aaaacc;
            border: 1px solid #3a3a5e;
            border-radius: 6px;
            padding: 6px 14px;
            font-size: 12px;
            font-weight: 500;
        }

        QPushButton#ToolButton:hover {
            background-color: #3a3a5e;
            color: #ffffff;
            border-color: #4a4a6e;
        }

        QPushButton#ToolButton:pressed {
            background-color: #4a4a6e;
        }

        /* ── Game Grid ───────────────────────────────────── */

        QListWidget#GameGrid {
            background-color: #1a1a2e;
            border: none;
            outline: none;
        }

        QListWidget#GameGrid::item {
            background: transparent;
            border-radius: 8px;
            padding: 4px;
        }

        QListWidget#GameGrid::item:selected {
            background-color: #2a2a4e;
        }

        QListWidget#GameGrid::item:hover:!selected {
            background-color: #222240;
        }

        /* ── Status Bar ──────────────────────────────────── */

        QLabel#StatusBar {
            color: #6c6c9e;
            font-size: 11px;
            padding: 6px 16px;
            background-color: #141428;
            border-top: 1px solid #2a2a4a;
        }

        /* ── Dialogs / General ───────────────────────────── */

        QDialog {
            background-color: #1e1e36;
            color: #ccccdd;
        }

        QDialog QLabel {
            color: #ccccdd;
        }

        QDialog QPushButton {
            background-color: #2a2a4e;
            color: #aaaacc;
            border: 1px solid #3a3a5e;
            border-radius: 4px;
            padding: 6px 16px;
        }

        QDialog QPushButton:hover {
            background-color: #3a3a5e;
            color: #ffffff;
        }

        QMenu {
            background-color: #2a2a4e;
            color: #ccccdd;
            border: 1px solid #3a3a5e;
            border-radius: 6px;
            padding: 4px;
        }

        QMenu::item {
            padding: 6px 24px;
            border-radius: 4px;
        }

        QMenu::item:selected {
            background-color: #3a3a5e;
            color: #ffffff;
        }

        QScrollBar:vertical {
            background: #1a1a2e;
            width: 8px;
            border: none;
        }

        QScrollBar::handle:vertical {
            background: #3a3a5e;
            border-radius: 4px;
            min-height: 30px;
        }

        QScrollBar::handle:vertical:hover {
            background: #4a4a6e;
        }

        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }

        QScrollBar:horizontal {
            height: 0px;
        }

        QInputDialog {
            background-color: #1e1e36;
            color: #ccccdd;
        }

        QInputDialog QComboBox {
            background-color: #2a2a4e;
            color: #ffffff;
            border: 1px solid #3a3a5e;
            padding: 4px 8px;
        }

        QInputDialog QComboBox QAbstractItemView {
            background-color: #2a2a4e;
            color: #ffffff;
            selection-background-color: #3a3a5e;
        }

        QMessageBox {
            background-color: #1e1e36;
            color: #ccccdd;
        }

        QFileDialog {
            background-color: #1e1e36;
        }
    )";
}
