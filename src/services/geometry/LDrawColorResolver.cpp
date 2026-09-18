#include "LDrawColorResolver.h"

#include <QColor>
#include <QHash>

namespace {
QVector4D vector(const QColor& color)
{ return QVector4D(color.redF(),color.greenF(),color.blueF(),color.alphaF()); }

QColor directColor(const QString& value)
{
    const QString text=value.trimmed();
    if ((text.startsWith(QStringLiteral("0x2"),Qt::CaseInsensitive)
         || text.startsWith(QStringLiteral("0x3"),Qt::CaseInsensitive)) && text.size()==9) {
        bool ok=false; const uint rgb=text.mid(3).toUInt(&ok,16);
        if(ok) return QColor::fromRgb(rgb);
    }
    return {};
}

QColor standard(const QString& value)
{
    static const QHash<int,QColor> colors={
        {0,QColor("#05131D")},{1,QColor("#0055BF")},{2,QColor("#237841")},
        {3,QColor("#008F9B")},{4,QColor("#C91A09")},{5,QColor("#C870A0")},
        {6,QColor("#583927")},{7,QColor("#9BA19D")},{8,QColor("#6D6E5C")},
        {9,QColor("#B4D2E3")},{10,QColor("#4B9F4A")},{11,QColor("#55A5AF")},
        {12,QColor("#F2705E")},{13,QColor("#FC97AC")},{14,QColor("#F2CD37")},
        {15,QColor("#FFFFFF")},{17,QColor("#C2DAB8")},{18,QColor("#FBE696")},
        {19,QColor("#E4CD9E")},{20,QColor("#C9CAE2")},{22,QColor("#81007B")},
        {23,QColor("#2032B0")},{25,QColor("#FE8A18")},{26,QColor("#923978")},
        {27,QColor("#BBE90B")},{28,QColor("#958A73")},{29,QColor("#E4ADC8")},
        {36,QColor("#C91A09")},{40,QColor("#635F52")},{71,QColor("#A0A5A9")},
        {72,QColor("#6C6E68")},{272,QColor("#0A3463")},{288,QColor("#184632")},
        {320,QColor("#720E0F")},{321,QColor("#078BC9")},{322,QColor("#68BCC5")},
        {323,QColor("#ADC3C0")},{326,QColor("#E2F99A")},{330,QColor("#77774E")}
    };
    bool ok=false; const int code=value.trimmed().toInt(&ok);
    return ok ? colors.value(code) : QColor();
}
}

QVector4D LDrawColorResolver::faceColor(const QString& value)
{
    if(value.trimmed()==QStringLiteral("16")) return vector(QColor("#C0C5C8"));
    const QColor direct=directColor(value); if(direct.isValid()) return vector(direct);
    const QColor known=standard(value); return vector(known.isValid()?known:QColor("#C0C5C8"));
}

QVector4D LDrawColorResolver::edgeColor(const QString& value)
{
    const QColor direct=directColor(value); if(direct.isValid()) return vector(direct.darker(180));
    Q_UNUSED(value); return vector(QColor("#111417"));
}

QVector4D LDrawColorResolver::wireframeColor()
{ return vector(QColor("#C8CDD0")); }
