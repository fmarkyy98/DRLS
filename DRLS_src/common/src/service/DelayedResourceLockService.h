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
    DelayedResourceLockService(
            std::shared_ptr<IResourceLockService> resourceLockService,
            std::shared_ptr<common::AsyncTaskService> asyncTaskService);

    void addAsyncLock(CallerContext context,
                      QMap<LockableResource, ResourceLockType> resources,
                      AsyncTaskPtr task,
                      int timeoutMs,
                      AsyncTaskPtr timeoutTask = nullptr) override;

    void addAsyncSystemLock(QString tag,
                            QMap<LockableResource, ResourceLockType> resources,
                            AsyncTaskPtr task,
                            int timeoutMs,
                            AsyncTaskPtr timeoutTask = nullptr) override;

private slots:
    void onLocksChanged();

private:
    struct AsyncLock {
        std::variant<common::CallerContext, QString> contextOrTag_;
        QMap<common::LockableResource, common::ResourceLockType> resources_;
        AsyncTaskPtr task_;
        QObject* guard_;

        AsyncLock(common::CallerContext context,
                  QMap<common::LockableResource, common::ResourceLockType> resources,
                  AsyncTaskPtr task)
            : contextOrTag_(context)
            , resources_(resources)
            , task_(task)
            , guard_(new QObject())
        {}

        AsyncLock(QString tag,
                  QMap<common::LockableResource, common::ResourceLockType> resources,
                  AsyncTaskPtr task)
            : contextOrTag_(tag), resources_(resources), task_(task), guard_(new QObject())
        {}

        ~AsyncLock() { delete guard_; }
    };

private:
    void manageAddedAsyncLock(std::variant<common::CallerContext, QString> contextOrTag,
                              QMap<LockableResource, ResourceLockType> resources,
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

signals:
    void lastTaskEnded();
};

}  // namespace common
