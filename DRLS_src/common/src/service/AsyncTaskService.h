#pragma once

#include <QtCore>
#include <QHash>

#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include <typeindex>

#include "common/src/AsyncTask.h"

namespace std {

template<typename T>
inline uint qHash(const std::shared_ptr<T>& p, uint seed = 0) {
    return ::qHash(p.get(), seed);
}

template<typename T>
inline uint qHash(const std::unique_ptr<T>& p, uint seed = 0) {
    return ::qHash(p.get(), seed);
}

template<typename T>
inline uint qHash(const std::weak_ptr<T>& p, uint seed = 0) {
    return ::qHash(p.lock().get(), seed);
}

inline uint qHash(const std::type_info& t, uint seed = 0) {
    return t.hash_code();
}

inline uint qHash(const std::type_index& t, uint seed = 0) {
    return t.hash_code();
}

}  // namespace std

namespace common {

class AsyncTaskService
    : public QObject
    , public std::enable_shared_from_this<AsyncTaskService>
{
    Q_OBJECT
public:
    static std::shared_ptr<AsyncTaskService> getInstance();

private:
    friend class AsyncTask;

    class AsyncRunnable : public QRunnable {
        friend class AsyncTaskService;

        std::function<void()> function;

        AsyncRunnable(std::function<void()> function);
        void run() override;
    };

private:
    QThreadPool* pool;
    QThread* mainThread;
    std::set<std::weak_ptr<common::AsyncTask>,
             std::owner_less<std::weak_ptr<common::AsyncTask>>>
            tasks;

    QSet<std::shared_ptr<AsyncTask>> runningTasks;
    mutable std::recursive_mutex taskMutex;
    std::mutex poolMutex;
    bool runOnMainThreadEnabled = false;

    bool addTask(std::shared_ptr<AsyncTask> task);
    void runTask(std::shared_ptr<AsyncTask> task,
                 bool async,
                 std::optional<AsyncTask::Priority> priority);
    void handleSubtaskTimeout(QList<std::shared_ptr<AsyncTask>> tasks, QObject* parent);
    bool checkOrigin(QList<std::shared_ptr<AsyncTask>> tasks);
    void modifyWorkersCount(int diff);
    void doAsynchronously(std::function<void()> function, bool onExtraThread = false);
    bool initSubtasks(std::shared_ptr<AsyncTask> self,
                      const QList<std::shared_ptr<AsyncTask>>& tasks,
                      const QString compositeType);

protected:
    virtual void emitExceptionSignals(std::shared_ptr<AsyncTask> task);

public:
    class ExtraThreadLock {
        std::shared_ptr<AsyncTaskService> service;

    public:
        ExtraThreadLock(std::shared_ptr<AsyncTaskService> service);
        ~ExtraThreadLock();

        ExtraThreadLock(const ExtraThreadLock&) = delete;
        ExtraThreadLock(ExtraThreadLock&&)      = delete;
        ExtraThreadLock& operator=(const ExtraThreadLock&) = delete;
        ExtraThreadLock& operator=(ExtraThreadLock&&) = delete;
    };

    AsyncTaskService();
    ~AsyncTaskService();

    std::shared_ptr<common::AsyncTask> createNoOpTask() {
        auto taskPtr = new AsyncTask(shared_from_this(), nullptr);
        auto task    = std::shared_ptr<AsyncTask>(taskPtr);
        addTask(task);
        return task;
    }

    std::shared_ptr<common::AsyncTask> createTask(
            std::function<void(std::shared_ptr<common::AsyncTask>)> function)
    {
        return createTask<true>([function](std::shared_ptr<common::AsyncTask> task) {
            function(task);
            return true;
        });
    }

    template<bool>
    std::shared_ptr<common::AsyncTask> createTask(
            std::function<bool(std::shared_ptr<common::AsyncTask>)> function)
    {
        auto taskPtr = new AsyncTask(shared_from_this(), function);
        auto task    = std::shared_ptr<AsyncTask>(taskPtr);
        addTask(task);
        return task;
    }

    template<typename T>
    std::shared_ptr<AsyncFunction<T>> createFunction(
            std::function<void(std::shared_ptr<AsyncFunction<T>>)> function)
    {
        return createFunction<T, true>([function](std::shared_ptr<AsyncFunction<T>> fun) {
            function(fun);
            return true;
        });
    }

    template<typename T, bool>
    std::shared_ptr<AsyncFunction<T>> createFunction(
            std::function<bool(std::shared_ptr<AsyncFunction<T>>)> function)
    {
        auto funPtr = new AsyncFunction<T>(shared_from_this(),
                                           [function](std::shared_ptr<AsyncTask> task) {
                                               return function(task->asFunction<T>());
                                           });
        auto fun    = std::shared_ptr<AsyncFunction<T>>(funPtr);
        addTask(fun);
        return fun;
    }

    std::shared_ptr<AsyncTask> createSequence(QList<std::shared_ptr<AsyncTask>> tasks);
    std::shared_ptr<AsyncTask> createFallback(QList<std::shared_ptr<AsyncTask>> tasks);
    std::shared_ptr<AsyncTask> createParallel(QList<std::shared_ptr<AsyncTask>> tasks);
    std::shared_ptr<AsyncTask> createAttempt(QList<std::shared_ptr<AsyncTask>> tasks);

    QThread* getMainThread() const;
    void setMainThread(QThread* value);

    int getNumberOfAllExecutors() const;
    int getNumberOfIdleExecutors() const;
    int getNumberOfRegisteredTasks() const;
    int getNumberOfRunningTasks() const;

    void setRunOnMainThreadEnabled(bool enabled);
    bool getRunOnMainThreadEnabled() const;

public slots:
    void runTask(std::shared_ptr<AsyncTask> task,
                 std::optional<common::AsyncTask::Priority> priority = std::nullopt);
    void runTaskSync(std::shared_ptr<AsyncTask> task,
                     bool rethrowException                                = false,
                     std::optional<common::AsyncTask::Priority> priority = std::nullopt);
    void cancelTask(std::shared_ptr<AsyncTask> task);
    void terminateTask(std::shared_ptr<AsyncTask> task);
    bool deleteTask(std::shared_ptr<AsyncTask> task);
    void deleteAllTasks();

signals:
    void numberOfTasksChanged(int registered, int running);

private:
    static std::shared_ptr<AsyncTaskService> instance_;
};

}  // namespace common
