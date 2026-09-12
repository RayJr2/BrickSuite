#pragma once
#include <QObject>
#include <memory>
class BrickSuiteOperationDispatcher;
class HostReadExecutor;
class HostReadProtocolService : public QObject
{
    Q_OBJECT
public:
    explicit HostReadProtocolService(const QString& databasePath, QObject* parent = nullptr);
    ~HostReadProtocolService() override;
    void registerOperations(BrickSuiteOperationDispatcher& dispatcher);
    bool isAvailable() const;
    HostReadExecutor& executor();
private:
    std::unique_ptr<HostReadExecutor> m_executor;
};
