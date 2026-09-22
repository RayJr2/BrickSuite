#include "ThreeMfWriter.h"
#include <lib3mf_implicit.hpp>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QTemporaryFile>
#include <QColor>
#include <cmath>
bool ThreeMfWriter::write(const PrintGeometry::PrintMesh&m,const QString&path,const Options&o,QString*error){
 if(!(o.uniformScale>0)||!std::isfinite(o.uniformScale)||m.vertices.empty()||m.faces.empty()){if(error)*error="The selected geometry is invalid or empty.";return false;}
 if(!o.modelColor.isValid()){if(error)*error="The selected Model Color is invalid.";return false;}
 try{std::vector<Lib3MF::sPosition>v;v.reserve(m.vertices.size());for(auto&p:m.vertices){if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)){if(error)*error="The selected geometry contains a non-finite vertex.";return false;}v.push_back({float(p.x*o.uniformScale),float(p.y*o.uniformScale),float(p.z*o.uniformScale)});}std::vector<Lib3MF::sTriangle>t;t.reserve(m.faces.size());for(auto&f:m.faces){if(f[0]>=v.size()||f[1]>=v.size()||f[2]>=v.size()){if(error)*error="The selected geometry contains an invalid face.";return false;}t.push_back({f[0],f[1],f[2]});}
 Lib3MF::CWrapper wrapper;auto model=wrapper.CreateModel();model->SetUnit(Lib3MF::eModelUnit::MilliMeter);auto object=model->AddMeshObject();object->SetName(o.objectName.toStdString());object->SetGeometry(v,t);auto colors=model->AddColorGroup();const Lib3MF::sColor color{quint8(o.modelColor.red()),quint8(o.modelColor.green()),quint8(o.modelColor.blue()),255};const auto defaultColorId=colors->AddColor(color);const auto triangleColorId=colors->AddColor(color);const auto colorResourceId=colors->GetUniqueResourceID();object->SetObjectLevelProperty(colorResourceId,defaultColorId);const Lib3MF::sTriangleProperties properties{colorResourceId,{triangleColorId,triangleColorId,triangleColorId}};for(Lib3MF_uint32 i=0;i<object->GetTriangleCount();++i)object->SetTriangleProperties(i,properties);model->AddBuildItem(std::static_pointer_cast<Lib3MF::CObject>(object),wrapper.GetIdentityTransform());auto meta=model->GetMetaDataGroup();meta->AddMetaData("https://rfstateside.com/bricksuite","Application","BrickSuite","string",false);if(!o.partIdentity.isEmpty())meta->AddMetaData("https://rfstateside.com/bricksuite","Part",o.partIdentity.toStdString(),"string",false);
 QTemporaryFile tmp(QFileInfo(path).absolutePath()+QStringLiteral("/.bricksuite-3mf-XXXXXX"));tmp.setAutoRemove(true);if(!tmp.open()){if(error)*error="A temporary 3MF file could not be created.";return false;}const QString tempName=tmp.fileName();tmp.close();model->QueryWriter("3mf")->WriteToFile(tempName.toStdString());QFile source(tempName);if(!source.open(QIODevice::ReadOnly)){if(error)*error="The generated 3MF package could not be read.";return false;}QSaveFile destination(path);if(!destination.open(QIODevice::WriteOnly)||destination.write(source.readAll())<0||!destination.commit()){if(error)*error="The 3MF file could not be committed to its destination.";return false;}return true;
 }catch(const std::exception&e){if(error)*error=QStringLiteral("The 3MF package could not be generated: %1").arg(QString::fromUtf8(e.what()));return false;}}

bool ThreeMfWriter::writeCollection(const QVector<NamedMesh>& meshes, const QString& path,
                                    const Options& options, QString* error)
{
    if (meshes.isEmpty() || !options.modelColor.isValid() ||
        !(options.uniformScale > 0) || !std::isfinite(options.uniformScale)) {
        if (error) *error = QStringLiteral("The calibration package geometry or export options are invalid.");
        return false;
    }
    try {
        Lib3MF::CWrapper wrapper;
        auto model = wrapper.CreateModel();
        model->SetUnit(Lib3MF::eModelUnit::MilliMeter);
        auto colors = model->AddColorGroup();
        const Lib3MF::sColor color{quint8(options.modelColor.red()),
            quint8(options.modelColor.green()), quint8(options.modelColor.blue()), 255};
        const auto objectColor = colors->AddColor(color);
        const auto triangleColor = colors->AddColor(color);
        const auto resourceId = colors->GetUniqueResourceID();
        for (const auto& member : meshes) {
            if (member.name.trimmed().isEmpty() || member.mesh.vertices.empty() || member.mesh.faces.empty() ||
                !std::isfinite(member.translation.x) || !std::isfinite(member.translation.y) ||
                !std::isfinite(member.translation.z)) {
                if (error) *error = QStringLiteral("A named calibration zone has invalid geometry or placement.");
                return false;
            }
            std::vector<Lib3MF::sPosition> vertices;
            vertices.reserve(member.mesh.vertices.size());
            for (const auto& point : member.mesh.vertices) {
                const auto x = (point.x + member.translation.x) * options.uniformScale;
                const auto y = (point.y + member.translation.y) * options.uniformScale;
                const auto z = (point.z + member.translation.z) * options.uniformScale;
                if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) {
                    if (error) *error = QStringLiteral("A calibration zone has a non-finite vertex.");
                    return false;
                }
                vertices.push_back({float(x), float(y), float(z)});
            }
            std::vector<Lib3MF::sTriangle> triangles;
            triangles.reserve(member.mesh.faces.size());
            for (const auto& face : member.mesh.faces) {
                if (face[0] >= vertices.size() || face[1] >= vertices.size() || face[2] >= vertices.size()) {
                    if (error) *error = QStringLiteral("A calibration zone has an invalid face.");
                    return false;
                }
                triangles.push_back({face[0], face[1], face[2]});
            }
            auto object = model->AddMeshObject();
            object->SetName(member.name.toStdString());
            object->SetGeometry(vertices, triangles);
            object->SetObjectLevelProperty(resourceId, objectColor);
            const Lib3MF::sTriangleProperties properties{resourceId,
                {triangleColor, triangleColor, triangleColor}};
            for (Lib3MF_uint32 index = 0; index < object->GetTriangleCount(); ++index)
                object->SetTriangleProperties(index, properties);
            model->AddBuildItem(std::static_pointer_cast<Lib3MF::CObject>(object),
                                wrapper.GetIdentityTransform());
        }
        auto metadata = model->GetMetaDataGroup();
        metadata->AddMetaData("https://rfstateside.com/bricksuite", "Application",
                              "BrickSuite", "string", false);
        if (!options.partIdentity.isEmpty())
            metadata->AddMetaData("https://rfstateside.com/bricksuite", "Part",
                                  options.partIdentity.toStdString(), "string", false);
        QTemporaryFile temporary(QFileInfo(path).absolutePath() +
                                 QStringLiteral("/.bricksuite-3mf-XXXXXX"));
        temporary.setAutoRemove(true);
        if (!temporary.open()) {
            if (error) *error = QStringLiteral("A temporary 3MF file could not be created.");
            return false;
        }
        const QString temporaryName = temporary.fileName();
        temporary.close();
        model->QueryWriter("3mf")->WriteToFile(temporaryName.toStdString());
        QFile source(temporaryName);
        if (!source.open(QIODevice::ReadOnly)) {
            if (error) *error = QStringLiteral("The generated 3MF package could not be read.");
            return false;
        }
        QSaveFile destination(path);
        if (!destination.open(QIODevice::WriteOnly) ||
            destination.write(source.readAll()) < 0 || !destination.commit()) {
            if (error) *error = QStringLiteral("The 3MF file could not be committed to its destination.");
            return false;
        }
        if (error) error->clear();
        return true;
    } catch (const std::exception& exception) {
        if (error) *error = QStringLiteral("The 3MF package could not be generated: %1")
            .arg(QString::fromUtf8(exception.what()));
        return false;
    }
}
