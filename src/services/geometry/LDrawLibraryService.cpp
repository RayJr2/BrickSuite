#include "LDrawLibraryService.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QBuffer>
#include <QRegularExpression>
#include <QSet>
#include <QStringConverter>
#include <cmath>
#include <algorithm>

namespace {
using namespace LDrawGeometry;

constexpr int MaxDepth = 128;
constexpr qint64 MaxFileBytes = 16 * 1024 * 1024;
constexpr qint64 MaxTotalBytes = 64 * 1024 * 1024;
constexpr int MaxFiles = 10000;
constexpr int MaxTriangles = 5000000;
constexpr int MaxLineChars = 65536;

struct Transform {
    double m[3][3]{{1,0,0},{0,1,0},{0,0,1}};
    QVector3D t;
};

QVector3D apply(const Transform& x, const QVector3D& p)
{
    return QVector3D(float(x.m[0][0]*p.x()+x.m[0][1]*p.y()+x.m[0][2]*p.z()+x.t.x()),
                     float(x.m[1][0]*p.x()+x.m[1][1]*p.y()+x.m[1][2]*p.z()+x.t.y()),
                     float(x.m[2][0]*p.x()+x.m[2][1]*p.y()+x.m[2][2]*p.z()+x.t.z()));
}

Transform compose(const Transform& a, const Transform& b)
{
    Transform c;
    for (int r=0;r<3;++r) for (int col=0;col<3;++col) {
        c.m[r][col]=0;
        for (int k=0;k<3;++k) c.m[r][col]+=a.m[r][k]*b.m[k][col];
    }
    c.t=apply(a,b.t);
    return c;
}

double determinant(const Transform& x)
{
    return x.m[0][0]*(x.m[1][1]*x.m[2][2]-x.m[1][2]*x.m[2][1])
        -x.m[0][1]*(x.m[1][0]*x.m[2][2]-x.m[1][2]*x.m[2][0])
        +x.m[0][2]*(x.m[1][0]*x.m[2][1]-x.m[1][1]*x.m[2][0]);
}

QString safeReference(QString value)
{
    value=value.trimmed().replace('\\','/');
    if (value.isEmpty() || value.size()>1024 || value.startsWith('/')
        || value.startsWith("//") || QRegularExpression("^[A-Za-z]:").match(value).hasMatch()) return {};
    const QString clean=QDir::cleanPath(value);
    if (clean==".." || clean.startsWith("../") || clean.contains("/../")) return {};
    return clean;
}

bool parseDouble(const QString& s, double& value)
{
    bool ok=false; value=s.toDouble(&ok); return ok && std::isfinite(value);
}

struct Loader {
    QString root;
    QString canonicalRoot;
    QHash<QString,QString> index;
    QSet<QString> ambiguous;
    QSet<QString> active;
    QHash<QString,QByteArray> embedded;
    QSet<QString> countedSources;
    qint64 totalBytes=0;
    int fileCount=0;
    LDrawLoadResult result;

    int sourceFile(const QString& relative) {
        const QString key=relative.toCaseFolded();
        auto it=result.sourceModel->fileIds.constFind(key);
        if(it!=result.sourceModel->fileIds.cend()) return it.value();
        const int id=result.sourceModel->files.size();
        result.sourceModel->fileIds.insert(key,id);
        result.sourceModel->files.push_back({id,relative,SourceClassification::Unknown});
        const QFileInfo info(QDir(root).filePath(relative));
        result.dependencyFingerprint.dependencies.push_back(
            {relative, info.size(), info.lastModified().toUTC()});
        return id;
    }

    int referenceNode(int parentId,int fileId,int sourceLine,const Transform& transform,bool inverted) {
        ReferenceRecord node; node.id=result.sourceModel->references.size();node.parentId=parentId;
        node.fileId=fileId;node.sourceLine=sourceLine;node.mirrored=determinant(transform)<0.0;node.inverted=inverted;
        node.accumulatedTransform={transform.m[0][0],transform.m[0][1],transform.m[0][2],double(transform.t.x()),
            transform.m[1][0],transform.m[1][1],transform.m[1][2],double(transform.t.y()),
            transform.m[2][0],transform.m[2][1],transform.m[2][2],double(transform.t.z())};
        result.sourceModel->references.push_back(node); return node.id;
    }

    void fail(ErrorCode code, const QString& message, const QString& ref={}, int line=0) {
        if (result.error.code==ErrorCode::None) result.error={code,message,ref,line};
    }

    bool buildIndex() {
        const QStringList roots={"parts","p"};
        for (const QString& sub:roots) {
            QDir base(QDir(root).filePath(sub));
            QDirIterator it(base.absolutePath(), {"*.dat"}, QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                const QString path=it.next();
                const QString canonical=QFileInfo(path).canonicalFilePath();
                if (canonical.isEmpty() || !(canonical==canonicalRoot || canonical.startsWith(canonicalRoot+'/'))) {
                    fail(ErrorCode::TraversalRejected,"An LDraw library entry resolves outside the selected library.",path); return false;
                }
                QString rel=QDir(root).relativeFilePath(path).replace('\\','/');
                const QString key=rel.toCaseFolded();
                if (index.contains(key) && index.value(key)!=rel) ambiguous.insert(key); else index.insert(key,rel);
            }
        }
        return true;
    }

    QString resolve(const QString& reference, bool rootModel) {
        const QString safe=safeReference(reference);
        if (safe.isEmpty()) { fail(ErrorCode::TraversalRejected,"Unsafe LDraw reference was rejected.",reference); return {}; }
        if (!rootModel && embedded.contains(safe.toCaseFolded()))
            return QStringLiteral("@mpd/") + safe.toCaseFolded();
        QStringList candidates;
        if (rootModel) candidates << "parts/"+safe;
        else if (safe.startsWith("s/",Qt::CaseInsensitive)) candidates << "parts/"+safe;
        else if (safe.startsWith("8/",Qt::CaseInsensitive)||safe.startsWith("48/",Qt::CaseInsensitive)) candidates << "p/"+safe;
        else candidates << "parts/"+safe << "p/"+safe;
        for (QString c:candidates) {
            c=QDir::cleanPath(c).replace('\\','/'); const QString key=c.toCaseFolded();
            if (ambiguous.contains(key)) { fail(ErrorCode::InvalidLibrary,"The library contains ambiguous file names that differ only by case.",reference); return {}; }
            if (index.contains(key)) return index.value(key);
        }
        fail(rootModel?ErrorCode::ModelNotFound:ErrorCode::DependencyMissing,
             rootModel?"The selected LDraw model was not found.":"An LDraw dependency is missing.",reference);
        return {};
    }

    void bounds(const QVector3D& p) {
        if (!result.mesh.hasBounds) { result.mesh.minimumBounds=result.mesh.maximumBounds=p; result.mesh.hasBounds=true; return; }
        auto& lo=result.mesh.minimumBounds; auto& hi=result.mesh.maximumBounds;
        lo.setX(qMin(lo.x(),p.x())); lo.setY(qMin(lo.y(),p.y())); lo.setZ(qMin(lo.z(),p.z()));
        hi.setX(qMax(hi.x(),p.x())); hi.setY(qMax(hi.y(),p.y())); hi.setZ(qMax(hi.z(),p.z()));
    }

    QString inheritedColor(const QString& value,const QString& parent) const { return value=="16"?parent:value; }

    bool load(const QString& relative,const Transform& transform,const QString& parentColor,bool inverted,int depth,
              int parentNode=-1,int referenceLine=0) {
        if (depth>MaxDepth) { fail(ErrorCode::ResourceLimitExceeded,"LDraw reference depth exceeded the safety limit.",relative); return false; }
        const QString key=relative.toCaseFolded();
        if (active.contains(key)) { fail(ErrorCode::CycleDetected,"A cyclic LDraw reference was detected.",relative); return false; }
        QByteArray source;
        if (relative.startsWith("@mpd/")) {
            source=embedded.value(relative.mid(5));
        } else {
            const QString path=QDir(root).filePath(relative); QFileInfo info(path);
            if (info.size()>MaxFileBytes || ++fileCount>MaxFiles || (totalBytes+=info.size())>MaxTotalBytes) {
                fail(ErrorCode::ResourceLimitExceeded,"LDraw input exceeded a safety limit.",relative); return false;
            }
            QFile sourceFile(path); if (!sourceFile.open(QIODevice::ReadOnly)) { fail(ErrorCode::DependencyMissing,"The LDraw source could not be opened.",relative); return false; }
            source=sourceFile.readAll();
            // MPD container sections are local named subfiles. The first FILE
            // section is the entry point; subsequent sections are resolved
            // before the installed library without touching the filesystem.
            QList<QByteArray> entryLines; QHash<QString,QByteArray> sections;
            QString current; bool sawFile=false;
            for (const QByteArray& raw : source.split('\n')) {
                const QString text=QString::fromUtf8(raw).trimmed();
                if (text.startsWith("0 FILE ",Qt::CaseInsensitive)) {
                    current=safeReference(text.mid(7)); sawFile=true;
                    if(current.isEmpty()) { fail(ErrorCode::TraversalRejected,"Unsafe embedded MPD name was rejected.",text.mid(7)); return false; }
                    continue;
                }
                if (text.compare("0 NOFILE",Qt::CaseInsensitive)==0) { current.clear(); continue; }
                if (sawFile && !current.isEmpty()) sections[current.toCaseFolded()].append(raw+'\n');
            }
            if (sawFile && !sections.isEmpty()) {
                // Locate the first FILE explicitly (comments may precede it).
                QString entry;
                for(const QByteArray& raw:source.split('\n')) { const QString text=QString::fromUtf8(raw).trimmed(); if(text.startsWith("0 FILE ",Qt::CaseInsensitive)){entry=safeReference(text.mid(7)).toCaseFolded();break;} }
                for(auto it=sections.cbegin();it!=sections.cend();++it) {
                    if(embedded.contains(it.key()) && embedded.value(it.key())!=it.value()) { fail(ErrorCode::MalformedSource,"Duplicate embedded MPD file name.",it.key()); return false; }
                    embedded.insert(it.key(),it.value());
                }
                source=sections.value(entry);
            }
        }
        QBuffer file(&source); file.open(QIODevice::ReadOnly|QIODevice::Text);
        const int fileId=sourceFile(relative);
        const int currentNode=referenceNode(parentNode,fileId,referenceLine,transform,inverted);
        active.insert(key);
        if(!countedSources.contains(key)){countedSources.insert(key);++result.mesh.sourceFiles;}
        bool clockwise=false, certified=false, clip=true, invertNext=false;
        int lineNo=0;
        while (!file.atEnd() && result.ok()) {
            QByteArray raw=file.readLine(); ++lineNo;
            if (raw.size()>MaxLineChars) { fail(ErrorCode::ResourceLimitExceeded,"An LDraw line exceeded the safety limit.",relative,lineNo); break; }
            QString line=QString::fromUtf8(raw).trimmed(); if (line.isEmpty()) continue;
            QStringList v=line.split(QRegularExpression("\\s+")); bool typeOk=false; int type=v.value(0).toInt(&typeOk);
            if (!typeOk || type<0 || type>5) { fail(ErrorCode::MalformedSource,"Malformed LDraw line type.",relative,lineNo); break; }
            if (type==0) {
                if (lineNo==1) result.sourceModel->files[fileId].description=line.mid(2).trimmed();
                const QString upper=line.toUpper();
                if(upper.startsWith("0 !LDRAW_ORG ")) {
                    const QString kind=v.value(2).toUpper();
                    auto& classification=result.sourceModel->files[fileId].classification;
                    if(kind=="PART") classification=SourceClassification::Part;
                    else if(kind=="SUBPART") classification=SourceClassification::Subpart;
                    else if(kind=="PRIMITIVE"||kind=="48_PRIMITIVE"||kind=="8_PRIMITIVE") classification=SourceClassification::Primitive;
                }
                if (upper.contains(" BFC ") || upper.startsWith("0 BFC ")) {
                    if (upper.contains("NOCERTIFY")) certified=false;
                    if (upper.contains("CERTIFY" )&&!upper.contains("NOCERTIFY")) certified=true;
                    if (upper.contains("NOCLIP")) clip=false;
                    if (upper.contains(" CLIP")&&!upper.contains("NOCLIP")) clip=true;
                    if (upper.contains(" CCW")) clockwise=false;
                    if (upper.contains(" CW")&&!upper.contains("CCW")) clockwise=true;
                    if (upper.contains("INVERTNEXT")) invertNext=true;
                }
                continue;
            }
            const int need=type==1?15:type==2?8:type==3?11:type==4?14:14;
            if (v.size()<need) { fail(ErrorCode::MalformedSource,"Malformed LDraw geometry record.",relative,lineNo); break; }
            const QString color=inheritedColor(v[1],parentColor);
            auto point=[&](int start,QVector3D& p){ double x,y,z; if(!parseDouble(v[start],x)||!parseDouble(v[start+1],y)||!parseDouble(v[start+2],z)) return false; p=apply(transform,QVector3D(float(x),float(y),float(z))); return true; };
            if (type==1) {
                double n[12]; bool ok=true; for(int i=0;i<12;++i) ok=parseDouble(v[2+i],n[i])&&ok;
                if(!ok) { fail(ErrorCode::MalformedSource,"Invalid numeric value in LDraw reference.",relative,lineNo); break; }
                Transform local; local.t=QVector3D(float(n[0]),float(n[1]),float(n[2]));
                int z=3; for(int r=0;r<3;++r) for(int c=0;c<3;++c) local.m[r][c]=n[z++];
                QString child=resolve(v.mid(14).join(' '),false); if(child.isEmpty()) break;
                const bool childInverted=inverted ^ invertNext ^ (determinant(local)<0.0); invertNext=false;
                if(!load(child,compose(transform,local),color,childInverted,depth+1,currentNode,lineNo)) break;
                continue;
            }
            QVector<QVector3D> points; int count=type==2?2:type==3?3:4;
            for(int i=0;i<count;++i) { QVector3D p; if(!point(2+i*3,p)) { fail(ErrorCode::MalformedSource,"Invalid numeric value in LDraw geometry.",relative,lineNo); break; } points.push_back(p); }
            if(!result.ok()) break;
            if(type==2) { result.mesh.hardEdges.push_back({points[0],points[1],color}); continue; }
            if(type==5) { result.mesh.conditionalEdges.push_back({points[0],points[1],points[2],points[3],color}); continue; }
            if(clockwise ^ inverted) std::reverse(points.begin()+1,points.end());
            auto addTriangle=[&](const QVector3D&a,const QVector3D&b,const QVector3D&c){
                QVector3D normal=QVector3D::crossProduct(b-a,c-a); if(normal.lengthSquared()<1e-12f) { ++result.mesh.degenerateFaces; return; }
                normal.normalize(); result.mesh.triangles.push_back({a,b,c,normal,color,certified&&clip}); bounds(a); bounds(b); bounds(c);
                result.sourceModel->surfaces.push_back({int(result.mesh.triangles.size()-1),currentNode,fileId,lineNo,type,certified,clip,inverted});
                if(result.mesh.triangles.size()>MaxTriangles) fail(ErrorCode::ResourceLimitExceeded,"Triangle count exceeded the safety limit.",relative,lineNo);
            };
            addTriangle(points[0],points[1],points[2]); if(type==4 && result.ok()) addTriangle(points[0],points[2],points[3]);
            result.mesh.bfcCertified=result.mesh.bfcCertified||certified;
        }
        active.remove(key); return result.ok();
    }
};
}

LDrawGeometry::LibraryValidation LDrawLibraryService::validateLibrary(const QString& root)
{
    LibraryValidation value; QDir dir(root.trimmed());
    if (root.trimmed().isEmpty()) { value.status="No LDraw library is configured."; return value; }
    if (!dir.exists() || !QDir(dir.filePath("parts")).exists() || !QDir(dir.filePath("p")).exists()) {
        value.status="The selected folder is not an LDraw library (parts and p are required)."; return value;
    }
    QDirIterator it(dir.filePath("parts"),{"*.dat"},QDir::Files,QDirIterator::Subdirectories);
    if (!it.hasNext()) { value.status="The selected LDraw library does not contain recognizable Part files."; return value; }
    value.normalizedRoot=QFileInfo(dir.absolutePath()).canonicalFilePath(); value.valid=!value.normalizedRoot.isEmpty();
    value.status=value.valid?"Valid LDraw library.":"The selected LDraw library path could not be resolved."; return value;
}

LDrawGeometry::LDrawLoadResult LDrawLibraryService::loadPart(const QString& root, const QString& ldrawId)
{
    const auto validation=validateLibrary(root); Loader loader; loader.result.mesh.ldrawId=ldrawId.trimmed();
    loader.result.sourceModel=std::make_shared<LDrawSourceModel>();
    if(!validation.valid) { loader.fail(root.trimmed().isEmpty()?ErrorCode::LibraryNotConfigured:ErrorCode::InvalidLibrary,validation.status); return loader.result; }
    loader.root=validation.normalizedRoot;
    loader.canonicalRoot=validation.normalizedRoot;
    loader.canonicalRoot.replace('\\','/');
    if(!loader.buildIndex()) return loader.result;
    QString id=safeReference(ldrawId); if(id.isEmpty()) { loader.fail(ErrorCode::TraversalRejected,"Unsafe LDraw identity was rejected.",ldrawId); return loader.result; }
    if(!id.endsWith(".dat",Qt::CaseInsensitive)) id += ".dat";
    const QString rel=loader.resolve(id,true); if(rel.isEmpty()) return loader.result;
    loader.result.mesh.sourceRelativePath=rel; loader.result.mesh.sourceProvenance="Installed LDraw library";
    loader.load(rel,Transform{},"16",false,0); return loader.result;
}
