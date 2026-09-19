#pragma once

#include "../../services/geometry/PartMesh.h"
#include "../../services/geometry/PartViewerState.h"

#include <QDialog>
#include <QStringList>
#include <optional>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class LDrawViewportWidget;

struct LDrawModelViewerRequest
{
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
    explicit LDrawModelViewerWindow(QWidget* parent=nullptr);
    void showPart(const LDrawModelViewerRequest& request);
protected:
    void closeEvent(QCloseEvent* event)override;
private:
    enum class LoadBehavior{ResetView,PreserveView};
    void startLoad(LoadBehavior behavior);
    void applyResult(const LDrawGeometry::Result& result,LoadBehavior behavior);
    void updateDimensions();
    void exportObj();
    void saveWindowGeometry();

    LDrawModelViewerRequest m_request;
    LDrawGeometry::PartMesh m_mesh;
    PartViewerState m_state;
    LDrawViewportWidget* m_viewport=nullptr;
    QComboBox* m_candidate=nullptr;
    QComboBox* m_projection=nullptr;
    QComboBox* m_renderMode=nullptr;
    QComboBox* m_standardView=nullptr;
    QComboBox* m_modelColor=nullptr;
    QLabel* m_status=nullptr;
    QLabel* m_part=nullptr;
    QLabel* m_source=nullptr;
    QLabel* m_dimensions=nullptr;
    QLabel* m_counts=nullptr;
    QLabel* m_bfc=nullptr;
    QDoubleSpinBox* m_scale=nullptr;
    QPushButton* m_reload=nullptr;
    QPushButton* m_fit=nullptr;
    QPushButton* m_resetView=nullptr;
    QPushButton* m_export=nullptr;
    QString m_renderingError;
};
