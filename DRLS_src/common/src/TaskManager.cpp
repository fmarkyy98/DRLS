#include "TaskManager.h"

#include <QDebug>
#include "common/src/service/AsyncTaskService.h"

using namespace common;

AbstractTaskManager::AbstractTaskManager(std::shared_ptr<AsyncTaskService> asyncTaskService)
    : asyncTaskService(asyncTaskService), deleting_(false), parent_(nullptr)
{
    tasksUpdatedSignalProxy_ = new TasksUpdatedSignalProxy();
    //    qDebug() << "AbstractTaskManager created:  "<< reinterpret_cast<intptr_t>(this);
}

AbstractTaskManager::~AbstractTaskManager() {
    prepareForDeletion();

    tasksUpdatedSignalProxy_->deleteLater();

    decltype(managedTasks_) copyOfManagedTasks;
    {
        std::lock_guard<std::recursive_mutex> lock(managedTaskMutex_);
        //        qDebug() << "~AbstractTaskManager:  "<< reinterpret_cast<intptr_t>(this)
        //                 << "task count:" << managedTasks_.size();

        copyOfManagedTasks = managedTasks_;
    }

    for (auto& taskPair : copyOfManagedTasks) {
        auto task = taskPair.first.lock();
        if (taskPair.second == ManagedTaskBehaviour::CancelOnExit && task != nullptr) {
            //            qDebug() << "TaskManager is canceling task: "
            //                     << reinterpret_cast<intptr_t>(task.get());
            task->terminate();
        } else if (task != nullptr) {
            qWarning() << "TaskManager destructed with non-cancellable tasks in progress";
        }

        QObject::disconnect(destroyedConnections_[taskPair.first]);

        std::lock_guard<std::recursive_mutex> lock(managedTaskMutex_);
        auto taskGuardIt = taskGuards_.find(task);
        if (taskGuardIt != taskGuards_.end()) {
            auto guard = taskGuardIt->second.lock();
            if (guard != nullptr) {
                guard->disconnect();
                task->removeMaintainedObject(guard);
            }
        }
    }

    //    qDebug() << "~AbstractTaskManager destroyed:  "<< reinterpret_cast<intptr_t>(this);
}

TasksUpdatedSignalProxy* AbstractTaskManager::tasksUpdated() {
    return tasksUpdatedSignalProxy_;
}

void AbstractTaskManager::addManagedTaskBase(std::weak_ptr<AsyncTask> task,
                                             bool removeFromManagerAfterExecution,
                                             ManagedTaskBehaviour behaviour)
{
    auto taskPtr = task.lock();
    if (taskPtr == nullptr)
        return;

    {
        std::lock_guard<std::mutex> lock(deletingMutex_);
        if (deleting_) {
            qWarning() << "AbstractTaskManager: Task "
                       << reinterpret_cast<intptr_t>(taskPtr.get())
                       << "was not added because manager is already being destroyed.";
            taskPtr->terminate();
            return;
        }
    }

    bool hadTask = true;
    {
        std::lock_guard<std::recursive_mutex> taskLock(managedTaskMutex_);
        auto managedTasksIt = managedTasks_.find(task);
        if (managedTasksIt != managedTasks_.end()) {
            qWarning() << "AbstractTaskManager: Task "
                       << reinterpret_cast<intptr_t>(taskPtr.get())
                       << "is already managed by this manager.";
            return;
        }

        hadTask = hasPendingManagedTask();

        auto conn = QObject::connect(task.lock().get(), &QObject::destroyed, [this, task]() {
            this->removeManagedTask(task);
        });

        this->destroyedConnections_.insert({task, conn});
        this->managedTasks_.insert({task, behaviour});
        //    qDebug() << "AbstractTaskManager: Task " <<
        //    reinterpret_cast<intptr_t>(taskPtr.get())
        //               << "was added to manager" << reinterpret_cast<intptr_t>(this);

        if (removeFromManagerAfterExecution) {
            auto guard = std::make_shared<QObject>();
            QObject::connect(guard.get(), &QObject::destroyed, [this, taskPtr]() {
                this->removeManagedTask(taskPtr);
            });
            taskPtr->addMaintainedObject(guard);
            this->taskGuards_.insert({task, guard});
        }
    }

    if (!hadTask)
        emit tasksUpdatedSignalProxy_->tasksUpdated();
}

void AbstractTaskManager::removeManagedTask(std::weak_ptr<AsyncTask> task) {
    {
        std::lock_guard<std::mutex> lock(deletingMutex_);
        if (deleting_)
            return;
    }

    bool hadTask = true;
    auto t    = task.lock();
    {
        std::lock_guard<std::recursive_mutex> lock(managedTaskMutex_);

        auto managedTasksIt = managedTasks_.find(task);
        if (managedTasksIt != managedTasks_.end()) {
            //            qDebug() << "AbstractTaskManager: Task " <<
            //            reinterpret_cast<intptr_t>(t.get())
            //                       << "was removed from manager" <<
            //                       reinterpret_cast<intptr_t>(this);

            managedTasks_.erase(managedTasksIt);
            hadTask = hasPendingManagedTask();
        }

        auto destroyedConnectionsIt = destroyedConnections_.find(task);
        if (destroyedConnectionsIt != destroyedConnections_.end()) {
            QObject::disconnect(destroyedConnectionsIt->second);
            destroyedConnections_.erase(destroyedConnectionsIt);
        }

        auto taskGuardIt = taskGuards_.find(task);
        if (taskGuardIt != taskGuards_.end()) {
            auto guard = taskGuardIt->second.lock();
            if (guard != nullptr) {
                guard->disconnect();
                if (t != nullptr)
                    t->removeMaintainedObject(guard);
            }

            taskGuards_.erase(taskGuardIt);
        }
    }

    if (!hadTask)
        emit tasksUpdatedSignalProxy_->tasksUpdated();
}

bool AbstractTaskManager::hasPendingManagedTask() const {
    std::lock_guard<std::recursive_mutex> lock(managedTaskMutex_);

    for (auto& behaviourPair : managedTasks_)
        if (behaviourPair.second == ManagedTaskBehaviour::WaitOnExit)
            return true;

    for (auto child : children_)
        if (child->hasPendingManagedTask())
            return true;

    return false;
}

void AbstractTaskManager::prepareForDeletion() {
    for (auto child : children_)
        child->prepareForDeletion();

    std::lock_guard<std::mutex> lock(deletingMutex_);

    if (!deleting_)
        deleting_ = true;
}

std::shared_ptr<AsyncTask> AbstractTaskManager::createNoOpTask() {
    return asyncTaskService->createNoOpTask();
}

TaskManager<WaitOnExitEnabledRoot>::TaskManager(
        std::shared_ptr<AsyncTaskService> asyncTaskService)
    : TaggedTaskManager(asyncTaskService)
{
}

TaskManager<WaitOnExitEnabledRoot>::~TaskManager() {
    for (auto child : children())
        child->setParentTaskManager(nullptr);
}

TaskManager<CancellableOnly>::TaskManager(std::shared_ptr<AsyncTaskService> asyncTaskService)
    : TaggedTaskManager(asyncTaskService)
{
}

TaskManager<WaitOnExitEnabledChild>::TaskManager(
        TaggedTaskManager<WaitOnExitEnabled>* rootTaskManager,
        std::shared_ptr<AsyncTaskService> asyncTaskService)
    : TaggedTaskManager(asyncTaskService)
{
    setParentTaskManager(rootTaskManager);
    parentTaskManager()->addChildTaskManager(this);

    QObject::connect(tasksUpdated(),
                     &TasksUpdatedSignalProxy::tasksUpdated,
                     rootTaskManager->tasksUpdated(),
                     &TasksUpdatedSignalProxy::tasksUpdated);
}

TaskManager<WaitOnExitEnabledChild>::~TaskManager() {
    for (auto child : children())
        child->setParentTaskManager(nullptr);

    if (parentTaskManager() != nullptr)
        parentTaskManager()->removeChildTaskManager(this);
}

void TaskManager<WaitOnExitEnabledRoot>::abstract() {
}

void TaskManager<CancellableOnly>::abstract() {
}

void TaskManager<WaitOnExitEnabledChild>::abstract() {
}

TaggedTaskManager<WaitOnExitDisabled>::TaggedTaskManager(
        std::shared_ptr<AsyncTaskService> asyncTaskService)
    : AbstractTaskManager(asyncTaskService)
{
}

TaggedTaskManager<WaitOnExitEnabled>::TaggedTaskManager(
        std::shared_ptr<AsyncTaskService> asyncTaskService)
    : TaggedTaskManager<WaitOnExitDisabled>(asyncTaskService)
{
}
