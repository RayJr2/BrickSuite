#include "GlobalRebrickableImportDialog.h"

#include "../../import/global/RebrickableDatasetRegistry.h"
#include "../../import/global/RebrickableImportDiscoveryService.h"

#include <QAbstractItemView>
#include <QDialogButtonBox>
#include <QDebug>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QThread>
#include <QVBoxLayout>

#include <memory>

GlobalRebrickableImportDialog::GlobalRebrickableImportDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Import Rebrickable Data Files"));
    resize(900, 560);

    auto* layout = new QVBoxLayout(this);
    auto* attribution = new QLabel(
        QStringLiteral("<b>Data source: Rebrickable</b><br>"
                       "Select a folder containing official Rebrickable CSV or CSV.ZIP bulk downloads."),
        this);
    attribution->setWordWrap(true);
    layout->addWidget(attribution);

    auto* folderLayout = new QHBoxLayout;
    m_directoryEdit = new QLineEdit(this);
    m_directoryEdit->setReadOnly(true);
    m_directoryEdit->setPlaceholderText(QStringLiteral("No folder selected"));
    m_browseButton = new QPushButton(QStringLiteral("Select Folder..."), this);
    m_rescanButton = new QPushButton(QStringLiteral("Rescan"), this);
    m_rescanButton->setEnabled(false);
    folderLayout->addWidget(m_directoryEdit, 1);
    folderLayout->addWidget(m_browseButton);
    folderLayout->addWidget(m_rescanButton);
    layout->addLayout(folderLayout);

    m_table = new QTableWidget(0, 4, this);
    m_table->setHorizontalHeaderLabels(
        {QStringLiteral("Dataset"), QStringLiteral("Discovered file"),
         QStringLiteral("Status"), QStringLiteral("Validation / dependency")});
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    layout->addWidget(m_table, 1);

    m_progressLabel = new QLabel(QStringLiteral("No import is running."), this);
    m_summaryLabel = new QLabel(
        QStringLiteral("M25.1 provides discovery and validation only. Dataset import will be enabled in a later M25 phase."),
        this);
    m_summaryLabel->setWordWrap(true);
    layout->addWidget(m_progressLabel);
    layout->addWidget(m_summaryLabel);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    m_importButton = buttons->addButton(QStringLiteral("Import"), QDialogButtonBox::ActionRole);
    m_importButton->setEnabled(false);
    m_importButton->setToolTip(QStringLiteral("Global dataset execution is not implemented in M25.1."));
    layout->addWidget(buttons);

    connect(m_browseButton, &QPushButton::clicked, this, &GlobalRebrickableImportDialog::selectFolder);
    connect(m_rescanButton, &QPushButton::clicked, this, &GlobalRebrickableImportDialog::rescan);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);

    RebrickableImportPlan emptyPlan;
    for (const auto& descriptor : RebrickableDatasetRegistry::datasets()) {
        RebrickableImportPlanEntry entry;
        entry.dataset = descriptor.id;
        entry.displayName = descriptor.displayName;
        entry.status = RebrickableImportStatus::Missing;
        entry.message = QStringLiteral("Select a folder to scan.");
        emptyPlan.entries.append(entry);
    }
    displayPlan(emptyPlan);
}

void GlobalRebrickableImportDialog::selectFolder()
{
    const QString selected = QFileDialog::getExistingDirectory(
        this, QStringLiteral("Select Rebrickable Data Folder"), m_directoryEdit->text());
    if (selected.isEmpty())
        return;
    qInfo() << "Rebrickable data folder selected:" << selected;
    m_directoryEdit->setText(selected);
    m_rescanButton->setEnabled(true);
    rescan();
}

void GlobalRebrickableImportDialog::rescan()
{
    if (m_directoryEdit->text().isEmpty() || m_scanRunning)
        return;
    m_scanRunning = true;
    m_browseButton->setEnabled(false);
    m_rescanButton->setEnabled(false);
    m_progressLabel->setText(QStringLiteral("Scanning and validating supported files..."));
    const QString directory = m_directoryEdit->text();
    auto result = std::make_shared<RebrickableImportPlan>();
    QThread* worker = QThread::create([result, directory]() {
        *result = RebrickableImportDiscoveryService().buildPlan(directory);
    });
    connect(worker, &QThread::finished, this, [this, result]() {
        displayPlan(*result);
        m_scanRunning = false;
        m_browseButton->setEnabled(true);
        m_rescanButton->setEnabled(!m_directoryEdit->text().isEmpty());
        m_progressLabel->setText(QStringLiteral("Scan complete."));
    });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void GlobalRebrickableImportDialog::displayPlan(const RebrickableImportPlan& plan)
{
    m_table->setRowCount(plan.entries.size());
    int recognized = 0;
    int ready = 0;
    int attention = 0;
    for (int row = 0; row < plan.entries.size(); ++row) {
        const auto& entry = plan.entries.at(row);
        QString fileName;
        if (entry.status == RebrickableImportStatus::Ambiguous) {
            QStringList names;
            for (const QString& path : entry.conflictingSourcePaths)
                names.append(QFileInfo(path).fileName());
            fileName = names.join(QStringLiteral(", "));
        } else if (!entry.sourcePath.isEmpty()) {
            fileName = QFileInfo(entry.sourcePath).fileName();
        } else {
            fileName = QStringLiteral("—");
        }
        m_table->setItem(row, 0, new QTableWidgetItem(entry.displayName));
        m_table->setItem(row, 1, new QTableWidgetItem(fileName));
        m_table->setItem(row, 2, new QTableWidgetItem(rebrickableImportStatusText(entry.status)));
        m_table->setItem(row, 3, new QTableWidgetItem(entry.message));
        if (entry.status != RebrickableImportStatus::Missing)
            ++recognized;
        if (entry.status == RebrickableImportStatus::Ready)
            ++ready;
        if (entry.status == RebrickableImportStatus::Ambiguous
            || entry.status == RebrickableImportStatus::Invalid
            || entry.status == RebrickableImportStatus::BlockedByDependency)
            ++attention;
    }
    m_summaryLabel->setText(QStringLiteral(
        "Recognized: %1 · Ready for a future import phase: %2 · Needs attention: %3. "
        "Missing datasets are optional and are not failures. M25.1 does not modify the database.")
                                .arg(recognized).arg(ready).arg(attention));
}
