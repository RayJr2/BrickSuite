#include "TechnicAxleSemantic.h"

#include <QCryptographicHash>
#include <QStringList>
#include <QVector3D>
#include <cmath>

namespace PrintGeometry { namespace {
Point convert(const QVector3D&p){return {double(p.x())*.4,double(-p.z())*.4,double(-p.y())*.4};}
Point direction(const std::array<double,12>&t,double x,double y,double z){QVector3D p(float(t[0]*x+t[1]*y+t[2]*z),float(t[4]*x+t[5]*y+t[6]*z),float(t[8]*x+t[9]*y+t[10]*z));p=QVector3D(p.x(),-p.z(),-p.y());if(!qFuzzyIsNull(p.length()))p.normalize();return {p.x(),p.y(),p.z()};}
Point cross(const Point&a,const Point&b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z};}
double axialScale(const std::array<double,12>&t){return .4*std::sqrt(t[1]*t[1]+t[5]*t[5]+t[9]*t[9]);}
QVector<FunctionalFeature> recognize(const LDrawGeometry::LDrawLoadResult&source,const QStringList&primitives,const FunctionalFeature&prototype,const QString&prefix)
{
    QVector<FunctionalFeature>out;if(!source.sourceModel)return out;
    for(const auto&r:source.sourceModel->references){if(r.fileId<0||r.fileId>=source.sourceModel->files.size())continue;const QString path=source.sourceModel->files[r.fileId].relativePath;bool matched=false;for(const auto&primitive:primitives)matched|=path.compare(QStringLiteral("p/")+primitive,Qt::CaseInsensitive)==0||path.compare(primitive,Qt::CaseInsensitive)==0;if(!matched)continue;auto f=prototype;const auto&t=r.accumulatedTransform;f.frame.origin=convert(QVector3D(float(t[3]),float(t[7]),float(t[11])));f.frame.axis=direction(t,0,1,0);f.frame.profileU=direction(t,1,0,0);f.frame.profileV=cross(f.frame.axis,f.frame.profileU);f.frame.mirrored=r.mirrored;f.nominalAxialExtentMillimetres=axialScale(t);f.nominalEngagementExtentMillimetres=f.nominalAxialExtentMillimetres;QCryptographicHash h(QCryptographicHash::Sha256);h.addData(QString("%1|%2|%3|%4|%5").arg(r.id).arg(r.sourceLine).arg(f.frame.origin.x,0,'g',17).arg(f.frame.origin.y,0,'g',17).arg(f.frame.origin.z,0,'g',17).toUtf8());f.stableIdentity=prefix+QString::fromLatin1(h.result().toHex());f.governingOperandIdentity=f.stableIdentity+QStringLiteral(":operand");f.provenance.push_back({path,r.id,r.sourceLine,r.inverted});out.push_back(f);}return out;
}
}

FunctionalFeature TechnicAxleSemantic::canonicalAxlePrototype(){FunctionalFeature f;f.stableIdentity="official-technic-axle-prototype";f.family=FunctionalInterfaceFamily::TechnicAxle;f.role=FunctionalInterfaceRole::Male;f.materialSide=FunctionalMaterialSide::MaterialInside;f.eligibility=FunctionalEligibility::Eligible;f.confidence=SemanticConfidence::HighConfidence;f.frame={{0,0,0},{0,0,1},{1,0,0},{0,1,0},false};f.nominalRadiusMillimetres=2.4;f.nominalDiameterMillimetres=4.8;f.nominalAxialExtentMillimetres=8.0;f.nominalEngagementExtentMillimetres=8.0;f.protectedCrossArmHalfWidthMillimetres=.8;f.nominalCrossShoulderRadiusMillimetres=2.2408;f.operandAction=FunctionalOperandAction::Unite;f.constructionRecipe="technic-axle-cross-profile-v1";f.evidenceContract="official-ldraw-axle-cross-profile-v1";f.radialProfile={{0,2.4},{8,2.4}};f.governingOperandIdentity=f.stableIdentity+":operand";return f;}
FunctionalFeature TechnicAxleSemantic::canonicalAxleHolePrototype(){auto f=canonicalAxlePrototype();f.stableIdentity="official-technic-axle-hole-prototype";f.family=FunctionalInterfaceFamily::TechnicAxleHole;f.role=FunctionalInterfaceRole::Female;f.materialSide=FunctionalMaterialSide::EmptyInsideMaterialOutside;f.operandAction=FunctionalOperandAction::Subtract;f.constructionRecipe="technic-axle-hole-cross-profile-v1";f.evidenceContract="official-ldraw-axlehole-cross-profile-v1";f.governingOperandIdentity=f.stableIdentity+":operand";return f;}
QVector<FunctionalFeature> TechnicAxleSemantic::recognizeAxles(const LDrawGeometry::LDrawLoadResult&source){return recognize(source,{QStringLiteral("axle.dat"),QStringLiteral("axlehol8.dat")},canonicalAxlePrototype(),QStringLiteral("technic-axle:"));}
QVector<FunctionalFeature> TechnicAxleSemantic::recognizeAxleHoles(const LDrawGeometry::LDrawLoadResult&source){return recognize(source,{QStringLiteral("axlehole.dat"),QStringLiteral("axlehol4.dat")},canonicalAxleHolePrototype(),QStringLiteral("technic-axle-hole:"));}

} // namespace PrintGeometry
