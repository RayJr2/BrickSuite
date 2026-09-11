/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 *
 * This file is part of BrickSuite.
 *
 * BrickSuite is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, version 3 of the License.
 *
 * BrickSuite is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with BrickSuite. If not, see <https://www.gnu.org/licenses/>.
 */

#include "PartSearchCompleterHelper.h"
#include "../../models/Part.h"
#include "../../repositories/PartRepository.h"
#include <QAbstractItemView>
#include <QCompleter>
#include <QLineEdit>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QTimer>
#include <QVariant>

namespace PartSearchCompleterHelper {
void install(QLineEdit* edit, std::function<void()> resolutionChanged)
{
    if (!edit || edit->completer())
        return;

    auto* model = new QStandardItemModel(edit);
    auto* completer = new QCompleter(model, edit);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
    completer->setMaxVisibleItems(12);
    edit->setCompleter(completer);

    auto* timer = new QTimer(edit);
    timer->setSingleShot(true);
    timer->setInterval(200);

    QObject::connect(edit, &QLineEdit::textChanged, edit,
                     [edit, resolutionChanged]() {
        edit->setProperty("canonicalPartNumber", QVariant());
        if (resolutionChanged)
            resolutionChanged();
    });
    QObject::connect(edit, &QLineEdit::textEdited, edit, [timer]() {
        timer->start();
    });

    QObject::connect(timer, &QTimer::timeout, edit, [edit, model, completer]() {
        model->clear();
        const QString text = edit->text().trimmed();

        if (text.size() < 2) {
            completer->popup()->hide();
            return;
        }

        for (const Part& part : PartRepository().searchForInventoryEntry(text, 20)) {
            auto* item = new QStandardItem(
                QStringLiteral("%1 — %2").arg(part.partNumber(), part.name()));
            item->setData(part.partNumber(), Qt::UserRole + 1);
            model->appendRow(item);
        }

        if (model->rowCount() > 0)
            completer->complete();
    });

    QObject::connect(completer, QOverload<const QModelIndex&>::of(&QCompleter::activated),
                     edit, [edit, resolutionChanged](const QModelIndex& index) {
        const QString number = index.data(Qt::UserRole + 1).toString();

        if (number.isEmpty())
            return;

        edit->setProperty("canonicalPartNumber", number);
        if (resolutionChanged)
            resolutionChanged();
    });
}

QString canonicalPartNumber(const QLineEdit* edit)
{
    if (!edit)
        return {};

    const QString selected = edit->property("canonicalPartNumber").toString();
    if (!selected.isEmpty())
        return selected;

    const QString visibleText = edit->text().trimmed();
    if (QCompleter* completer = edit->completer()) {
        QAbstractItemModel* model = completer->completionModel();
        for (int row = 0; model && row < model->rowCount(); ++row) {
            const QModelIndex index = model->index(row, 0);
            if (index.data(Qt::DisplayRole).toString() == visibleText) {
                const QString canonical = index.data(Qt::UserRole + 1).toString();
                if (!canonical.isEmpty())
                    return canonical;
            }
        }
    }

    return visibleText;
}

bool hasResolvablePart(const QLineEdit* edit)
{
    const QString partNumber = canonicalPartNumber(edit);
    return !partNumber.isEmpty() && PartRepository().getByPartNumber(partNumber).has_value();
}
}
