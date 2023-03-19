#include "DelayedResourceLockService.h"

#include "utils/Finally.h"

using namespace common;

DelayedResourceLockService::DelayedResourceLockService(
        std::shared_ptr<IResourceLockService> resourceLockService,
        std::shared_ptr<AsyncTaskService> asyncTaskService)
    : TaskManager<CancellableOnly>(asyncTaskService)
    , resourceLockService_(resourceLockService)
    , asyncTaskService_(asyncTaskService)
{
    connect(resourceLockService_->meta(),
            &IResourceLockService::Meta::locksChanged,
            this,
            [this] {
                if (!inProgress_) {
                    bool isEmpty;
                    {
                        auto lock = std::lock_guard(asyncLocksMutex_);
                        isEmpty   = asyncLocks_.isEmpty();
                    }

                    if (!isEmpty)
                        onLocksChanged();

                } else {
                    hasMissedSignal_ = true;
                }
            });
}

// clang-format off
void DelayedResourceLockService::onLocksChanged() {
    inProgress_ = true;

    auto releaseCallback = [this](auto it){
        if (std::holds_alternative<common::CallerContext>((*it)->contextOrTag_)) {
            resourceLockService_
            ->releaseLocks((*it)->resources_,
                           std::get<common::CallerContext>((*it)->contextOrTag_))
            ->runSync();
        } else {
            resourceLockService_
            ->releaseSystemLocks((*it)->resources_,
                                 std::get<QString>((*it)->contextOrTag_))
            ->runSync();
        }
    };

    auto onResourceAvailableCallback = [this, releaseCallback](auto& it, bool result) {
        if (result) {
            (*it)->task_
            ->onEnded([it, releaseCallback](auto /*result*/, bool /*success*/) {
                releaseCallback(it);
            },
            false)
            ->runSync();
            it = asyncLocks_.erase(it);
        } else {
            ++it;
        }
    };

    auto onFailedCallback = [this](auto task, auto& it) {
        qWarning() << logOnFailure(task);
        // In the failing case we don't want the lock,
        // the failure won't resolv itself so get rid of it.
        it = asyncLocks_.erase(it);
    };

    asyncTaskService_
    ->createTask([this, onResourceAvailableCallback, onFailedCallback](auto /*task*/) {
        auto finInProgress= util::finally([this] { inProgress_ = false; });

        auto finHasMissedSignal = util::finally([this] {
            if (hasMissedSignal_)
                onLocksChanged();
        });

        hasMissedSignal_ = false;

        auto lock = std::lock_guard(asyncLocksMutex_);

        for (auto it = asyncLocks_.begin(); it != asyncLocks_.end();) {
            if (std::holds_alternative<common::CallerContext>((*it)->contextOrTag_)) {
                resourceLockService_
                ->acquireLocks((*it)->resources_,
                               std::get<common::CallerContext>((*it)->contextOrTag_))
                ->onResultAvailable([&it, onResourceAvailableCallback](bool result) {
                   onResourceAvailableCallback(it, result);
                },
                false)
                ->onFailed([&it, onFailedCallback](auto task) { // Exception from aquireLocks()
                    onFailedCallback(task, it);
                },
                false)
                ->runSync();
            } else {
                resourceLockService_
                ->acquireSystemLocks((*it)->resources_,
                               std::get<QString>((*it)->contextOrTag_))
                ->onResultAvailable([&it, onResourceAvailableCallback](bool result) {
                    onResourceAvailableCallback(it, result);
                },
                false)
                ->onFailed([&it, onFailedCallback](auto task) { // Exception from aquireLocks()
                    onFailedCallback(task, it);
                },
                false)
                ->runSync();
            }
        }
        if (asyncLocks_.isEmpty())
            emit lastTaskEnded();
    })
    ->run<ManagedTaskBehaviour::CancelOnExit>(this);
}
// clang-format on

void DelayedResourceLockService::addAsyncLock(
        CallerContext context,
        std::map<LockableResource, ResourceLockType> resources,
        AsyncTaskPtr task,
        int timeoutMs,
        AsyncTaskPtr timeoutTask)
{
    manageAddedAsyncLock(context, resources, task, timeoutMs, timeoutTask);
}

void DelayedResourceLockService::addAsyncSystemLock(
        QString tag,
        std::map<LockableResource, ResourceLockType> resources,
        AsyncTaskPtr task,
        int timeoutMs,
        AsyncTaskPtr timeoutTask)
{
    manageAddedAsyncLock(tag, resources, task, timeoutMs, timeoutTask);
}

void DelayedResourceLockService::manageAddedAsyncLock(
        std::variant<common::CallerContext, QString> contextOrTag,
        std::map<LockableResource, ResourceLockType> resources,
        AsyncTaskPtr task,
        int timeoutMs,
        AsyncTaskPtr timeoutTask)
{
    std::shared_ptr<AsyncLock> asyncLock;
    if (std::holds_alternative<common::CallerContext>(contextOrTag)) {
        asyncLock = std::make_shared<AsyncLock>(std::get<common::CallerContext>(contextOrTag),
                                                resources,
                                                task);
    } else {
        asyncLock =
                std::make_shared<AsyncLock>(std::get<QString>(contextOrTag), resources, task);
    }

    auto release = [this, task, resources, contextOrTag, asyncLock] {
        if (!task->isRunning()) {
            task->terminate();
            {
                auto lock = std::lock_guard(asyncLocksMutex_);
                asyncLocks_.removeOne(asyncLock);
                if (asyncLocks_.isEmpty())
                    emit lastTaskEnded();
            }
            if (std::holds_alternative<common::CallerContext>(contextOrTag)) {
                resourceLockService_
                        ->releaseLocks(resources, std::get<common::CallerContext>(contextOrTag))
                        ->runSync();
            } else {
                resourceLockService_
                        ->releaseSystemLocks(resources, std::get<QString>(contextOrTag))
                        ->runSync();
            }
        }
    };

    auto onResultAvailableCallback =
            [this, task, timeoutMs, asyncLock, release, timeoutTask](bool result)
    {
                if (result) {
                    task->onEnded([release](auto, bool) { release(); }, false)->runUnmanaged();
                } else {
                    {
                        auto lock = std::lock_guard(asyncLocksMutex_);
                        asyncLocks_.append(asyncLock);
                    }

                    QTimer::singleShot(timeoutMs, asyncLock->guard_, [release, timeoutTask] {
                        release();
                        if (timeoutTask != nullptr)
                            timeoutTask->runUnmanaged();
                    });
                }
            };

    if (std::holds_alternative<common::CallerContext>(contextOrTag)) {
        resourceLockService_
                ->acquireLocks(resources, std::get<common::CallerContext>(contextOrTag))
                ->onResultAvailable(onResultAvailableCallback)
                ->onFailed([this](auto task) {  // Exception from aquireLocks()
                    qWarning() << logOnFailure(task);
                })
                ->run<ManagedTaskBehaviour::CancelOnExit>(this);
    } else {
        resourceLockService_->acquireSystemLocks(resources, std::get<QString>(contextOrTag))
                ->onResultAvailable(onResultAvailableCallback)
                ->onFailed([this](auto task) {  // Exception from aquireLocks()
                    qWarning() << logOnFailure(task);
                })
                ->run<ManagedTaskBehaviour::CancelOnExit>(this);
    }
}

QString DelayedResourceLockService::logOnFailure(AsyncTaskPtr task) {
    QString message;
    auto exception = task->getStoredException();
    try {
        if (exception != nullptr)
            std::rethrow_exception(exception);
    } catch (const std::exception& e) {
        message = e.what();
    }

    return "An Exception has been thrown while executing acquireLocks()\n" + message;
}
