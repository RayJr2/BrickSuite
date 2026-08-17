#include "LogViewerDialog.h"

#include "../../services/Logger.h"
#include "../../settings/UserSettings.h"

#include <QCloseEvent>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFile>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QTextCursor>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

LogViewerDialog::LogViewerDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("BrickSuite Application Log");
    resize(950, 650);

    const QByteArray savedGeometry = UserSettings::instance().logViewerGeometry();

    if (!savedGeometry.isEmpty()) {
        restoreGeometry(savedGeometry);
    }

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

    //
    // QDialog::reject()/done() does not necessarily route through closeEvent().
    // Save geometry whenever the dialog finishes so the Close button, Esc key,
    // and window close button all persist the viewer position and size.
    //
    connect(this, &QDialog::finished, this, [this](int) {
        UserSettings::instance().setLogViewerGeometry(saveGeometry());
    });

    //
    // Refresh automatically so the viewer can remain open on another screen
    // while BrickSuite is being exercised. One second is frequent enough for
    // testing without generating any additional log traffic.
    //
    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(1000);

    connect(m_refreshTimer, &QTimer::timeout, this, [this]() {
        refreshLog();
    });

    m_refreshTimer->start();

    refreshLog();
}

void LogViewerDialog::closeEvent(QCloseEvent* event)
{
    //
    // Keep the Log Viewer where the user placed it, including another
    // monitor, so diagnostic testing can resume without repositioning it.
    //
    UserSettings::instance().setLogViewerGeometry(saveGeometry());

    QDialog::closeEvent(event);
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

    const QString logText = QString::fromUtf8(file.readAll());

    //
    // If the user is watching the bottom of the log, continue following new
    // entries. If they have scrolled upward to inspect older entries, preserve
    // that position instead of snapping them back to the bottom every second.
    //
    QScrollBar* scrollBar = m_logEdit->verticalScrollBar();
    const bool wasAtBottom = scrollBar->value() >= scrollBar->maximum();
    const int previousScrollValue = scrollBar->value();

    if (m_logEdit->toPlainText() == logText)
        return;

    m_logEdit->setPlainText(logText);

    if (wasAtBottom) {
        QTextCursor cursor = m_logEdit->textCursor();
        cursor.movePosition(QTextCursor::End);
        m_logEdit->setTextCursor(cursor);
        scrollBar->setValue(scrollBar->maximum());
    } else {
        scrollBar->setValue(qMin(previousScrollValue, scrollBar->maximum()));
    }
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
