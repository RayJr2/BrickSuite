#pragma once

#include <QList>
#include <QSqlDatabase>
#include <QString>

enum class EffectiveSetCompositionSource
{
    None,
    PreferredRebrickableRevision,
    LegacyCatalogFallback
};

struct EffectiveSetCompositionPart
{
    int id = 0;
    int partId = 0;
    int colorId = 0;
    qint64 quantity = 0;
    bool spare = false;
    QString partNumber;
    QString partName;
    QString colorName;
    int rebrickableColorId = 0;
};

struct EffectiveSetComposition
{
    bool success = false;
    EffectiveSetCompositionSource source = EffectiveSetCompositionSource::None;
    int revisionId = 0;
    int revisionVersion = 0;
    QString provider;
    QString message;
    QList<EffectiveSetCompositionPart> parts;
};

class EffectiveSetCompositionRepository
{
public:
    explicit EffectiveSetCompositionRepository(QSqlDatabase database = QSqlDatabase());
    EffectiveSetComposition forSet(int setCatalogId, bool includeSpares = true) const;

private:
    QSqlDatabase m_database;
};
