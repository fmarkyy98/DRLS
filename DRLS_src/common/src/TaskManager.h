#pragma once

#include <QObject>
#include <memory>
#include <mutex>

#include "common/src/TasksUpdatedSignalProxy.h"

namespace common {

class AsyncTaskService;
class AsyncTask;

struct WaitOnExitEnabled {};
struct WaitOnExitDisabled {};

struct CancellableOnly {};
struct WaitOnExitEnabledRoot {};
struct WaitOnExitEnabledChild {};

enum class ManagedTaskBehaviour { WaitOnExit, CancelOnExit };

class AbstractTaskManager {
public:
    AbstractTaskManager(std::shared_ptr<common::AsyncTaskService> asyncTaskService);
    virtual ~AbstractTaskManager();

    void removeManagedTask(std::weak_ptr<common::AsyncTask> task);
    bool hasPendingManagedTask() const;
    void prepareForDeletion();
    std::shared_ptr<AsyncTask> createNoOpTask();

    AbstractTaskManager* parentTaskManager() { return parent_; }
    void setParentTaskManager(AbstractTaskManager* parent) { parent_ = parent; }
    void addChildTaskManager(AbstractTaskManager* child) { children_.append(child); }
    void removeChildTaskManager(AbstractTaskManager* child) { children_.removeAll(child); }
    const QList<AbstractTaskManager*> children() { return children_; }

    TasksUpdatedSignalProxy* tasksUpdated();

protected:
    std::shared_ptr<common::AsyncTaskService> asyncTaskService;

    virtual void abstract() = 0;

    void addManagedTaskBase(std::weak_ptr<common::AsyncTask> task,
                            bool removeFromManagerAfterExecution,
                            ManagedTaskBehaviour behaviour = ManagedTaskBehaviour::WaitOnExit);

private:
    mutable std::recursive_mutex managedTaskMutex_;

    std::mutex deletingMutex_;
    bool deleting_;
    AbstractTaskManager* parent_;
    QList<AbstractTaskManager*> children_;
    TasksUpdatedSignalProxy* tasksUpdatedSignalProxy_;

    std::map<std::weak_ptr<common::AsyncTask>,
             ManagedTaskBehaviour,
             std::owner_less<std::weak_ptr<common::AsyncTask>>>
            managedTasks_;

    std::map<std::weak_ptr<common::AsyncTask>,
             QMetaObject::Connection,
             std::owner_less<std::weak_ptr<common::AsyncTask>>>
            destroyedConnections_;

    std::map<std::weak_ptr<common::AsyncTask>,
             std::weak_ptr<QObject>,
             std::owner_less<std::weak_ptr<common::AsyncTask>>>
            taskGuards_;
};

template<typename TypeT>
class TaggedTaskManager;

template<>
class TaggedTaskManager<WaitOnExitDisabled> : public AbstractTaskManager {
public:
    TaggedTaskManager(std::shared_ptr<common::AsyncTaskService> asyncTaskService);

    virtual ~TaggedTaskManager() = default;
};

template<>
class TaggedTaskManager<WaitOnExitEnabled>
    : public TaggedTaskManager<WaitOnExitDisabled>
{
public:
    TaggedTaskManager(std::shared_ptr<common::AsyncTaskService> asyncTaskService);

    virtual ~TaggedTaskManager() = default;
};

template<typename TaskManagerTypeT>
class TaskManager;

template<>
class TaskManager<WaitOnExitEnabledRoot>
    : public TaggedTaskManager<WaitOnExitEnabled>
{
public:
    TaskManager(std::shared_ptr<common::AsyncTaskService> asyncTaskService);

    ~TaskManager();

    template<ManagedTaskBehaviour behaviour>
    void addManagedTask(std::weak_ptr<common::AsyncTask> task,
                        bool removeFromManagerAfterExecution)
    {
        addManagedTaskBase(task, removeFromManagerAfterExecution, behaviour);
    }

private:
    using AbstractTaskManager::addManagedTaskBase;
    void abstract() override;
};

template<>
class TaskManager<CancellableOnly>
    : public TaggedTaskManager<WaitOnExitDisabled>
{
public:
    TaskManager(std::shared_ptr<common::AsyncTaskService> asyncTaskService);

    template<ManagedTaskBehaviour behaviour>
    void addManagedTask(std::weak_ptr<common::AsyncTask> task,
                        bool removeFromManagerAfterExecution)
    {
        static_assert(behaviour == ManagedTaskBehaviour::CancelOnExit,
                      "Only cancellable tasks are supported in this task manager");

        addManagedTaskBase(task, removeFromManagerAfterExecution, behaviour);
    }

private:
    using AbstractTaskManager::addManagedTaskBase;
    void abstract() override;
};

template<>
class TaskManager<WaitOnExitEnabledChild>
    : public TaggedTaskManager<WaitOnExitEnabled>
{
public:
    TaskManager(TaggedTaskManager<WaitOnExitEnabled>* rootTaskManager,
                std::shared_ptr<common::AsyncTaskService> asyncTaskService);

    ~TaskManager();

    template<ManagedTaskBehaviour behaviour>
    void addManagedTask(std::weak_ptr<common::AsyncTask> task,
                        bool removeFromManagerAfterExecution)
    {
        addManagedTaskBase(task, removeFromManagerAfterExecution, behaviour);
    }

private:
    using AbstractTaskManager::addManagedTaskBase;
    void abstract() override;
};

}  // namespace common
