#include "MissingPartsExportDialog.h"

#include "../../services/builds/MissingPartsCsvWriter.h"
#include "../../services/builds/MissingPartsExportService.h"
#include "../../settings/UserSettings.h"

#include <QAbstractItemView>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QHash>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <utility>

MissingPartsExportDialog::MissingPartsExportDialog(
    QList<MissingPartsExportRow> rows, QString defaultFileName, QWidget* parent)
    : QDialog(parent), m_rows(std::move(rows)), m_defaultFileName(std::move(defaultFileName))
{
    setWindowTitle(QStringLiteral("Missing Parts Export"));
    resize(1050, 650);

    auto* root = new QVBoxLayout(this);
    auto* description = new QLabel(
        QStringLiteral("General CSV — choose fields and their order. The preview is exactly what will be exported."),
        this);
    description->setWordWrap(true);
    root->addWidget(description);

    auto* content = new QHBoxLayout;
    auto* fieldLayout = new QVBoxLayout;
    fieldLayout->addWidget(new QLabel(QStringLiteral("Export fields:"), this));
    m_fields = new QListWidget(this);
    m_fields->setSelectionMode(QAbstractItemView::SingleSelection);
    fieldLayout->addWidget(m_fields, 1);
    auto* ordering = new QHBoxLayout;
    m_moveUp = new QPushButton(QStringLiteral("Move Up"), this);
    m_moveDown = new QPushButton(QStringLiteral("Move Down"), this);
    ordering->addWidget(m_moveUp);
    ordering->addWidget(m_moveDown);
    fieldLayout->addLayout(ordering);
    auto* reset = new QPushButton(QStringLiteral("Reset to Defaults"), this);
    fieldLayout->addWidget(reset);
    content->addLayout(fieldLayout, 0);

    m_preview = new QTableWidget(this);
    m_preview->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_preview->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_preview->setSortingEnabled(false);
    m_preview->verticalHeader()->setVisible(false);
    content->addWidget(m_preview, 1);
    root->addLayout(content, 1);

    m_status = new QLabel(this);
    root->addWidget(m_status);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    m_export = buttons->addButton(QStringLiteral("Export..."), QDialogButtonBox::AcceptRole);
    root->addWidget(buttons);

    connect(m_fields, &QListWidget::itemChanged, this, [this] {
        persistConfiguration();
        updatePreview();
    });
    connect(m_fields, &QListWidget::currentRowChanged, this, [this] {
        const int row = m_fields->currentRow();
        m_moveUp->setEnabled(row > 0);
        m_moveDown->setEnabled(row >= 0 && row + 1 < m_fields->count());
    });
    connect(m_moveUp, &QPushButton::clicked, this, [this] { moveCurrentField(-1); });
    connect(m_moveDown, &QPushButton::clicked, this, [this] { moveCurrentField(1); });
    connect(reset, &QPushButton::clicked, this, [this] {
        applyConfiguration(MissingPartsExportService::defaultConfiguration());
        persistConfiguration();
        updatePreview();
    });
    connect(m_export, &QPushButton::clicked, this, &MissingPartsExportDialog::exportCsv);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    loadConfiguration();
    updatePreview();
}

void MissingPartsExportDialog::loadConfiguration()
{
    const auto& settings = UserSettings::instance();
    applyConfiguration(MissingPartsExportService::normalizeConfiguration(
        settings.missingPartsExportFieldOrder(),
        settings.missingPartsExportEnabledFields()));
}

void MissingPartsExportDialog::applyConfiguration(
    const MissingPartsExportConfiguration& configuration)
{
    QSignalBlocker blocker(m_fields);
    m_fields->clear();
    QHash<QString, MissingPartsExportFieldDescriptor> descriptors;
    for (const auto& descriptor : MissingPartsExportService::fieldDescriptors())
        descriptors.insert(descriptor.id, descriptor);
    for (const QString& id : configuration.fieldOrder) {
        if (!descriptors.contains(id)) continue;
        const auto descriptor = descriptors.value(id);
        auto* item = new QListWidgetItem(descriptor.displayName, m_fields);
        item->setData(Qt::UserRole, descriptor.id);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(configuration.enabledFields.contains(id)
                                ? Qt::Checked : Qt::Unchecked);
        if (descriptor.ambiguous)
            item->setToolTip(QStringLiteral("Multiple authoritative values are separated with semicolons."));
    }
    if (m_fields->count() > 0) m_fields->setCurrentRow(0);
}

MissingPartsExportConfiguration MissingPartsExportDialog::configuration() const
{
    MissingPartsExportConfiguration result;
    for (int row = 0; row < m_fields->count(); ++row) {
        const auto* item = m_fields->item(row);
        const QString id = item->data(Qt::UserRole).toString();
        result.fieldOrder.append(id);
        if (item->checkState() == Qt::Checked) result.enabledFields.insert(id);
    }
    return result;
}

void MissingPartsExportDialog::persistConfiguration()
{
    const auto current = configuration();
    UserSettings::instance().setMissingPartsExportConfiguration(
        current.fieldOrder, current.enabledFields.values());
}

void MissingPartsExportDialog::moveCurrentField(int offset)
{
    const int current = m_fields->currentRow();
    const int destination = current + offset;
    if (current < 0 || destination < 0 || destination >= m_fields->count()) return;
    QSignalBlocker blocker(m_fields);
    QListWidgetItem* item = m_fields->takeItem(current);
    m_fields->insertItem(destination, item);
    m_fields->setCurrentRow(destination);
    persistConfiguration();
    updatePreview();
}

void MissingPartsExportDialog::updatePreview()
{
    const auto projection = MissingPartsExportService::project(m_rows, configuration());
    m_preview->clear();
    m_preview->setColumnCount(projection.headers.size());
    m_preview->setHorizontalHeaderLabels(projection.headers);
    m_preview->setRowCount(projection.rows.size());
    for (int row = 0; row < projection.rows.size(); ++row) {
        for (int column = 0; column < projection.rows.at(row).size(); ++column)
            m_preview->setItem(row, column,
                               new QTableWidgetItem(projection.rows.at(row).at(column)));
    }
    m_preview->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_preview->horizontalHeader()->setStretchLastSection(!projection.headers.isEmpty());
    m_status->setText(QStringLiteral("%1 missing Part/Color row(s); %2 export field(s).")
                          .arg(m_rows.size()).arg(projection.fields.size()));
    m_export->setEnabled(!projection.fields.isEmpty() && !m_rows.isEmpty());
}

void MissingPartsExportDialog::exportCsv()
{
    const auto projection = MissingPartsExportService::project(m_rows, configuration());
    const QString fileName = QFileDialog::getSaveFileName(
        this, QStringLiteral("Export Missing Parts CSV"), m_defaultFileName,
        QStringLiteral("CSV Files (*.csv)"));
    if (fileName.isEmpty()) return;
    const auto result = MissingPartsCsvWriter::write(fileName, projection);
    if (!result.success) {
        QMessageBox::critical(this, QStringLiteral("Export Missing Parts"), result.message);
        return;
    }
    int pieces = 0;
    for (const auto& row : m_rows) pieces += row.missing;
    QMessageBox::information(this, QStringLiteral("Export Missing Parts"),
        QStringLiteral("Missing Parts List exported successfully.\n\nPart/Color Rows: %1\nPieces Missing: %2\nFile:\n%3")
            .arg(m_rows.size()).arg(pieces).arg(fileName));
    accept();
}
