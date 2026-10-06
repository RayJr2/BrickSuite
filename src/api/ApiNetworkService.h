/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 *
 * This file is part of BrickSuite.
 *
 * BrickSuite is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, version 3 of the License.
 *
 * BrickSuite is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with BrickSuite. If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include "ApiRequestContext.h"

#include <QObject>

class QByteArray;
class QNetworkAccessManager;
class QNetworkReply;
class QNetworkRequest;

class ApiNetworkService : public QObject
{
    Q_OBJECT

public:
    explicit ApiNetworkService(QObject* parent = nullptr);

    QNetworkReply* get(const QNetworkRequest& request,
                       const ApiRequestContext& context);

    QNetworkReply* post(const QNetworkRequest& request,
                        const QByteArray& body,
                        const ApiRequestContext& context);

private:
    void attachCompletionLogging(QNetworkReply* reply,
                                 const ApiRequestContext& context);

    QNetworkAccessManager* m_networkManager = nullptr;
};
