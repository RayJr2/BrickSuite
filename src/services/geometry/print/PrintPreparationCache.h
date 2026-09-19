#pragma once
#include "PrintPreparation.h"
#include <QList>
#include <QMutex>
namespace PrintGeometry {
struct PrintPreparationCacheKey {QString libraryAuthority,partReference,ldrawIdentity,dependencySignature,profileIdentity,mcutVersion;bool operator==(const PrintPreparationCacheKey&o)const{return libraryAuthority==o.libraryAuthority&&partReference==o.partReference&&ldrawIdentity==o.ldrawIdentity&&dependencySignature==o.dependencySignature&&profileIdentity==o.profileIdentity&&mcutVersion==o.mcutVersion;}};
struct PrintPreparationCacheStatistics {qsizetype entries=0;quint64 approximateBytes=0,hits=0,misses=0,evictions=0;};
class PrintPreparationCache {
public:explicit PrintPreparationCache(qsizetype maximumEntries=16,quint64 maximumBytes=64*1024*1024);
    static PrintPreparationCacheKey keyFor(const PrintPreparationRequest&,const QString& mcutVersion);
    std::shared_ptr<const PreparedMesh> find(const PrintPreparationCacheKey&);
    void insert(const PrintPreparationCacheKey&,std::shared_ptr<const PreparedMesh>);
    void clear();PrintPreparationCacheStatistics statistics()const;
private:struct Entry{PrintPreparationCacheKey key;std::shared_ptr<const PreparedMesh> mesh;quint64 bytes=0;};
    static quint64 approximateBytes(const PreparedMesh&);void trim();
    qsizetype m_maximumEntries;quint64 m_maximumBytes=0,m_bytes=0,m_hits=0,m_misses=0,m_evictions=0;QList<Entry>m_entries;mutable QMutex m_mutex;
};
}
