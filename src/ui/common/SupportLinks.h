#pragma once

#include "../../core/AppConstants.h"
#include <QAction>
#include <QDesktopServices>
#include <QMenu>
#include <QMessageBox>
#include <QUrl>

namespace SupportLinks {

using UrlOpener = bool (*)(const QUrl&);

inline void openSupportPage(QWidget* parent,
                            UrlOpener opener = &QDesktopServices::openUrl)
{
    // No caller-supplied address, user data, or tracking parameters.
    const QUrl url(QString::fromLatin1(AppConstants::SupportUrl));
    if (opener(url))
        return;

    QMessageBox warning(QMessageBox::Warning, QObject::tr("Support BrickSuite"),
        QObject::tr("BrickSuite could not open the PayPal support page in your web browser.\n\n"
                    "You can open this address manually:\n\n%1").arg(url.toString()),
        QMessageBox::Ok, parent);
    warning.setTextFormat(Qt::PlainText);
    warning.setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    warning.exec();
}

inline QAction* addHelpAction(QMenu* helpMenu, QWidget* parent)
{
    auto* action = helpMenu->addAction(QObject::tr("Support BrickSuite…"));
    action->setObjectName(QStringLiteral("supportBrickSuiteAction"));
    action->setStatusTip(QObject::tr("Open the voluntary PayPal support page in your web browser."));
    QObject::connect(action, &QAction::triggered, parent, [parent] { openSupportPage(parent); });
    return action;
}

} // namespace SupportLinks
