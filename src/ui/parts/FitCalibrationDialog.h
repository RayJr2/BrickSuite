#pragma once
#include "../../services/geometry/fit/FitCalibrationExperiment.h"
#include "../../services/geometry/fit/FitCalibrationLibrary.h"
#include "FitCalibrationFamilyCatalog.h"
#include <QDialog>
class QComboBox; class QDoubleSpinBox; class QLabel; class QLineEdit; class QPushButton; class QSpinBox; class QTabWidget; class QTableWidget; class QTimer;
class FitCalibrationDialog : public QDialog {
    Q_OBJECT
public: explicit FitCalibrationDialog(QWidget* parent=nullptr);
private:
    PrintGeometry::FitCalibrationExperiment* activeExperiment();
    QTableWidget* activeTable() const;
    bool openSession(const QString&); bool saveSession(); bool writeSession(const QString&);
    void loadSession(); void resumeManagedSession(); void exportSession(); void createFitProfile(); void createSelectedCalibration(); void createCalibration(FitCalibrationFamily); void newTechnicCalibration(); void newStudCalibration(); void newReceivingClutchCalibration(); void newFrictionlessPinCalibration(); void newFrictionPinCalibration(); void newTechnicAxleCalibration(); void newTechnicAxleHoleCalibration(); void refreshLibrary(); void scheduleManagedSave();
    void addObservation(); void selectPreferred(); void markVerified(); void generateFineSearch();
    void readProcess(); void showSession(const PrintGeometry::FitCalibrationSession&,const QString&); void refresh(); void refreshTable(QTableWidget*,const PrintGeometry::FitCalibrationExperiment*); void updateGuidance();
    void showWorkspace(const PrintGeometry::FitCalibrationWorkspace&, const QString& preferredSession = {}); void selectFeature(int);
    PrintGeometry::FitCalibrationSession m_session; QString m_path;
    PrintGeometry::FitCalibrationWorkspace m_workspace; int m_featureIndex=-1;
    QVector<PrintGeometry::FitCalibrationExperiment> m_coarseHistory;
    PrintGeometry::FitCalibrationLibrary m_library;
    bool m_loading=false, m_dirty=false, m_savedConfirmation=false;
    QLineEdit *m_printer=nullptr,*m_material=nullptr,*m_profile=nullptr,*m_orientationNotes=nullptr,*m_compensationNotes=nullptr,*m_notes=nullptr;
    QDoubleSpinBox *m_nozzle=nullptr,*m_layer=nullptr,*m_measured=nullptr; QComboBox *m_orientation=nullptr,*m_result=nullptr,*m_sessions=nullptr,*m_features=nullptr,*m_newFamily=nullptr,*m_coarseReview=nullptr; QSpinBox* m_repeat=nullptr; QTimer* m_autoSave=nullptr;
    QTabWidget* m_tabs=nullptr; QTableWidget *m_coarseTable=nullptr,*m_fineTable=nullptr; QLabel *m_identity=nullptr,*m_coarseContext=nullptr,*m_fineContext=nullptr,*m_guidance=nullptr; QPushButton *m_save=nullptr,*m_generate=nullptr,*m_verify=nullptr;
};
