#include "PartDetailsDialog.h"

#include "../../models/Part.h"
#include "../../models/PartCategory.h"

#include "../../repositories/PartCategoryRepository.h"
#include "../../repositories/PartRepository.h"

#include "../../services/images/PartImageService.h"
#include "../../settings/UserSettings.h"

#include <QDesktopServices>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTabWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

PartDetailsDialog::PartDetailsDialog(
    int partId,
    QWidget* parent)
    : QDialog(parent)
    , m_partId(partId)
{
    setWindowTitle("Part Details");

    resize(850, 650);

    auto* mainLayout =
        new QVBoxLayout(this);

    //
    // Top section
    //
    auto* topLayout =
        new QHBoxLayout();

    m_imageLabel =
        new QLabel(this);

    m_imageLabel->setFixedSize(
        220,
        180);

    m_imageLabel->setAlignment(
        Qt::AlignCenter);

    m_imageLabel->setText(
        "No Image");

    auto* detailsGroup =
        new QGroupBox(
            "Part",
            this);

    auto* detailsLayout =
        new QFormLayout(
            detailsGroup);

    m_partNumberLabel =
        new QLabel(detailsGroup);

    m_nameLabel =
        new QLabel(detailsGroup);

    m_categoryLabel =
        new QLabel(detailsGroup);

    m_materialLabel =
        new QLabel(detailsGroup);

    m_yearsLabel =
        new QLabel(
            "Loading...",
            detailsGroup);

    m_rebrickableStatusLabel =
        new QLabel(
            "Loading Rebrickable details...",
            detailsGroup);

    m_rebrickableStatusLabel
        ->setWordWrap(true);

    detailsLayout->addRow(
        "Part #:",
        m_partNumberLabel);

    detailsLayout->addRow(
        "Name:",
        m_nameLabel);

    detailsLayout->addRow(
        "Category:",
        m_categoryLabel);

    detailsLayout->addRow(
        "Material:",
        m_materialLabel);

    detailsLayout->addRow(
        "Years:",
        m_yearsLabel);

    detailsLayout->addRow(
        "Rebrickable:",
        m_rebrickableStatusLabel);

    topLayout->addWidget(
        m_imageLabel);

    topLayout->addWidget(
        detailsGroup,
        1);

    mainLayout->addLayout(
        topLayout);

    //
    // Tabs
    //
    m_tabWidget =
        new QTabWidget(this);

    //
    // General tab
    //
    auto* generalTab =
        new QWidget(m_tabWidget);

    auto* generalLayout =
        new QVBoxLayout(generalTab);

    m_openRebrickableButton =
        new QPushButton(
            "Open Rebrickable Part Page",
            generalTab);

    m_openRebrickableButton
        ->setEnabled(false);

    generalLayout->addWidget(
        m_openRebrickableButton);

    generalLayout->addStretch();

    m_tabWidget->addTab(
        generalTab,
        "General");

    //
    // Related Parts tab
    //
    auto* relatedTab =
        new QWidget(m_tabWidget);

    auto* relatedLayout =
        new QHBoxLayout(relatedTab);

    auto* moldsGroup =
        new QGroupBox(
            "Molds",
            relatedTab);

    auto* moldsLayout =
        new QVBoxLayout(
            moldsGroup);

    m_moldsList =
        new QListWidget(
            moldsGroup);

    moldsLayout->addWidget(
        m_moldsList);

    auto* alternatesGroup =
        new QGroupBox(
            "Alternates",
            relatedTab);

    auto* alternatesLayout =
        new QVBoxLayout(
            alternatesGroup);

    m_alternatesList =
        new QListWidget(
            alternatesGroup);

    alternatesLayout->addWidget(
        m_alternatesList);

    auto* printsGroup =
        new QGroupBox(
            "Prints",
            relatedTab);

    auto* printsLayout =
        new QVBoxLayout(
            printsGroup);

    m_printsList =
        new QListWidget(
            printsGroup);

    printsLayout->addWidget(
        m_printsList);

    relatedLayout->addWidget(
        moldsGroup);

    relatedLayout->addWidget(
        alternatesGroup);

    relatedLayout->addWidget(
        printsGroup);

    m_tabWidget->addTab(
        relatedTab,
        "Related Parts");

    //
    // External IDs tab
    //
    auto* externalTab =
        new QWidget(m_tabWidget);

    auto* externalLayout =
        new QVBoxLayout(externalTab);

    m_externalIdsTable =
        new QTableWidget(
            externalTab);

    m_externalIdsTable
        ->setColumnCount(2);

    m_externalIdsTable
        ->setHorizontalHeaderLabels(
            QStringList()
            << "Provider"
            << "IDs");

    m_externalIdsTable
        ->setEditTriggers(
            QAbstractItemView::NoEditTriggers);

    m_externalIdsTable
        ->verticalHeader()
        ->setVisible(false);

    m_externalIdsTable
        ->horizontalHeader()
        ->setSectionResizeMode(
            0,
            QHeaderView::ResizeToContents);

    m_externalIdsTable
        ->horizontalHeader()
        ->setSectionResizeMode(
            1,
            QHeaderView::Stretch);

    externalLayout->addWidget(
        m_externalIdsTable);

    m_tabWidget->addTab(
        externalTab,
        "External IDs");

    mainLayout->addWidget(
        m_tabWidget,
        1);

    //
    // Bottom buttons
    //
    auto* buttonLayout =
        new QHBoxLayout();

    buttonLayout->addStretch();

    m_closeButton =
        new QPushButton(
            "Close",
            this);

    buttonLayout->addWidget(
        m_closeButton);

    mainLayout->addLayout(
        buttonLayout);

    //
    // Services
    //
    m_rebrickableApiClient =
        new RebrickableApiClient(this);

    m_partImageService =
        new PartImageService(this);

    connect(
        m_closeButton,
        &QPushButton::clicked,
        this,
        &QDialog::accept);

    connect(
        m_openRebrickableButton,
        &QPushButton::clicked,
        this,
        [this]()
        {
            if (m_rebrickableUrl.isEmpty())
                return;

            QDesktopServices::openUrl(
                QUrl(m_rebrickableUrl));
        });

    connect(
        m_partImageService,
        &PartImageService::imageReady,
        this,
        [this](
            const QString& partNumber,
            const QString& imagePath)
        {
            if (partNumber !=
                m_partNumber)
            {
                return;
            }

            QPixmap pixmap(
                imagePath);

            if (pixmap.isNull())
                return;

            m_imageLabel->setPixmap(
                pixmap.scaled(
                    m_imageLabel->size(),
                    Qt::KeepAspectRatio,
                    Qt::SmoothTransformation));

            m_imageLabel->setText(
                QString());
        });

    connect(m_rebrickableApiClient,
            &RebrickableApiClient::partDetailsFinished,
            this,
            [this](const RebrickableApiClient::PartDetailsResult& result) {
                if (!result.success) {
                    m_yearsLabel->setText("Unavailable");

                    m_rebrickableStatusLabel->setText(result.message);

                    m_moldsList->clear();
                    m_alternatesList->clear();
                    m_printsList->clear();

                    m_moldsList->addItem("(Unavailable)");

                    m_alternatesList->addItem("(Unavailable)");

                    m_printsList->addItem("(Unavailable)");

                    return;
                }

                if (result.part.partNumber != m_partNumber) {
                    return;
                }

                m_yearsLabel->setText(
                    QString("%1 - %2").arg(result.part.yearFrom).arg(result.part.yearTo));

                m_rebrickableStatusLabel->setText("Details loaded.");

                m_rebrickableUrl = result.part.partUrl;

                m_openRebrickableButton->setEnabled(!m_rebrickableUrl.isEmpty());

                populateRelatedParts(result);

                populateExternalIds(result);

                if (!result.part.partImageUrl.isEmpty()) {
                    m_partImageService->requestPartImage(result.part.partNumber,
                                                         result.part.partImageUrl);
                }
            });

    if (!loadLocalPart())
    {
        QMessageBox::warning(
            this,
            "BrickSuite",
            "Unable to load the selected part.");

        m_rebrickableStatusLabel
            ->setText(
                "Part unavailable.");

        return;
    }

    loadCachedImage();

    requestRebrickableDetails();
}

bool PartDetailsDialog::loadLocalPart()
{
    if (m_partId <= 0)
        return false;

    PartRepository partRepository;

    const std::optional<Part> part = partRepository.getById(m_partId);

    if (!part)
        return false;

    m_partNumber = part->partNumber();

    m_partNumberLabel->setText(part->partNumber());

    m_nameLabel->setText(part->name());

    m_materialLabel->setText(part->material());

    QString categoryName;

    if (part->partCategoryId() > 0) {
        PartCategoryRepository categoryRepository;

        const std::optional<PartCategory> category = categoryRepository.getById(
            part->partCategoryId());

        if (category) {
            categoryName = category->name();
        }
    }

    m_categoryLabel->setText(categoryName);

    return true;
}

void PartDetailsDialog::loadCachedImage()
{
    if (m_partNumber.isEmpty())
        return;

    const QString cachedPath = m_partImageService->cachedImagePath(m_partNumber);

    if (cachedPath.isEmpty())
        return;

    QPixmap pixmap(cachedPath);

    if (pixmap.isNull())
        return;

    m_imageLabel->setPixmap(
        pixmap.scaled(m_imageLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));

    m_imageLabel->setText(QString());
}

void PartDetailsDialog::requestRebrickableDetails()
{
    const QString apiKey = UserSettings::instance().rebrickableApiKey();

    if (apiKey.isEmpty()) {
        m_rebrickableStatusLabel->setText("No Rebrickable API key configured.");

        return;
    }

    if (m_partNumber.isEmpty())
        return;

    m_rebrickableApiClient->getPartDetails(m_partNumber, apiKey);
}

void PartDetailsDialog::populateRelatedParts(const RebrickableApiClient::PartDetailsResult& result)
{
    populateRelatedList(m_moldsList, result.part.molds);

    populateRelatedList(m_alternatesList, result.part.alternates);

    populateRelatedList(m_printsList, result.part.prints);
}

void PartDetailsDialog::populateRelatedList(QListWidget* listWidget, const QStringList& partNumbers)
{
    listWidget->clear();

    if (partNumbers.isEmpty()) {
        listWidget->addItem("(None)");

        return;
    }

    PartRepository repository;

    for (const QString& partNumber : partNumbers) {
        const std::optional<Part> part = repository.getByPartNumber(partNumber);

        if (part) {
            listWidget->addItem(QString("%1 — %2").arg(part->partNumber(), part->name()));
        } else {
            listWidget->addItem(QString("%1 — Not in local catalog").arg(partNumber));
        }
    }
}

void PartDetailsDialog::populateExternalIds(const RebrickableApiClient::PartDetailsResult& result)
{
    m_externalIdsTable->setRowCount(0);

    QStringList providers = result.part.externalIds.keys();

    providers.sort(Qt::CaseInsensitive);

    int row = 0;

    for (const QString& provider : providers) {
        const QStringList ids = result.part.externalIds.value(provider);

        m_externalIdsTable->insertRow(row);

        m_externalIdsTable->setItem(row, 0, new QTableWidgetItem(provider));

        m_externalIdsTable->setItem(row, 1, new QTableWidgetItem(ids.join(", ")));

        ++row;
    }
}