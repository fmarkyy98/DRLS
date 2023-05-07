#pragma once

#include <variant>

#include "interface/IDelayedResourceLockService.h"
#include "interface/IResourceLockService.h"
#include "AsyncTaskService.h"
#include "common/src/TaskManager.h"

namespace test {
class DelayedResourceLockServiceTest;
}
namespace common {

class DelayedResourceLockService
    : public QObject
    , public common::IDelayedResourceLockService
    , public common::TaskManager<common::CancellableOnly>
{
    friend class test::DelayedResourceLockServiceTest;
    Q_OBJECT

public:
    static std::shared_ptr<DelayedResourceLockService> getInstance();

private:
    DelayedResourceLockService(
            std::shared_ptr<IResourceLockService> resourceLockService,
            std::shared_ptr<common::AsyncTaskService> asyncTaskService);

public:
    void addAsyncLock(CallerContext context,
                      std::map<LockableResource, ResourceLockType> resources,
                      AsyncTaskPtr task,
                      int timeoutMs,
                      AsyncTaskPtr timeoutTask = nullptr) override;

    void addAsyncSystemLock(QString tag,
                            std::map<LockableResource, ResourceLockType> resources,
                            AsyncTaskPtr task,
                            int timeoutMs,
                            AsyncTaskPtr timeoutTask = nullptr) override;

signals:
    void lastTaskEnded();

private slots:
    void onLocksChanged();

private:
    struct AsyncLock {
        std::variant<common::CallerContext, QString> contextOrTag_;
        std::map<common::LockableResource, common::ResourceLockType> resources_;
        AsyncTaskPtr task_;
        std::unique_ptr<QObject> guard_ = std::make_unique<QObject>();

        AsyncLock(common::CallerContext context,
                  std::map<common::LockableResource, common::ResourceLockType> resources,
                  AsyncTaskPtr task)
            : contextOrTag_(context)
            , resources_(resources)
            , task_(task)
        {}

        AsyncLock(QString tag,
                  std::map<common::LockableResource, common::ResourceLockType> resources,
                  AsyncTaskPtr task)
            : contextOrTag_(tag)
            , resources_(resources)
            , task_(task)
        {}
    };

private:
    void manageAddedAsyncLock(std::variant<common::CallerContext, QString> contextOrTag,
                              std::map<LockableResource, ResourceLockType> resources,
                              AsyncTaskPtr task,
                              int timeoutMs,
                              AsyncTaskPtr timeoutTask);
    QString logOnFailure(AsyncTaskPtr task);

private:
    std::shared_ptr<common::IResourceLockService> resourceLockService_;
    std::shared_ptr<common::AsyncTaskService> asyncTaskService_;

    QList<std::shared_ptr<AsyncLock>> asyncLocks_;
    std::mutex asyncLocksMutex_;

    std::atomic_bool inProgress_      = false;
    std::atomic_bool hasMissedSignal_ = false;

private:
    static std::shared_ptr<DelayedResourceLockService> instance_;
};

}  // namespace common
