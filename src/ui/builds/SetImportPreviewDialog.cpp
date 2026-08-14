#include "SetImportPreviewDialog.h"

#include "../../models/BuildRequirement.h"
#include "../../models/Color.h"
#include "../../models/Part.h"

#include "../../repositories/BuildRequirementRepository.h"
#include "../../repositories/ColorRepository.h"
#include "../../repositories/PartRepository.h"

#include "../../database/DatabaseManager.h"

#include "../../settings/UserSettings.h"

#include <QAbstractItemView>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSqlDatabase>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

SetImportPreviewDialog::SetImportPreviewDialog(int buildId,
                                               const QString& setNumber,
                                               QWidget* parent)
    : QDialog(parent)
    , m_buildId(buildId)
    , m_setNumber(setNumber.trimmed())
{
    setWindowTitle("Set Import Preview");

    resize(1100, 700);

    auto* mainLayout = new QVBoxLayout(this);

    m_titleLabel = new QLabel(QString("Rebrickable Set Import Preview — %1").arg(m_setNumber), this);

    QFont titleFont = m_titleLabel->font();

    titleFont.setBold(true);

    m_titleLabel->setFont(titleFont);

    mainLayout->addWidget(m_titleLabel);

    m_summaryLabel = new QLabel("Loading Set Details and Set Parts...", this);

    m_summaryLabel->setWordWrap(true);

    mainLayout->addWidget(m_summaryLabel);

    m_statusLabel = new QLabel(this);

    m_statusLabel->setWordWrap(true);

    mainLayout->addWidget(m_statusLabel);

    m_previewTable = new QTableWidget(this);

    m_previewTable->setColumnCount(8);

    m_previewTable->setHorizontalHeaderLabels(QStringList() << "Part #"
                                                            << "Name"
                                                            << "Rebrickable Color"
                                                            << "Qty"
                                                            << "Spare"
                                                            << "Part Match"
                                                            << "Color Match"
                                                            << "Import Status");

    m_previewTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    m_previewTable->setSelectionBehavior(QAbstractItemView::SelectRows);

    m_previewTable->setSelectionMode(QAbstractItemView::SingleSelection);

    m_previewTable->verticalHeader()->setVisible(false);

    m_previewTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);

    m_previewTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);

    m_previewTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);

    for (int column = 3; column <= 7; ++column) {
        m_previewTable->horizontalHeader()->setSectionResizeMode(column,
                                                                 QHeaderView::ResizeToContents);
    }

    mainLayout->addWidget(m_previewTable, 1);

    auto* buttonLayout = new QHBoxLayout();

    buttonLayout->addStretch();

    m_importButton = new QPushButton("Import Requirements", this);

    m_importButton->setEnabled(false);

    m_closeButton = new QPushButton("Close", this);

    buttonLayout->addWidget(m_importButton);

    buttonLayout->addWidget(m_closeButton);

    mainLayout->addLayout(buttonLayout);

    connect(m_importButton,
            &QPushButton::clicked,
            this,
            &SetImportPreviewDialog::importRequirements);

    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::reject);

    m_apiClient = new RebrickableApiClient(this);

    connect(m_apiClient,
            &RebrickableApiClient::setDetailsFinished,
            this,
            &SetImportPreviewDialog::handleSetDetails);

    connect(m_apiClient,
            &RebrickableApiClient::setPartsFinished,
            this,
            &SetImportPreviewDialog::handleSetParts);

    loadFromRebrickable();
}

void SetImportPreviewDialog::loadFromRebrickable()
{
    if (m_buildId <= 0 || m_setNumber.isEmpty()) {
        m_statusLabel->setText("A valid Build and Set Number are required.");

        return;
    }

    const QString apiKey = UserSettings::instance().rebrickableApiKey();

    if (apiKey.isEmpty()) {
        m_statusLabel->setText("No Rebrickable API key is configured.");

        return;
    }

    if (RebrickableApiClient::isSessionBlocked()) {
        m_statusLabel->setText(RebrickableApiClient::sessionBlockReason());

        return;
    }

    setLoadingState(true);

    //
    // Deliberately queue both calls immediately.
    //
    // RebrickableApiClient's shared request scheduler
    // will enforce the configured minimum interval.
    //
    m_apiClient->getSetDetails(m_setNumber, apiKey);

    m_apiClient->getSetParts(m_setNumber, apiKey);
}

void SetImportPreviewDialog::handleSetDetails(const RebrickableApiClient::SetDetailsResult& result)
{
    m_setDetailsReceived = true;

    m_setDetailsSucceeded = result.success;

    if (result.success) {
        m_setDetails = result.set;
    } else {
        m_statusLabel->setText(QString("Unable to load Set Details.\n\n%1").arg(result.message));
    }

    tryPopulatePreview();
}

void SetImportPreviewDialog::handleSetParts(const RebrickableApiClient::SetPartsResult& result)
{
    m_setPartsReceived = true;

    m_setPartsSucceeded = result.success;

    if (result.success) {
        m_setPartsResult = result;
    } else {
        m_statusLabel->setText(QString("Unable to load Set Parts.\n\n%1").arg(result.message));
    }

    tryPopulatePreview();
}

void SetImportPreviewDialog::tryPopulatePreview()
{
    //
    // Responses can complete in either order.
    //
    if (!m_setDetailsReceived || !m_setPartsReceived) {
        return;
    }

    setLoadingState(false);

    if (!m_setDetailsSucceeded || !m_setPartsSucceeded) {
        return;
    }

    populatePreview();
}

void SetImportPreviewDialog::populatePreview()
{
    m_previewTable->setRowCount(0);

    m_previewRows.clear();

    m_newCount = 0;
    m_quantityChangedCount = 0;
    m_noChangeCount = 0;
    m_problemCount = 0;

    PartRepository partRepository;

    ColorRepository colorRepository;

    BuildRequirementRepository requirementRepository;

    const QList<BuildRequirement> existingRequirements = requirementRepository.getByBuild(m_buildId);

    int regularRows = 0;
    int spareRows = 0;

    int regularQuantity = 0;
    int spareQuantity = 0;

    int row = 0;

    for (const RebrickableApiClient::SetPart& setPart : m_setPartsResult.parts) {
        PreviewRow previewRow;

        previewRow.setPart = setPart;

        const std::optional<Part> part = partRepository.getByPartNumber(setPart.partNumber);

        const std::optional<Color> color = colorRepository.getByRebrickableId(
            setPart.rebrickableColorId);

        const bool partMatched = part.has_value();

        const bool colorMatched = color.has_value();

        if (part) {
            previewRow.partId = part->id();
        }

        if (color) {
            previewRow.colorId = color->id();
        }

        //
        // Resolve the existing Build Requirement by
        // our unique logical identity:
        //
        // Build + Part + Color + Spare
        //
        if (partMatched && colorMatched) {
            for (const BuildRequirement& existing : existingRequirements) {
                if (existing.partId() == previewRow.partId
                    && existing.colorId() == previewRow.colorId
                    && existing.isSpare() == setPart.isSpare) {
                    previewRow.existingRequirementId = existing.id();

                    previewRow.existingQuantity = existing.quantityRequired();

                    break;
                }
            }

            if (previewRow.existingRequirementId <= 0) {
                previewRow.status = ImportStatus::New;

                ++m_newCount;
            } else if (previewRow.existingQuantity == setPart.quantity) {
                previewRow.status = ImportStatus::NoChange;

                ++m_noChangeCount;
            } else {
                previewRow.status = ImportStatus::QuantityChanged;

                ++m_quantityChangedCount;
            }
        } else {
            previewRow.status = ImportStatus::MappingProblem;

            ++m_problemCount;
        }

        if (setPart.isSpare) {
            ++spareRows;

            spareQuantity += setPart.quantity;
        } else {
            ++regularRows;

            regularQuantity += setPart.quantity;
        }

        m_previewRows.append(previewRow);

        m_previewTable->insertRow(row);

        auto* partNumberItem = new QTableWidgetItem(setPart.partNumber);

        auto* nameItem = new QTableWidgetItem(setPart.partName);

        auto* colorItem = new QTableWidgetItem(
            QString("%1 (%2)").arg(setPart.colorName).arg(setPart.rebrickableColorId));

        auto* quantityItem = new QTableWidgetItem(QString::number(setPart.quantity));

        auto* spareItem = new QTableWidgetItem(setPart.isSpare ? "Yes" : "No");

        auto* partMatchItem = new QTableWidgetItem(partMatched ? "Matched" : "Missing");

        auto* colorMatchItem = new QTableWidgetItem(colorMatched ? "Matched" : "Missing");

        auto* statusItem = new QTableWidgetItem(importStatusText(previewRow.status));

        quantityItem->setTextAlignment(Qt::AlignCenter);

        spareItem->setTextAlignment(Qt::AlignCenter);

        partMatchItem->setTextAlignment(Qt::AlignCenter);

        colorMatchItem->setTextAlignment(Qt::AlignCenter);

        statusItem->setTextAlignment(Qt::AlignCenter);

        if (part) {
            partNumberItem->setData(Qt::UserRole, part->id());
        }

        if (color) {
            colorItem->setData(Qt::UserRole, color->id());
        }

        m_previewTable->setItem(row, 0, partNumberItem);

        m_previewTable->setItem(row, 1, nameItem);

        m_previewTable->setItem(row, 2, colorItem);

        m_previewTable->setItem(row, 3, quantityItem);

        m_previewTable->setItem(row, 4, spareItem);

        m_previewTable->setItem(row, 5, partMatchItem);

        m_previewTable->setItem(row, 6, colorMatchItem);

        m_previewTable->setItem(row, 7, statusItem);

        ++row;
    }

    QString paginationText = "Complete";

    if (!m_setPartsResult.nextUrl.isEmpty()) {
        paginationText = "Additional API pages exist";
    }

    m_summaryLabel->setText(QString("<b>%1</b><br>"
                                    "Set: %2<br>"
                                    "Year: %3<br>"
                                    "Theme ID: %4<br>"
                                    "Set Details num_parts: %5<br><br>"
                                    "API Rows: %6<br>"
                                    "Rows Returned: %7<br>"
                                    "Regular Rows: %8<br>"
                                    "Regular Pieces: %9<br>"
                                    "Spare Rows: %10<br>"
                                    "Spare Pieces: %11<br><br>"
                                    "<b>Import Comparison</b><br>"
                                    "New: %12<br>"
                                    "Quantity Changed: %13<br>"
                                    "No Change: %14<br>"
                                    "Problems: %15<br>"
                                    "Pagination: %16")
                                .arg(m_setDetails.name)
                                .arg(m_setDetails.setNumber)
                                .arg(m_setDetails.year)
                                .arg(m_setDetails.themeId)
                                .arg(m_setDetails.numberOfParts)
                                .arg(m_setPartsResult.totalCount)
                                .arg(m_setPartsResult.parts.size())
                                .arg(regularRows)
                                .arg(regularQuantity)
                                .arg(spareRows)
                                .arg(spareQuantity)
                                .arg(m_newCount)
                                .arg(m_quantityChangedCount)
                                .arg(m_noChangeCount)
                                .arg(m_problemCount)
                                .arg(paginationText));

    if (!m_setPartsResult.nextUrl.isEmpty()) {
        m_statusLabel->setText("Import is disabled because the Set Parts "
                               "response has additional pages.");
    } else if (m_problemCount > 0) {
        m_statusLabel->setText(QString("%1 row(s) have mapping problems. "
                                       "Resolve them before importing.")
                                   .arg(m_problemCount));
    } else {
        m_statusLabel->setText("All Set Part rows are ready for import. "
                               "No database changes have been made yet.");
    }

    updateImportButtonState();
}

void SetImportPreviewDialog::setLoadingState(bool loading)
{
    m_previewTable->setEnabled(!loading);

    if (loading) {
        m_statusLabel->setText(QString("Loading %1 from Rebrickable...").arg(m_setNumber));
    }
}

QString SetImportPreviewDialog::importStatusText(ImportStatus status) const
{
    switch (status) {
    case ImportStatus::New:
        return "New";

    case ImportStatus::NoChange:
        return "No Change";

    case ImportStatus::QuantityChanged:
        return "Quantity Changed";

    case ImportStatus::MappingProblem:
    default:
        return "Mapping Problem";
    }
}

void SetImportPreviewDialog::updateImportButtonState()
{
    const bool canImport = m_buildId > 0 && m_setDetailsSucceeded && m_setPartsSucceeded
                           && m_problemCount == 0 && m_setPartsResult.nextUrl.isEmpty();

    m_importButton->setEnabled(canImport);
}

void SetImportPreviewDialog::importRequirements()
{
    if (m_problemCount > 0 || !m_setPartsResult.nextUrl.isEmpty()) {
        QMessageBox::warning(this,
                             "Import Requirements",
                             "This preview is not complete and cannot "
                             "be imported.");

        return;
    }

    const int changes = m_newCount + m_quantityChangedCount;

    if (changes == 0) {
        QMessageBox::information(this,
                                 "Import Requirements",
                                 "The Build Requirements already match "
                                 "the Rebrickable Set Parts data.");

        return;
    }

    const QMessageBox::StandardButton response
        = QMessageBox::question(this,
                                "Import Requirements",
                                QString("Import requirements for:\n\n"
                                        "%1\n"
                                        "Set %2\n\n"
                                        "New: %3\n"
                                        "Quantity Changed: %4\n"
                                        "No Change: %5\n\n"
                                        "Regular and spare requirements will "
                                        "remain separate.\n\n"
                                        "Existing requirements that are not in "
                                        "this Rebrickable response will NOT be "
                                        "deleted.\n\n"
                                        "Proceed with the import?")
                                    .arg(m_setDetails.name)
                                    .arg(m_setDetails.setNumber)
                                    .arg(m_newCount)
                                    .arg(m_quantityChangedCount)
                                    .arg(m_noChangeCount),
                                QMessageBox::Yes | QMessageBox::No,
                                QMessageBox::No);

    if (response != QMessageBox::Yes)
        return;

    QSqlDatabase database = DatabaseManager::instance().database();

    if (!database.transaction()) {
        QMessageBox::critical(this,
                              "Import Requirements",
                              "Unable to start the database transaction.");

        return;
    }

    BuildRequirementRepository repository;

    int inserted = 0;
    int updated = 0;

    for (const PreviewRow& previewRow : m_previewRows) {
        if (previewRow.status == ImportStatus::NoChange) {
            continue;
        }

        if (previewRow.status == ImportStatus::MappingProblem) {
            database.rollback();

            QMessageBox::critical(this,
                                  "Import Requirements",
                                  "A mapping problem was detected while "
                                  "importing. No changes were saved.");

            return;
        }

        if (previewRow.status == ImportStatus::New) {
            BuildRequirement requirement;

            requirement.setBuildId(m_buildId);

            requirement.setPartId(previewRow.partId);

            requirement.setColorId(previewRow.colorId);

            requirement.setQuantityRequired(previewRow.setPart.quantity);

            requirement.setIsSpare(previewRow.setPart.isSpare);

            if (!repository.create(requirement)) {
                database.rollback();

                QMessageBox::critical(this,
                                      "Import Requirements",
                                      QString("Unable to create requirement "
                                              "for part %1.\n\n"
                                              "No changes were saved.")
                                          .arg(previewRow.setPart.partNumber));

                return;
            }

            ++inserted;

            continue;
        }

        if (previewRow.status == ImportStatus::QuantityChanged) {
            std::optional<BuildRequirement> requirement;

            const QList<BuildRequirement> currentRequirements = repository.getByBuild(m_buildId);

            for (const BuildRequirement& current : currentRequirements) {
                if (current.id() == previewRow.existingRequirementId) {
                    requirement = current;

                    break;
                }
            }

            if (!requirement) {
                database.rollback();

                QMessageBox::critical(this,
                                      "Import Requirements",
                                      QString("Unable to locate existing "
                                              "requirement for part %1.\n\n"
                                              "No changes were saved.")
                                          .arg(previewRow.setPart.partNumber));

                return;
            }

            requirement->setQuantityRequired(previewRow.setPart.quantity);

            if (!repository.update(*requirement)) {
                database.rollback();

                QMessageBox::critical(this,
                                      "Import Requirements",
                                      QString("Unable to update requirement "
                                              "for part %1.\n\n"
                                              "No changes were saved.")
                                          .arg(previewRow.setPart.partNumber));

                return;
            }

            ++updated;
        }
    }

    if (!database.commit()) {
        database.rollback();

        QMessageBox::critical(this,
                              "Import Requirements",
                              "Unable to commit the requirement import. "
                              "No changes were saved.");

        return;
    }

    QMessageBox::information(this,
                             "Import Requirements",
                             QString("Set requirements imported successfully.\n\n"
                                     "New: %1\n"
                                     "Updated: %2\n"
                                     "Unchanged: %3")
                                 .arg(inserted)
                                 .arg(updated)
                                 .arg(m_noChangeCount));

    //
    // Accepted tells BuildsWidget that data changed
    // and its requirement table should be refreshed.
    //
    accept();
}