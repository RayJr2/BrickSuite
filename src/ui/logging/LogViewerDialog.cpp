#include "LogViewerDialog.h"

#include "../../services/Logger.h"

#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFile>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextCursor>
#include <QUrl>
#include <QVBoxLayout>

LogViewerDialog::LogViewerDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("BrickSuite Application Log");
    resize(950, 650);

    auto* mainLayout = new QVBoxLayout(this);

    m_logEdit = new QPlainTextEdit(this);
    m_logEdit->setReadOnly(true);
    m_logEdit->setLineWrapMode(QPlainTextEdit::NoWrap);

    mainLayout->addWidget(m_logEdit, 1);

    auto* actionLayout = new QHBoxLayout();

    m_refreshButton = new QPushButton("Refresh", this);
    m_clearButton = new QPushButton("Clear Log", this);
    m_openFolderButton = new QPushButton("Open Log Folder", this);

    actionLayout->addWidget(m_refreshButton);
    actionLayout->addWidget(m_clearButton);
    actionLayout->addWidget(m_openFolderButton);
    actionLayout->addStretch(1);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);

    actionLayout->addWidget(buttonBox);

    mainLayout->addLayout(actionLayout);

    connect(m_refreshButton, &QPushButton::clicked, this, [this]() {
        refreshLog();
    });

    connect(m_clearButton, &QPushButton::clicked, this, [this]() {
        clearLog();
    });

    connect(m_openFolderButton, &QPushButton::clicked, this, [this]() {
        openLogFolder();
    });

    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    refreshLog();
}

void LogViewerDialog::refreshLog()
{
    QFile file(Logger::logFilePath());

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_logEdit->setPlainText(
            QString("Unable to open BrickSuite application log.\n\n%1")
                .arg(file.errorString()));
        return;
    }

    m_logEdit->setPlainText(QString::fromUtf8(file.readAll()));

    QTextCursor cursor = m_logEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_logEdit->setTextCursor(cursor);
}

void LogViewerDialog::clearLog()
{
    const QMessageBox::StandardButton response
        = QMessageBox::question(this,
                                "Clear BrickSuite Log",
                                "Clear the current BrickSuite application log?",
                                QMessageBox::Yes | QMessageBox::No,
                                QMessageBox::No);

    if (response != QMessageBox::Yes)
        return;

    QString errorMessage;

    if (!Logger::clear(&errorMessage)) {
        QMessageBox::critical(this,
                              "Clear BrickSuite Log",
                              QString("The application log could not be cleared.\n\n%1")
                                  .arg(errorMessage));
        return;
    }

    qInfo() << "Application log cleared by user.";

    refreshLog();
}

void LogViewerDialog::openLogFolder()
{
    const QString directoryPath = Logger::logDirectoryPath();

    if (directoryPath.isEmpty()
        || !QDesktopServices::openUrl(QUrl::fromLocalFile(directoryPath))) {
        QMessageBox::warning(this,
                             "BrickSuite Application Log",
                             "BrickSuite could not open the application log folder.");
    }
}
