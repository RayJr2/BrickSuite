#pragma once

#include "PreparedMesh.h"
#include <memory>
#include <optional>

namespace PrintGeometry {

// No catalog writes or fit ownership. All geometry is nominal Z-up millimeters.
class LocalPrintableOverrideService
{
public:
    struct Context {
        int partId = 0;
        QString partNumber;
        LDrawGeometry::LDrawLoadResult source;
    };
    struct Result {
        std::shared_ptr<const PreparedMesh> prepared;
        QString diagnostic;
        bool stale = false;
        bool reviewable = false;
        QByteArray reviewedSourceFingerprint,reviewedMeshFingerprint;
        std::shared_ptr<const PreparedMesh> reviewCandidate;
        bool ok() const { return bool(prepared); }
    };
    struct NormalizationResult {
        PrintMesh mesh;
        std::vector<MeshAnalysisResult> components;
        QString diagnostic;
        bool accepted = false;
        bool normalized = false;
        std::size_t internalIntersectionPairs = 0, crossComponentIntersectionPairs = 0;
        qint64 elapsedMilliseconds = 0;
    };
    static NormalizationResult normalizeClosedComponents(const PrintMesh&);
    // Internal child-process entry point, before GUI/database initialization.
    static std::optional<int> runUnionWorker(int argc,char** argv);
    explicit LocalPrintableOverrideService(QString storageRoot = {});
    static PrintMesh repairSource(const Context&);
    static QByteArray sourceFingerprint(const Context&);
    static bool exportRepairSource(const Context&, const QString& path, QString* error);
    static bool readThreeMf(const QString& path, PrintMesh* mesh, QString* error);
    static bool readStl(const QString& path, PrintMesh* mesh, QString* error);
    Result importRepaired(const Context&, const QString& path, bool explicitlyConfirmed=false,
                          const QByteArray& reviewedSource={},const QByteArray& reviewedMesh={}) const;
    Result load(const Context&) const;
    bool remove(const Context&, QString* error) const;
    QString storagePath(const Context&) const;
private:
    static Result validate(const Context&, PrintMesh, bool allowReview=false);
    QString m_root;
};

} // namespace PrintGeometry
