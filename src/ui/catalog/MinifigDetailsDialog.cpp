#include "MinifigDetailsDialog.h"

#include "../../models/MinifigCatalogItem.h"
#include "../../models/MinifigExternalIdentifier.h"
#include "../../repositories/MinifigCatalogRepository.h"
#include "../../services/images/MinifigImageService.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QVBoxLayout>

MinifigDetailsDialog::MinifigDetailsDialog(int minifigCatalogId, QWidget* parent)
    : QDialog(parent)
    , m_minifigCatalogId(minifigCatalogId)
    , m_imageService(new MinifigImageService(this))
{
    setWindowTitle("Minifig Details");
    resize(700, 430);

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

    auto* scopeLabel = new QLabel(
        "This catalog entry provides identification metadata. Constituent parts and "
        "inventory comparison are not available in this phase.",
        this);
    scopeLabel->setWordWrap(true);
    mainLayout->addWidget(scopeLabel);
    mainLayout->addStretch();

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttons);

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
