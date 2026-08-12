#include "ThemeManager.h"

#include <QApplication>
#include <QColor>
#include <QPalette>
#include <QStyle>

void ThemeManager::applySavedTheme(QApplication& application)
{
    applyTheme(application, UserSettings::instance().theme());
}

void ThemeManager::applyTheme(QApplication& application, UserSettings::Theme theme)
{
    switch (theme) {
    case UserSettings::Theme::Light:
        applyLightTheme(application);
        break;

    case UserSettings::Theme::Dark:
        applyDarkTheme(application);
        break;

    case UserSettings::Theme::System:
    default:
        applySystemTheme(application);
        break;
    }
}

void ThemeManager::applySystemTheme(QApplication& application)
{
    application.setStyleSheet(QString());

    application.setPalette(application.style()->standardPalette());
}

void ThemeManager::applyLightTheme(QApplication& application)
{
    QPalette palette = application.style()->standardPalette();

    palette.setColor(QPalette::Button, QColor(0, 96, 96));

    palette.setColor(QPalette::ButtonText, Qt::white);

    palette.setColor(QPalette::Highlight, QColor(30, 90, 160));

    palette.setColor(QPalette::HighlightedText, Qt::white);

    palette.setColor(QPalette::Text, Qt::black);

    palette.setColor(QPalette::WindowText, Qt::black);

    palette.setColor(QPalette::PlaceholderText, QColor(110, 110, 110));

    application.setPalette(palette);

    application.setStyleSheet(globalStyleSheet(UserSettings::Theme::Light));
}

void ThemeManager::applyDarkTheme(QApplication& application)
{
    QPalette palette;

    palette.setColor(QPalette::Window, QColor(43, 43, 43));

    palette.setColor(QPalette::WindowText, Qt::white);

    palette.setColor(QPalette::Base, QColor(30, 30, 30));

    palette.setColor(QPalette::AlternateBase, QColor(43, 43, 43));

    palette.setColor(QPalette::ToolTipBase, QColor(43, 43, 43));

    palette.setColor(QPalette::ToolTipText, Qt::white);

    palette.setColor(QPalette::Text, Qt::white);

    palette.setColor(QPalette::Button, QColor(0, 96, 96));

    palette.setColor(QPalette::ButtonText, Qt::white);

    palette.setColor(QPalette::BrightText, Qt::red);

    palette.setColor(QPalette::Highlight, QColor(42, 130, 218));

    palette.setColor(QPalette::HighlightedText, Qt::white);

    palette.setColor(QPalette::PlaceholderText, QColor(150, 150, 150));

    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(120, 120, 120));

    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(120, 120, 120));

    application.setPalette(palette);

    application.setStyleSheet(globalStyleSheet(UserSettings::Theme::Dark));
}

QString ThemeManager::globalStyleSheet(UserSettings::Theme theme)
{
    const bool isDarkTheme = theme == UserSettings::Theme::Dark;

    const QString borderColor = isDarkTheme ? "#00cccc" : "#555555";

    const QString accentColor = "rgb(0, 96, 96)";

    const QString textColor = isDarkTheme ? "#ffffff" : "#000000";

    const QString backgroundColor = isDarkTheme ? "#2d2d2d" : "#ffffff";

    const QString inputBackground = isDarkTheme ? "#1e1e1e" : "#ffffff";

    const QString hoverBackground = isDarkTheme ? "#009999" : "#cceeff";

    const QString hoverText = isDarkTheme ? "#ffffff" : "#000000";

    const QString disabledText = isDarkTheme ? "#777777" : "#888888";

    const QString arrowIcon = isDarkTheme ? ":/icons/combo_down_light.png"
                                          : ":/icons/combo_down_dark.png";

    return QString(R"(
        QRadioButton::indicator {
            width: 13px;
            height: 13px;
            border-radius: 6px;
            background-color: transparent;
            border: 1px solid %1;
        }

        QRadioButton::indicator:checked {
            background-color: %2;
            border: 1px solid %1;
        }

        QPushButton {
            background-color: %2;
            color: white;
            border: 1px solid %1;
            padding: 4px 8px;
            border-radius: 4px;
        }

        QPushButton:hover {
            background-color: #007a7a;
        }

        QPushButton:pressed {
            background-color: #005050;
        }

        QPushButton:disabled {
            background-color: %4;
            color: %7;
            border-color: #555555;
        }

        QLineEdit,
        QSpinBox,
        QDoubleSpinBox,
        QTextEdit,
        QPlainTextEdit {
            background-color: %5;
            color: %3;
            border: 1px solid %1;
            padding: 3px;
            border-radius: 4px;
        }

        QComboBox {
            color: %3;
            background-color: %5;
            border: 1px solid %1;
            padding: 3px 6px;
            border-radius: 4px;
        }

        QComboBox::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: top right;
            width: 18px;
            border-left: 1px solid %1;
        }

        QComboBox::down-arrow {
            image: url(%9);
            width: 10px;
            height: 10px;
        }

        QComboBox QAbstractItemView {
            background-color: %5;
            color: %3;
            selection-background-color: %2;
            selection-color: white;
            border: 1px solid %1;
            outline: none;
        }

        QComboBox QAbstractItemView::item {
            min-height: 20px;
            padding: 1px 4px;
        }

        QComboBox QAbstractItemView::item:hover {
            background-color: %6;
            color: %8;
        }

        QMenuBar {
            background-color: %4;
            color: %3;
        }

        QMenuBar::item {
            background-color: transparent;
            color: %3;
            padding: 4px 8px;
        }

        QMenuBar::item:selected {
            background-color: %2;
            color: white;
        }

        QMenu {
            background-color: %4;
            color: %3;
            border: 1px solid %1;
        }

        QMenu::item {
            padding: 5px 24px;
        }

        QMenu::item:selected {
            background-color: %2;
            color: white;
        }

        QTabWidget::pane {
            border: 1px solid %1;
            background-color: %4;
            top: -1px;
        }

        QTabBar::tab {
            background-color: %4;
            color: %3;
            border: 1px solid %1;
            padding: 5px 10px;
            border-top-left-radius: 4px;
            border-top-right-radius: 4px;
            margin-right: 2px;
        }

        QTabBar::tab:selected {
            background-color: %2;
            color: white;
            font-weight: bold;
        }

        QTabBar::tab:hover {
            background-color: %6;
            color: %8;
        }

        QGroupBox {
            border: 1px solid %1;
            border-radius: 4px;
            margin-top: 18px;
            background-color: %4;
            color: %3;
        }

        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            padding: 0 4px;
            background-color: %4;
            color: %3;
        }

        QTableWidget,
        QTreeWidget,
        QListWidget {
            background-color: %5;
            color: %3;
            alternate-background-color: %4;
            border: 1px solid %1;
            gridline-color: #444444;
        }

        QHeaderView::section {
            background-color: %4;
            color: %3;
            border: 1px solid %1;
            padding: 4px;
        }

        QTableWidget::item:selected,
        QTreeWidget::item:selected,
        QListWidget::item:selected {
            background-color: %2;
            color: white;
        }

        QToolTip {
            background-color: %4;
            color: %3;
            border: 1px solid %1;
        }
    )")
        .arg(borderColor,     // %1
             accentColor,     // %2
             textColor,       // %3
             backgroundColor, // %4
             inputBackground, // %5
             hoverBackground, // %6
             disabledText,    // %7
             hoverText,       // %8
             arrowIcon);      // %9
}