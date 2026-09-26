#pragma once

#include "../../services/geometry/LDrawLoadResult.h"
#include "../../services/geometry/PartViewerState.h"
#include "../../services/geometry/print/PrintPreparation.h"
#include "../../services/geometry/print/PrintMesh.h"
#include "../../services/geometry/print/LocalPrintableOverrideService.h"

#include <QDialog>
#include <QStringList>
#include <optional>
#include <memory>

class QComboBox;
class QCheckBox;
class QColor;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QProgressBar;
class LDrawViewportWidget;
class PrintPreparationCoordinator;
namespace PrintGeometry { struct FitProfile; }

struct LDrawModelViewerRequest
{
    QString externalFilePath;
    int partId=0;
    QString partNumber;
    QString partName;
    QStringList candidates;
    std::optional<int> initialRebrickableColorId;
};

class LDrawModelViewerWindow : public QDialog
{
    Q_OBJECT
public:
    explicit LDrawModelViewerWindow(PrintPreparationCoordinator* coordinator,QWidget* parent=nullptr);
    void showPart(const LDrawModelViewerRequest& request);
protected:
    void closeEvent(QCloseEvent* event)override;
private:
    enum class LoadBehavior{ResetView,PreserveView};
    void startLoad(LoadBehavior behavior);
    void applyResult(const LDrawGeometry::LDrawLoadResult& result,LoadBehavior behavior);
    void invalidatePreparation();
    void startPreparation();
    void applyPreparationProgress(quint64 generation,const PrintGeometry::PrintPreparationProgress& progress);
    void applyPreparationResult(quint64 generation,const PrintGeometry::PrintPreparationResult& result);
    void updatePreparationControls();
    void selectGeometry();
    void updateDimensions();
    void applyPrintRotation(PrintOrientation::Rotation rotation);
    void updatePrintOrientationLabel();
    QColor currentModelColor() const;
    void exportModel();
    PrintGeometry::LocalPrintableOverrideService::Context overrideContext() const;
    void exportRepairSource();
    void importRepairedMesh();
    void acceptNominalOverride();
    void attemptExperimentalFit();
    void removeLocalOverride();
    void applyLocalOverride(const PrintGeometry::LocalPrintableOverrideService::Result&);
    void startManufacturingExport(const PrintGeometry::FitProfile&,bool stl,bool threeMf,
                                  const QString& path,const QString& ldrawId,double scale,const QColor& color);
    void saveWindowGeometry();

    LDrawModelViewerRequest m_request;
    LDrawGeometry::PartMesh m_mesh;
    LDrawGeometry::LDrawLoadResult m_loadResult;
    std::shared_ptr<const PrintGeometry::PreparedMesh>m_preparedMesh;
    PrintGeometry::MeshAnalysisResult m_sourceAnalysis;
    PrintGeometry::PrintMesh m_sourcePrintMesh;
    PartViewerState m_state;
    LDrawViewportWidget* m_viewport=nullptr;
    QComboBox* m_candidate=nullptr;
    QComboBox* m_projection=nullptr;
    QComboBox* m_renderMode=nullptr;
    QComboBox* m_standardView=nullptr;
    QComboBox* m_modelColor=nullptr;
    QComboBox* m_geometryView=nullptr;
    QLabel* m_status=nullptr;
    QLabel* m_part=nullptr;
    QLabel* m_source=nullptr;
    QLabel* m_dimensions=nullptr;
    QLabel* m_counts=nullptr;
    QLabel* m_bfc=nullptr;
    QLabel* m_sourceMeshStatus=nullptr;
    QLabel* m_preparedMeshStatus=nullptr;
    QLabel* m_printOrientationLabel=nullptr;
    QDoubleSpinBox* m_scale=nullptr;
    QPushButton* m_reload=nullptr;
    QPushButton* m_fit=nullptr;
    QPushButton* m_resetView=nullptr;
    QPushButton* m_export=nullptr;
    QPushButton* m_prepare=nullptr;
    QPushButton* m_exportRepair=nullptr;
    QPushButton* m_importRepair=nullptr;
    QPushButton* m_removeOverride=nullptr;
    QPushButton* m_acceptOverride=nullptr;
    QPushButton* m_experimentalFit=nullptr;
    PrintGeometry::LocalPrintableOverrideService::Result m_pendingOverride;
    QString m_pendingOverridePath;
    QProgressBar* m_prepareBusy=nullptr;
    QPushButton* m_manufacturingProof=nullptr;
    QCheckBox* m_showMeshIssues=nullptr;
    PrintPreparationCoordinator* m_coordinator=nullptr;
    quint64 m_sourceGeneration=0;
    bool m_sourceReady=false;
    bool m_preparing=false;
    bool m_prepareBlocked=false;
    bool m_overrideBusy=false;
    QString m_renderingError;
};
