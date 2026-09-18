#include "InventoryCsvWriter.h"

#include <QFile>
#include <QTextStream>

namespace {
QString field(QString value, bool quoteAlways)
{
    value.replace('"', QStringLiteral("\"\""));
    if (quoteAlways || value.contains(',') || value.contains('"')
        || value.contains('\n') || value.contains('\r')) return QStringLiteral("\"%1\"").arg(value);
    return value;
}
}

InventoryCsvWriter::Result InventoryCsvWriter::generate(const InventoryExportProjection& p)
{
    Result result;
    if (p.fields.isEmpty()) { result.message=QStringLiteral("Select at least one export field."); return result; }
    QTextStream out(&result.csv); out << QChar(0xFEFF);
    for(int c=0;c<p.headers.size();++c){if(c)out<<',';out<<field(p.headers.at(c),false);}out<<'\n';
    for(const auto& row:p.rows){for(int c=0;c<p.fields.size();++c){if(c)out<<',';out<<field(c<row.size()?row.at(c):QString(),!p.fields.at(c).numeric);}out<<'\n';}
    result.success=true; result.message=QStringLiteral("Inventory CSV generated successfully."); return result;
}

InventoryCsvWriter::Result InventoryCsvWriter::write(const QString& name,const InventoryExportProjection&p)
{
    Result result=generate(p);if(!result.success)return result;QFile file(name);
    if(!file.open(QIODevice::WriteOnly|QIODevice::Text)){result.success=false;result.message=QStringLiteral("Unable to create:\n\n%1").arg(name);return result;}
    QTextStream out(&file);out<<result.csv;if(out.status()!=QTextStream::Ok){result.success=false;result.message=QStringLiteral("Unable to write:\n\n%1").arg(name);}return result;
}
