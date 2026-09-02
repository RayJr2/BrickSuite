#include "MinifigDetailsDialog.h"

#include "../../models/MinifigCatalogItem.h"
#include "../../models/MinifigExternalIdentifier.h"
#include "../../models/MinifigCatalogPart.h"
#include "../../import/RebrickableMinifigPartsImporter.h"
#include "../../repositories/MinifigCatalogRepository.h"
#include "../../repositories/MinifigCatalogPartRepository.h"
#include "../../services/images/MinifigImageService.h"
#include "../../services/images/PartImageService.h"

#include <QAbstractItemView>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

MinifigDetailsDialog::MinifigDetailsDialog(int minifigCatalogId, QWidget* parent)
    : QDialog(parent)
    , m_minifigCatalogId(minifigCatalogId)
    , m_imageService(new MinifigImageService(this))
    , m_partImageService(new PartImageService(this))
{
    setWindowTitle("Minifig Details");
    resize(900, 720);

    auto* mainLayout = new QVBoxLayout(this);
    auto* definitionGroup = new QGroupBox("Catalog Definition", this);
    auto* contentLayout = new QHBoxLayout(definitionGroup);
    auto* imageLayout = new QVBoxLayout();
    m_imageLabel = new QLabel("No Image", definitionGroup);
    m_imageLabel->setFixedSize(280, 280);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setStyleSheet("QLabel { border: 1px solid gray; }");
    m_imageStatusLabel = new QLabel(definitionGroup);
    m_imageStatusLabel->setAlignment(Qt::AlignCenter);
    imageLayout->addWidget(m_imageLabel);
    imageLayout->addWidget(m_imageStatusLabel);
    contentLayout->addLayout(imageLayout);

    auto* formLayout = new QFormLayout();
    m_numberLabel = new QLabel(definitionGroup);
    m_nameLabel = new QLabel(definitionGroup);
    m_nameLabel->setWordWrap(true);
    m_partsLabel = new QLabel(definitionGroup);
    m_providerLabel = new QLabel(definitionGroup);
    m_sourceLabel = new QLabel(definitionGroup);
    m_sourceLabel->setWordWrap(true);
    formLayout->addRow("Minifig #:", m_numberLabel);
    formLayout->addRow("Name:", m_nameLabel);
    formLayout->addRow("Declared Parts:", m_partsLabel);
    formLayout->addRow("Provider:", m_providerLabel);
    formLayout->addRow("Source:", m_sourceLabel);
    contentLayout->addLayout(formLayout, 1);
    mainLayout->addWidget(definitionGroup);

    auto* compositionGroup = new QGroupBox("Imported Parts List", this);
    auto* compositionLayout = new QVBoxLayout(compositionGroup);
    auto* compositionHeader = new QHBoxLayout();
    m_compositionSummaryLabel = new QLabel(compositionGroup);
    m_compositionSummaryLabel->setWordWrap(true);
    m_importPartsButton = new QPushButton("Import Parts List...", compositionGroup);
    compositionHeader->addWidget(m_compositionSummaryLabel, 1);
    compositionHeader->addWidget(m_importPartsButton);
    compositionLayout->addLayout(compositionHeader);

    m_compositionTable = new QTableWidget(compositionGroup);
    m_compositionTable->setColumnCount(6);
    m_compositionTable->setHorizontalHeaderLabels(
        QStringList() << "Image" << "Part #" << "Part Name" << "Color" << "Qty" << "Spare");
    m_compositionTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_compositionTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_compositionTable->setIconSize(QSize(48, 48));
    m_compositionTable->verticalHeader()->setVisible(false);
    m_compositionTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_compositionTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_compositionTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_compositionTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_compositionTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_compositionTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    compositionLayout->addWidget(m_compositionTable);
    mainLayout->addWidget(compositionGroup, 1);

    auto* scopeLabel = new QLabel(
        "Imported parts describe the catalog composition. Create a Build from Stock to snapshot "
        "the required pieces and use BrickSuite's normal allocation and pulling workflow.", this);
    scopeLabel->setWordWrap(true);
    mainLayout->addWidget(scopeLabel);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    m_createBuildButton = buttons->addButton("Create Build From Stock...",
                                             QDialogButtonBox::ActionRole);
    m_createBuildButton->setEnabled(false);
    m_createBuildButton->setToolTip("Import a Minifig parts list first.");
    connect(m_createBuildButton, &QPushButton::clicked,
            this, &MinifigDetailsDialog::createBuildFromStock);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttons);
    connect(m_importPartsButton,
            &QPushButton::clicked,
            this,
            &MinifigDetailsDialog::importPartsList);

    connect(m_imageService,
            &MinifigImageService::imageReady,
            this,
            [this](const QString& minifigNumber, const QString& imagePath) {
                if (minifigNumber.compare(m_minifigNumber, Qt::CaseInsensitive) == 0)
                    displayImage(imagePath);
            });
    connect(m_imageService,
            &MinifigImageService::imageFailed,
            this,
            [this](const QString& minifigNumber, const QString& message) {
                if (minifigNumber.compare(m_minifigNumber, Qt::CaseInsensitive) == 0)
                    m_imageStatusLabel->setText(message);
            });

    if (!loadMinifig())
        return;

    loadComposition();

    const QString cachedPath = m_imageService->cachedImagePath(m_minifigNumber);
    if (!cachedPath.isEmpty()) {
        displayImage(cachedPath);
    } else if (m_imageUrl.isEmpty()) {
        m_imageStatusLabel->setText("No image URL is available.");
    } else {
        m_imageStatusLabel->setText("Loading...");
        m_imageService->requestMinifigImage(m_minifigNumber, m_imageUrl);
    }
}

bool MinifigDetailsDialog::loadMinifig()
{
    MinifigCatalogRepository repository;
    const std::optional<MinifigCatalogItem> minifig = repository.getById(m_minifigCatalogId);
    if (!minifig) {
        m_nameLabel->setText("Unable to load Minifig.");
        return false;
    }

    const QList<MinifigExternalIdentifier> identifiers = repository.identifiersForMinifig(
        m_minifigCatalogId);
    for (const MinifigExternalIdentifier& identifier : identifiers) {
        if (identifier.provider.compare("Rebrickable", Qt::CaseInsensitive) == 0) {
            m_minifigNumber = identifier.externalId;
            m_providerLabel->setText(identifier.provider);
            m_sourceLabel->setText(identifier.source);
            break;
        }
    }

    m_imageUrl = minifig->imageUrl();
    m_minifigName = minifig->name();
    m_numberLabel->setText(m_minifigNumber.isEmpty() ? "-" : m_minifigNumber);
    m_nameLabel->setText(minifig->name());
    m_partsLabel->setText(QString::number(minifig->numberOfParts()));
    if (m_providerLabel->text().isEmpty())
        m_providerLabel->setText("Local catalog");
    if (m_sourceLabel->text().isEmpty())
        m_sourceLabel->setText("-");
    setWindowTitle(QString("Minifig Details — %1").arg(m_numberLabel->text()));
    return true;
}

void MinifigDetailsDialog::loadComposition()
{
    MinifigCatalogPartRepository repository;
    const QList<MinifigCatalogPart> composition = repository.listForMinifig(m_minifigCatalogId);
    m_compositionTable->setRowCount(0);
    m_requiredPieces = 0;
    m_sparePieces = 0;

    if (composition.isEmpty()) {
        m_compositionSummaryLabel->setText(
            "Parts list has not been imported for this Minifig.");
        return;
    }

    qint64 requiredQuantity = 0;
    qint64 spareQuantity = 0;
    for (const MinifigCatalogPart& part : composition) {
        if (part.isSpare)
            spareQuantity += part.quantityRequired;
        else
            requiredQuantity += part.quantityRequired;

        const int row = m_compositionTable->rowCount();
        m_compositionTable->insertRow(row);
        m_compositionTable->setRowHeight(row, 54);
        auto* imageItem = new QTableWidgetItem("No Image");
        QString cachedPath = m_partImageService->cachedPartColorImagePath(
            part.partNumber, part.rebrickableColorId);
        if (cachedPath.isEmpty())
            cachedPath = m_partImageService->cachedImagePath(part.partNumber);
        const QPixmap pixmap(cachedPath);
        if (!pixmap.isNull()) {
            imageItem->setText(QString());
            imageItem->setIcon(QIcon(pixmap));
        }
        imageItem->setData(Qt::UserRole, part.id);
        m_compositionTable->setItem(row, 0, imageItem);
        m_compositionTable->setItem(row, 1, new QTableWidgetItem(part.partNumber));
        m_compositionTable->setItem(row, 2, new QTableWidgetItem(part.partName));
        m_compositionTable->setItem(row, 3, new QTableWidgetItem(part.colorName));
        auto* quantityItem = new QTableWidgetItem(QString::number(part.quantityRequired));
        quantityItem->setTextAlignment(Qt::AlignCenter);
        m_compositionTable->setItem(row, 4, quantityItem);
        auto* spareItem = new QTableWidgetItem(part.isSpare ? "Yes" : "No");
        spareItem->setTextAlignment(Qt::AlignCenter);
        m_compositionTable->setItem(row, 5, spareItem);
    }

    m_compositionSummaryLabel->setText(
        QString("%1 distinct rows; %2 required pieces; %3 spare pieces retained.")
            .arg(composition.size())
            .arg(requiredQuantity)
            .arg(spareQuantity));
    m_requiredPieces = static_cast<int>(requiredQuantity);
    m_sparePieces = static_cast<int>(spareQuantity);
    m_createBuildButton->setEnabled(m_requiredPieces > 0);
    m_createBuildButton->setToolTip(m_requiredPieces > 0
                                        ? QString()
                                        : "Import a parts list containing required parts first.");
}

void MinifigDetailsDialog::createBuildFromStock()
{
    if (m_requiredPieces <= 0) {
        QMessageBox::information(this, "Create Minifig Build",
                                 "Import a Minifig parts list containing required parts first.");
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle("Create Build From Stock");
    auto* layout = new QFormLayout(&dialog);
    layout->addRow("Minifig #:", new QLabel(m_minifigNumber.isEmpty() ? "-" : m_minifigNumber, &dialog));
    layout->addRow("Minifig:", new QLabel(m_minifigName, &dialog));
    layout->addRow("Required pieces:", new QLabel(QString::number(m_requiredPieces), &dialog));
    layout->addRow("Spare pieces excluded:", new QLabel(QString::number(m_sparePieces), &dialog));
    auto* nameEdit = new QLineEdit(m_minifigName, &dialog);
    layout->addRow("Build name:", nameEdit);
    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Cancel, &dialog);
    QPushButton* createButton = buttonBox->addButton("Create", QDialogButtonBox::AcceptRole);
    connect(nameEdit, &QLineEdit::textChanged, createButton,
            [createButton](const QString& text) { createButton->setEnabled(!text.trimmed().isEmpty()); });
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addRow(buttonBox);
    nameEdit->selectAll();
    if (dialog.exec() != QDialog::Accepted)
        return;
    emit createBuildRequested(m_minifigCatalogId, nameEdit->text().trimmed());
}

void MinifigDetailsDialog::importPartsList()
{
    MinifigCatalogPartRepository repository;
    const bool replacing = !repository.listForMinifig(m_minifigCatalogId).isEmpty();
    if (replacing) {
        const auto response = QMessageBox::question(
            this,
            "Replace Minifig Parts List",
            QString("Import a new parts list for %1 (%2)?\n\n"
                    "This replaces the existing imported parts list; it does not merge with it.")
                .arg(m_minifigName, m_minifigNumber),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (response != QMessageBox::Yes)
            return;
    }

    const QString fileName = QFileDialog::getOpenFileName(
        this,
        QString("Import Parts List for %1 (%2)").arg(m_minifigName, m_minifigNumber),
        QString(),
        "Rebrickable Parts Lists (*.csv *.CSV *.zip *.ZIP);;"
        "CSV Files (*.csv *.CSV);;ZIP Files (*.zip *.ZIP)");
    if (fileName.isEmpty())
        return;

    RebrickableMinifigPartsImporter importer;
    const RebrickableMinifigPartsImporter::Result result = importer.importFile(
        m_minifigCatalogId, fileName);
    if (!result.success) {
        QMessageBox::critical(this, "Import Minifig Parts List", result.message);
        return;
    }

    loadComposition();
    QMessageBox::information(
        this,
        "Import Minifig Parts List",
        QString("Parts list imported for %1 (%2).\n\n"
                "CSV rows read: %3\nDistinct composition rows: %4")
            .arg(m_minifigName, m_minifigNumber)
            .arg(result.rowsRead)
            .arg(result.compositionRows));
}

void MinifigDetailsDialog::displayImage(const QString& imagePath)
{
    const QPixmap pixmap(imagePath);
    if (pixmap.isNull()) {
        m_imageStatusLabel->setText("Unable to read cached image.");
        return;
    }
    m_imageLabel->setPixmap(pixmap.scaled(m_imageLabel->size(),
                                          Qt::KeepAspectRatio,
                                          Qt::SmoothTransformation));
    m_imageLabel->setText(QString());
    m_imageStatusLabel->setText("Cached");
}
