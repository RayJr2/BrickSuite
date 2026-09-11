/* BrickSuite - The Digital Twin Platform for Your Brick Workshop */
#pragma once
#include <QDialog>
#include <QList>
#include "../../models/PartReferenceEntry.h"
#include "../../services/parts/PartReferenceManifest.h"

class QComboBox; class QLabel; class QLineEdit; class QListWidget; class QPushButton;
class SharedPartReferenceCustomizationService;
class RemotePartReferenceMutationApplicationService;
class RemoteSessionState;

class AddPartReferenceDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AddPartReferenceDialog(SharedPartReferenceCustomizationService& customizationService,
                                    int initialPartId = 0,
                                    const PartReferenceEntry* anchor = nullptr,
                                    RemotePartReferenceMutationApplicationService* remoteMutations = nullptr,
                                    RemoteSessionState* remoteSession = nullptr,
                                    QWidget* parent = nullptr);
    bool customizationAdded() const { return m_added; }
private:
    void searchParts();
    void destinationChanged();
    void save();
    int selectedPartId() const;
    PartReferenceManifest m_manifest;
    QList<PartReferenceEntry> m_effective;
    QLineEdit* m_search = nullptr;
    QListWidget* m_results = nullptr;
    QComboBox* m_destination = nullptr;
    QComboBox* m_placement = nullptr;
    QComboBox* m_anchor = nullptr;
    QLabel* m_note = nullptr;
    QPushButton* m_save = nullptr;
    int m_initialPartId = 0;
    QString m_defaultCatalog;
    QString m_defaultSection;
    QString m_defaultAnchor;
    bool m_added = false;
    bool m_pending = false;
    QString m_mutationId;
    SharedPartReferenceCustomizationService& m_customizationService;
    RemotePartReferenceMutationApplicationService* m_remoteMutations = nullptr;
    RemoteSessionState* m_remoteSession = nullptr;
    quint64 m_sessionGeneration = 0;
    quint64 m_workspaceGeneration = 0;
};
