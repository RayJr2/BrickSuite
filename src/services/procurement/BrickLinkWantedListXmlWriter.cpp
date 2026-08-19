/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 */
#include "BrickLinkWantedListXmlWriter.h"

#include <QRegularExpression>
#include <QSet>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

BrickLinkWantedListXmlWriter::Result
BrickLinkWantedListXmlWriter::write(
    const ProcurementDraft& draft,
    const BrickLinkWantedListOptions& options) const
{
    Result result;

    if (draft.items.isEmpty()) {
        result.message = QStringLiteral("The procurement draft has no items.");
        return result;
    }

    QSet<QString> uniqueKeys;

    for (const ProcurementItem& item : draft.items) {
        if (!item.ready()) {
            result.message =
                QStringLiteral("All procurement rows must be Ready before BrickLink XML can be generated.");
            return result;
        }

        const QString itemId = item.effectiveItemId().trimmed();
        const QString colorId = item.effectiveColorId().trimmed();

        if (itemId.isEmpty()) {
            result.message = QStringLiteral("A BrickLink ITEMID is missing.");
            return result;
        }

        bool colorOk = false;
        const int numericColorId = colorId.toInt(&colorOk);

        if (!colorOk || numericColorId < 0) {
            result.message =
                QStringLiteral("A BrickLink COLOR value is missing or invalid.");
            return result;
        }

        if (item.quantityNeeded <= 0) {
            result.message =
                QStringLiteral("A procurement row has an invalid missing quantity.");
            return result;
        }

        const QString uniqueKey =
            QStringLiteral("%1|%2").arg(itemId, colorId);

        if (uniqueKeys.contains(uniqueKey)) {
            result.message =
                QStringLiteral("Duplicate BrickLink ITEMID/COLOR rows remain in the procurement draft.");
            return result;
        }

        uniqueKeys.insert(uniqueKey);
    }

    QString xml;
    QXmlStreamWriter writer(&xml);
    writer.setAutoFormatting(true);
    writer.setAutoFormattingIndent(4);

    // BrickLink's paste workflow expects INVENTORY as the root and does not
    // require an XML declaration, so intentionally do not call writeStartDocument().
    writer.writeStartElement(QStringLiteral("INVENTORY"));

    const QString remarks = remarksForDraft(draft, options);

    for (const ProcurementItem& item : draft.items) {
        writer.writeStartElement(QStringLiteral("ITEM"));

        writer.writeTextElement(QStringLiteral("ITEMTYPE"),
                                QStringLiteral("P"));

        writer.writeTextElement(QStringLiteral("ITEMID"),
                                item.effectiveItemId().trimmed());

        writer.writeTextElement(QStringLiteral("COLOR"),
                                item.effectiveColorId().trimmed());

        writer.writeTextElement(QStringLiteral("MINQTY"),
                                QString::number(item.quantityNeeded));

        if (!options.condition.trimmed().isEmpty()) {
            writer.writeTextElement(QStringLiteral("CONDITION"),
                                    options.condition.trimmed());
        }

        if (!options.notify.trimmed().isEmpty()) {
            writer.writeTextElement(QStringLiteral("NOTIFY"),
                                    options.notify.trimmed());
        }

        if (!options.wantedShow.trimmed().isEmpty()) {
            writer.writeTextElement(QStringLiteral("WANTEDSHOW"),
                                    options.wantedShow.trimmed());
        }

        if (!remarks.isEmpty()) {
            writer.writeTextElement(QStringLiteral("REMARKS"), remarks);
        }

        writer.writeEndElement();

        ++result.itemRows;
        result.totalPieces += item.quantityNeeded;
    }

    writer.writeEndElement();

    // QXmlStreamWriter produces valid escaping, but do one lightweight parse
    // as a defensive check before exposing Copy/Save.
    if (xml.trimmed().startsWith(QStringLiteral("<?xml"),
                                 Qt::CaseInsensitive)) {
        result.message =
            QStringLiteral("Generated BrickLink XML unexpectedly contains an XML declaration.");
        return result;
    }

    QXmlStreamReader reader(xml);

    QString rootName;
    int itemCount = 0;

    while (!reader.atEnd()) {
        reader.readNext();

        if (reader.isStartElement()) {
            if (rootName.isEmpty())
                rootName = reader.name().toString();

            if (reader.name() == QStringLiteral("ITEM"))
                ++itemCount;
        }
    }

    if (reader.hasError()) {
        result.message =
            QStringLiteral("Generated BrickLink XML failed internal XML validation.");
        return result;
    }

    if (rootName != QStringLiteral("INVENTORY")) {
        result.message =
            QStringLiteral("Generated BrickLink XML does not have an INVENTORY root.");
        return result;
    }

    if (itemCount <= 0 || itemCount != result.itemRows) {
        result.message =
            QStringLiteral("Generated BrickLink XML contains an unexpected number of ITEM rows.");
        return result;
    }

    // Keep saved/copied XML deterministic and human-readable.
    if (!xml.endsWith(QLatin1Char('\n')))
        xml.append(QLatin1Char('\n'));

    result.success = true;
    result.xml = xml;
    result.message =
        QStringLiteral("BrickLink Wanted List XML generated successfully.");

    return result;
}

QString BrickLinkWantedListXmlWriter::remarksForDraft(
    const ProcurementDraft& draft,
    const BrickLinkWantedListOptions& options) const
{
    if (options.remarksMode == QStringLiteral("BuildName"))
        return draft.buildName.trimmed();

    if (options.remarksMode == QStringLiteral("Custom"))
        return options.customRemarks.trimmed();

    return QString();
}
