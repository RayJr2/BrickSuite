#pragma once

#include "../../models/Part.h"

#include <QList>
#include <QSet>
#include <QString>

class BrickLinkCandidateDiscoveryService
{
public:
    static constexpr int MaximumCandidates = 20;

    enum class Status
    {
        Unsupported,
        NoCandidates,
        Candidates,
        TooBroad
    };

    struct Result
    {
        Status status = Status::Unsupported;
        QString brickLinkId;
        QString baseHint;
        QList<Part> candidates;
    };

    Result discover(const QString& brickLinkId) const;
    static QString baseHint(const QString& brickLinkId);
};

// Small value object used by Add Part to reject stale asynchronous results.
class BrickLinkCandidateLookupSession
{
public:
    void begin(const QString& brickLinkId, const QList<int>& candidatePartIds);
    void clear();
    bool accepts(const QString& currentInput, int partId) const;
    bool finish(int partId);
    bool isActive() const;
    QString brickLinkId() const;

private:
    QString m_brickLinkId;
    QSet<int> m_pendingPartIds;
};
