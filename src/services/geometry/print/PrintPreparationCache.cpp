#include "PrintPreparationCache.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QMutexLocker>
#include <algorithm>
namespace PrintGeometry {
PrintPreparationCache::PrintPreparationCache(qsizetype entries,quint64 bytes):m_maximumEntries(std::max<qsizetype>(1,entries)),m_maximumBytes(std::max<quint64>(1,bytes)){}
PrintPreparationCacheKey PrintPreparationCache::keyFor(const PrintPreparationRequest&r,const QString&mcutVersion)
{
    QString authority=QFileInfo(r.libraryAuthority).canonicalFilePath();
    if(authority.isEmpty())authority=QDir::cleanPath(QFileInfo(r.libraryAuthority).absoluteFilePath());
    authority=QDir::fromNativeSeparators(authority);
#ifdef Q_OS_WIN
    authority=authority.toLower();
#endif
    auto dependencies=r.loadResult.dependencyFingerprint.dependencies;
    std::sort(dependencies.begin(),dependencies.end(),[](const auto&a,const auto&b){return a.relativePath<b.relativePath;});
    QCryptographicHash hash(QCryptographicHash::Sha256);
    const QByteArray separator(1,'\0'),terminator(1,'\n');
    for(const auto&d:dependencies){hash.addData(d.relativePath.toUtf8());hash.addData(separator);hash.addData(QByteArray::number(d.size));hash.addData(separator);hash.addData(d.modifiedUtc.toUTC().toString(Qt::ISODateWithMs).toUtf8());hash.addData(terminator);}
    if(!r.loadResult.externalFilePath.isEmpty()){hash.addData(r.loadResult.externalFilePath.toUtf8());hash.addData(r.loadResult.externalContentHash);}
    return{authority,r.partReference,r.ldrawIdentity,QString::fromLatin1(hash.result().toHex()),r.profile.identity,mcutVersion};
}
quint64 PrintPreparationCache::approximateBytes(const PreparedMesh&m){quint64 n=sizeof(PreparedMesh)+m.mesh.vertices.capacity()*sizeof(Point)+m.mesh.faces.capacity()*sizeof(Face)+m.finalAnalysis.issues.capacity()*sizeof(MeshIssue)+m.functionalFeatures.capacity()*sizeof(FunctionalFeature);for(const auto&s:m.operationSummary)n+=quint64(s.capacity()+1)*2;for(const auto&s:m.warnings)n+=quint64(s.capacity()+1)*2;for(const auto&feature:m.functionalFeatures){n+=quint64(feature.stableIdentity.capacity()+feature.governingOperandIdentity.capacity()+feature.constructionRecipe.capacity()+feature.evidenceContract.capacity())*2;n+=feature.provenance.capacity()*sizeof(FunctionalFeatureProvenance)+feature.radialProfile.capacity()*sizeof(FunctionalRadialSection);for(const auto&p:feature.provenance)n+=quint64(p.sourceFile.capacity())*2;}return n;}
std::shared_ptr<const PreparedMesh> PrintPreparationCache::find(const PrintPreparationCacheKey&key){QMutexLocker lock(&m_mutex);for(qsizetype i=0;i<m_entries.size();++i)if(m_entries[i].key==key){auto entry=m_entries.takeAt(i);m_entries.prepend(entry);++m_hits;return entry.mesh;}++m_misses;return{};}
void PrintPreparationCache::insert(const PrintPreparationCacheKey&key,std::shared_ptr<const PreparedMesh>mesh){if(!mesh)return;QMutexLocker lock(&m_mutex);for(qsizetype i=0;i<m_entries.size();++i)if(m_entries[i].key==key){m_bytes-=m_entries[i].bytes;m_entries.removeAt(i);break;}Entry entry{key,std::move(mesh),0};entry.bytes=approximateBytes(*entry.mesh);m_bytes+=entry.bytes;m_entries.prepend(std::move(entry));trim();}
void PrintPreparationCache::trim(){while(!m_entries.isEmpty()&&(m_entries.size()>m_maximumEntries||m_bytes>m_maximumBytes)){m_bytes-=m_entries.back().bytes;m_entries.removeLast();++m_evictions;}}
void PrintPreparationCache::clear(){QMutexLocker lock(&m_mutex);m_entries.clear();m_bytes=0;}
PrintPreparationCacheStatistics PrintPreparationCache::statistics()const{QMutexLocker lock(&m_mutex);return{m_entries.size(),m_bytes,m_hits,m_misses,m_evictions};}
}
