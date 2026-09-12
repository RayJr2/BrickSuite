#pragma once

#include "../../network/OperationalInvalidation.h"

#include <QHash>
#include <QObject>
#include <QSet>
#include <QTimer>
#include <functional>

class RemoteRefreshCoordinator : public QObject
{
    Q_OBJECT
public:
    enum class Projection {
        Workspaces,
        Storage,
        Inventory,
        InventoryLocations,
        Builds,
        BuildRequirements,
        MissingParts,
        Pulling,
        InventoryHistory,
        Collection,
        CollectionLocations,
        PartReferenceCustomizations
    };
    Q_ENUM(Projection)

    using Completion = std::function<void(bool)>;
    using Refresher = std::function<void(const OperationalInvalidation&, Completion)>;
    using Visibility = std::function<bool()>;

    explicit RemoteRefreshCoordinator(QObject* parent = nullptr);

    void registerProjection(Projection projection, Visibility visible, Refresher refresher);
    void receiveInvalidation(const OperationalInvalidation& invalidation);
    void surfaceBecameRelevant(Projection projection);
    void resetContext(bool connected, quint64 sessionGeneration,
                      quint64 workspaceGeneration);
    void setConnected(bool connected);
    void setOperationalAvailable(bool available);

    bool isDirty(Projection projection) const;
    bool isInFlight(Projection projection) const;

signals:
    void refreshStarted(RemoteRefreshCoordinator::Projection projection);

private:
    struct Registration {
        Visibility visible;
        Refresher refresher;
        bool dirty = false;
        bool inFlight = false;
        bool rerun = false;
        quint64 runContext = 0;
        OperationalInvalidation cause;
    };

    static QSet<Projection> projectionsFor(OperationalInvalidationDomain domain);
    static void mergeCause(OperationalInvalidation* target,
                           const OperationalInvalidation& source);
    void processPending();
    void start(Projection projection);

    QHash<Projection, Registration> m_registrations;
    QSet<Projection> m_pending;
    QTimer m_coalescingTimer;
    bool m_connected = false;
    bool m_operationalAvailable = true;
    quint64 m_sessionGeneration = 0;
    quint64 m_workspaceGeneration = 0;
    quint64 m_contextSerial = 0;
};

Q_DECLARE_METATYPE(RemoteRefreshCoordinator::Projection)
