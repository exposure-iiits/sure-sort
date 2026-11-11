#pragma once
#include <QString>

namespace photocull {
inline QString photoUiStyle() {
    return QStringLiteral(R"(
        QWidget { background: #1d1d1f; color: #d4d4d8; font-family: 'Segoe UI'; font-size: 13px; }
        QLabel { background: transparent; }
        QLabel#AppTitle { color: #e4e4e7; font-weight: 600; letter-spacing: .5px; }
        QLabel#muted, QLabel#outputFolder { color: #99999f; }
        QWidget#toolbar, QWidget#photoFooter, QWidget#destinationBar, QWidget#cropBar { background: #1d1d1f; }
        QPushButton { background: #29292d; border: 1px solid #3b3b41; border-radius: 5px; padding: 6px 12px; }
        QPushButton:hover { background: #333338; border-color: #515159; }
        QPushButton:pressed, QPushButton:checked { background: #3a3a41; }
        QPushButton:focus { border-color: #85a9d4; }
        QPushButton:disabled { color: #66666c; border-color: #2a2a2e; background: #222225; }
        QPushButton[toolbarButton="true"] { background: transparent; border: 1px solid transparent; padding: 4px 8px; }
        QPushButton[toolbarButton="true"]:hover { background: #2d2d31; }
        QPushButton[toolbarButton="true"]:checked { background: #38383d; }
        QPushButton[destinationChip="true"] { border: 1px solid #44444b; border-radius: 10px; padding: 4px 12px; }
        QPushButton[destinationChip="true"] { background: #25394a; border-color: #425e76; color: #c4dcf1; }
        QPushButton[destinationChip="true"]:hover { background: #304b62; border-color: #7598b8; }
        QPushButton[destinationChip="true"][assigned="true"] { background: #25394a; border: 2px solid #7bd99b; }
        QPushButton[destinationChip="true"][assigned="true"]:hover { background: #304b62; border: 2px solid #7bd99b; }
        QWidget#quickDestinationEditor { background: #25394a; border: 1px solid #7598b8; border-radius: 10px; }
        QWidget#quickDestinationEditor QLabel { border: 0; color: #c4dcf1; }
        QLineEdit#quickFolderName { background: transparent; border: 0; padding: 3px; color: #e2effa; }
        QPushButton#applyCrop { background: #2f5a3c; color: #d4f5dc; border-color: #416d4e; }
        QPushButton#outputFolder { background: transparent; color:#aaaab3; border:1px solid transparent; border-radius:6px; padding:3px 8px; text-align:left; }
        QPushButton#outputFolder:hover { background:#29292f; border-color:#414147; color:#eeeeef; }
        QPushButton#outputFolder:pressed { background:#34343c; }
        QMenu { background:#252529; color:#d4d4d8; border:1px solid #45454d; border-radius:8px; padding:6px; }
        QMenu::item { background:transparent; padding:8px 14px 8px 10px; border-radius:4px; }
        QMenu::item:selected { background:#34485f; color:#f1f5fa; }
        QMenu::item:disabled { color:#74747e; }
        QMenu::separator { height:1px; background:#414147; margin:5px 8px; }
        QDialog { background: #1d1d1f; }
        QLineEdit, QComboBox, QKeySequenceEdit { background: #161617; border: 1px solid #414147; border-radius: 4px; padding: 5px; }
        QComboBox::drop-down { border: 0; width: 20px; }
        QComboBox QAbstractItemView { background: #252529; color: #d4d4d8; selection-background-color: #34485f; }
        QTableWidget { background: #19191b; alternate-background-color: #222225; border: 1px solid #35353a; gridline-color: #303035; selection-background-color: #34485f; }
        QTableWidget::item { padding: 5px; }
        QHeaderView::section { background: #29292d; color: #bcbcc3; padding: 7px; border: 0; }
        QListWidget#settingsSidebar { background: #18181a; border: 0; padding: 10px 6px; outline: 0; }
        QListWidget#settingsSidebar::item { padding: 10px 14px; margin: 2px 0; border-radius: 5px; }
        QListWidget#settingsSidebar::item:selected { background: #323239; color: #f0f0f2; }
        QListWidget#settingsSidebar::item:hover:!selected { background: #252529; }
        QLabel#settingsTitle { font-size: 21px; font-weight: 600; color: #eeeeef; }
        QLabel#settingsDescription { color: #a1a1a9; }
        QLabel#settingsFeedback { color: #b5c7db; }
        QTabBar#shortcutCategories { background: transparent; }
        QTabBar#shortcutCategories::tab { background: #252529; color: #aaaab3; padding: 10px 14px; border-bottom: 3px solid transparent; }
        QTabBar#shortcutCategories::tab:hover { background: #303036; color: #eeeeef; }
        QTabBar#shortcutCategories::tab:selected { background: #405c7c; color: #ffffff; border-bottom: 4px solid #b5d7ff; font-weight: 700; }
        QLineEdit#shortcutCapture { border: 2px solid #9bc4f5; background: #28384b; color: #ffffff; padding: 4px; }
        QLabel#shortcutToast { background: #49392a; color: #ffe0bd; border: 1px solid #8f6b46; border-radius: 6px; padding: 10px 14px; }
        QSlider::groove:horizontal { height: 4px; background: #3d3d44; border-radius: 2px; }
        QSlider::sub-page:horizontal { background: #8ba8c9; border-radius: 2px; }
        QSlider::handle:horizontal { width: 12px; margin: -5px 0; background: #dddddf; border: 1px solid #95959e; border-radius: 6px; }
        QScrollArea { border: 0; background: #1d1d1f; }
        QScrollBar:vertical { background: #202023; width: 9px; }
        QScrollBar::handle:vertical { background: #484850; min-height: 26px; border-radius: 4px; }
        QScrollBar:horizontal { background: #202023; height: 7px; }
        QScrollBar::handle:horizontal { background: #484850; min-width: 26px; border-radius: 3px; }
        QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
        QToolTip { background: #303035; color: #eeeeef; border: 1px solid #55555d; padding: 4px; }
    )");
}
}
