#include "../src/ui/helpers/ColorComboHelper.h"

#include <QApplication>
#include <QComboBox>
#include <QDebug>
#include <QImage>
#include <QPalette>

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QComboBox combo;
    for (const QColor background : {QColor(Qt::white), QColor(Qt::black)}) {
        QPalette palette = combo.palette();
        palette.setColor(QPalette::Base, background);
        combo.setPalette(palette);
        combo.clear();
        for (const QString hex : {QString("FFFFFF"), QString("#000000"), QString(" 12aBcD ")}) {
            const int index = ColorComboHelper::addColorItem(&combo, hex, 42, hex, true);
            QString normalized = hex.trimmed();
            if (!normalized.startsWith('#')) normalized.prepend('#');
            const QColor expected(normalized);
            if (combo.itemData(index).toInt() != 42 || combo.itemText(index) != hex) return 1;
            if (combo.itemData(index, Qt::ForegroundRole).isValid()) return 6;
            for (const auto mode : {QIcon::Normal, QIcon::Disabled, QIcon::Active, QIcon::Selected}) {
                const QImage image = combo.itemIcon(index).pixmap(16, 16, mode).toImage();
                if (image.isNull() || image.pixelColor(8, 8) != expected) return 2;
            }
        }
        for (const QString hex : {QString(), QString("not-hex"), QString("#GGGGGG")}) {
            const int index = ColorComboHelper::addColorItem(&combo, "Unknown", 99, hex, true);
            combo.setCurrentIndex(index);
            if (!combo.itemIcon(index).isNull() || combo.currentData().toInt() != 99
                || combo.currentText() != "Unknown") return 3;
        }
        const int plain = ColorComboHelper::addColorItem(&combo, "Other dialog", 7, "FF0000");
        if (!combo.itemIcon(plain).isNull() || combo.itemData(plain).toInt() != 7) return 4;
        // A palette change must not recolor an already populated swatch.
        palette.setColor(QPalette::Base, background == QColor(Qt::white) ? Qt::black : Qt::white);
        palette.setColor(QPalette::Text, background == QColor(Qt::white) ? Qt::white : Qt::black);
        palette.setColor(QPalette::HighlightedText, background == QColor(Qt::white) ? Qt::black : Qt::white);
        combo.setPalette(palette);
        if (combo.itemIcon(0).pixmap(16, 16).toImage().pixelColor(8, 8) != QColor(Qt::white)) return 5;
        if (combo.itemData(0, Qt::ForegroundRole).isValid()
            || combo.palette().color(QPalette::Text) != palette.color(QPalette::Text)
            || combo.palette().color(QPalette::HighlightedText) != palette.color(QPalette::HighlightedText)) return 7;
    }
    qInfo() << "Color combo swatch validation passed.";
    return 0;
}
