#pragma once

#include <QtCore>
#include <atomic>
#include <condition_variable>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>

#include "common/src/Delegate.h"
#include "common/src/TaskManager.h"

namespace  util {
class ContextReceiver;
class ContextDispatcher;
}

namespace common {
class BasicSignalConnection;
class AsyncTaskService;

template<typename TResult>
class AsyncFunction;

using AsyncTaskPtr = std::shared_ptr<common::AsyncTask>;

template<typename Ret>
using AsyncFuncPtr = std::shared_ptr<common::AsyncFunction<Ret>>;

class AsyncTask
    : public QObject
    , public std::enable_shared_from_this<AsyncTask>
{
    Q_OBJECT

    friend class AsyncTaskService;

public:
    enum class State {
        NotStarted,
        Terminated,

        Starting,
        Running,

        Finishing,
        Finished,

        Failing,
        Failed,

        TimingOut,
        TimedOut,

        Cancelling,
        Canceled
    };

    enum class Priority { Normal = 0, AboveNormal = 1, High = 10, BelowNormal = -1, Low = -10 };

    static QString priorityToString(const Priority& priority);

    static QString stateToString(State state) {
        switch (state) {
        case State::NotStarted:
            return "NotStarted";
        case State::Terminated:
            return "Terminated";
        case State::Starting:
            return "Starting";
        case State::Running:
            return "Running";
        case State::Finishing:
            return "Finishing";
        case State::Finished:
            return "Finished";
        case State::Failing:
            return "Failing";
        case State::Failed:
            return "Failed";
        case State::TimingOut:
            return "TimingOut";
        case State::TimedOut:
            return "TimedOut";
        case State::Cancelling:
            return "Cancelling";
        case State::Canceled:
            return "Canceled";
        default:
            return "Unknown";
        }
    }

    AsyncTask(std::shared_ptr<AsyncTaskService> taskService,
              std::function<bool(std::shared_ptr<AsyncTask>)> function);

    virtual void initialize();
    bool isInitialized() const;

private:
    static std::recursive_mutex aliveTasksCountMutex;
    static int aliveTasksCount;
    static std::mutex priorityMapMutex;
    static QMap<QThread*, Priority> priorityMap;

    std::shared_ptr<AsyncTaskService> taskService;
    std::function<bool(std::shared_ptr<AsyncTask>)> function;
    QList<std::weak_ptr<AsyncTask>> subtasks = {};

    bool initialized = false;
    volatile State state;
    mutable std::recursive_mutex stateMutex;
    mutable std::recursive_mutex signalHandlerMutex;
    bool autoRemove = true;
    std::atomic_bool waitingForDeletion;
    mutable QList<std::shared_ptr<std::condition_variable>> noPendingHandlerCondVars;
    mutable std::mutex noPendingHandlerCondVarsMutex;

    int timeoutMs                = -1;
    int progress                 = 0;
    std::exception_ptr exception = nullptr;
    std::mutex maintainedMutex;
    QMap<std::shared_ptr<QObject>, bool> maintainedObjects;
    QList<State> allowedHandlers = {};

#ifdef DEBUG_SAVE_STACKTRACE
    QStringList constructedFrom;
#endif

protected:
    QList<std::shared_ptr<BasicSignalConnection>> startedHandlers;
    QList<std::shared_ptr<BasicSignalConnection>> finishedHandlers;
    QList<std::shared_ptr<BasicSignalConnection>> failedHandlers;
    QList<std::shared_ptr<BasicSignalConnection>> canceledHandlers;
    QList<std::shared_ptr<BasicSignalConnection>> timeoutHandlers;
    QList<std::shared_ptr<BasicSignalConnection>> endedHandlers;
    QList<std::shared_ptr<BasicSignalConnection>> terminatedHandlers;
    QList<std::shared_ptr<BasicSignalConnection>> progressHandlers;

    void connectBasicSignals(std::function<void(std::shared_ptr<AsyncTask>)> callback,
                             QList<std::shared_ptr<BasicSignalConnection>>& handlerList,
                             std::shared_ptr<State> stateWhenInvokedPtr,
                             bool restoreContext,
                             bool autoDisconnect,
                             common::DelegateConnection* connection = nullptr);

    virtual bool runCore(Priority priority);
    virtual void setState(const State& value);

    void emitStarted(bool waitForCompletion) const;
    void emitFinished(bool waitForCompletion) const;
    void emitFailed(bool waitForCompletion) const;
    void emitCanceled(bool waitForCompletion) const;
    void emitTimeout(bool waitForCompletion) const;
    void emitEnded(bool waitForCompletion, const State& invocationState) const;
    void emitTerminated(bool waitForCompletion) const;
    void emitProgress(bool waitForCompletion) const;
    void emitBasicSignals(const QList<std::shared_ptr<BasicSignalConnection>>& handlers,
                          bool waitForCompletion,
                          const State& invocationState) const;
    void prepareForDeletion();

    template<typename HandlerT>
    static inline QList<HandlerT*> getRawCopyOfHandlers(
            const QList<std::shared_ptr<HandlerT>>& handlers)
    {
        QList<HandlerT*> result;
        for (auto& ptr : handlers)
            result.append(ptr.get());
        return result;
    }

    static Priority getCurrentPriority();

public:
    virtual ~AsyncTask();

    bool isRunning() const;
    bool isFinished() const;
    bool isCanceled() const;
    bool isFailed() const;
    bool isTimeout() const;
    bool isEnded() const;
    bool isTerminated() const;

    std::shared_ptr<AsyncTaskService> getTaskService() const;
    State getState() const;
    QString getStateString() const;
    int getProgress() const;

    bool isWaitingForDeletion() const;
    bool getAutoRemove() const;
    std::shared_ptr<AsyncTask> setAutoRemove(bool value = true);

    int getTimeout() const;
    std::shared_ptr<AsyncTask> setTimeout(int timeoutMs = -1);

    void reportProgress(int percent);
    bool isNoOpTask() const;

    void throwStoredException() const;
    std::exception_ptr getStoredException() const;

    std::shared_ptr<AsyncTask> onStarted(
            std::function<void(std::shared_ptr<AsyncTask>)> callback,
            bool restoreContext                    = true,
            common::DelegateConnection* connection = nullptr);

    std::shared_ptr<AsyncTask> onFinished(
            std::function<void(std::shared_ptr<AsyncTask>)> callback,
            bool restoreContext                    = true,
            common::DelegateConnection* connection = nullptr);

    std::shared_ptr<AsyncTask> onFailed(
            std::function<void(std::shared_ptr<AsyncTask>)> callback,
            bool restoreContext                    = true,
            common::DelegateConnection* connection = nullptr);

    std::shared_ptr<AsyncTask> onCanceled(
            std::function<void(std::shared_ptr<AsyncTask>)> callback,
            bool restoreContext                    = true,
            common::DelegateConnection* connection = nullptr);

    std::shared_ptr<AsyncTask> onTimeout(
            std::function<void(std::shared_ptr<AsyncTask>)> callback,
            bool restoreContext                    = true,
            common::DelegateConnection* connection = nullptr);

    std::shared_ptr<AsyncTask> onEnded(
            std::function<void(std::shared_ptr<AsyncTask>, bool)> callback,
            bool restoreContext                    = true,
            common::DelegateConnection* connection = nullptr);

    std::shared_ptr<AsyncTask> onTerminated(
            std::function<void(std::shared_ptr<AsyncTask>)> callback,
            bool restoreContext                    = true,
            common::DelegateConnection* connection = nullptr);

    std::shared_ptr<AsyncTask> onProgress(
            std::function<void(std::shared_ptr<AsyncTask>, int)> callback,
            bool restoreContext                    = true,
            common::DelegateConnection* connection = nullptr);

    std::shared_ptr<AsyncTask> addMaintainedObject(std::shared_ptr<QObject> object,
                                                   bool isPermanent = false);
    std::shared_ptr<AsyncTask> removeMaintainedObject(std::shared_ptr<QObject> object);
    std::shared_ptr<AsyncTask> clearMaintainedObjects(bool clearPermanentObjects = true);

    template<typename T>
    std::shared_ptr<AsyncFunction<T>> asFunction() {
        auto f = std::dynamic_pointer_cast<AsyncFunction<T>>(shared_from_this());
        if (f == nullptr)
            throw std::runtime_error("Cannot convert AsyncTask to desired AsyncFunction.");

        return f;
    }

    template<ManagedTaskBehaviour BehaviourT = ManagedTaskBehaviour::WaitOnExit, typename TagT>
    std::shared_ptr<AsyncTask> run(TaskManager<TagT>* taskManager,
                                   std::optional<Priority> priority = std::nullopt)
    {
        taskManager->template addManagedTask<BehaviourT>(shared_from_this(), true);
        return runUnmanaged(priority);
    }

public slots:
    std::shared_ptr<AsyncTask> runUnmanaged(std::optional<Priority> priority = std::nullopt);

    std::shared_ptr<AsyncTask> runSync(bool rethrowException             = false,
                                       std::optional<Priority> priority = std::nullopt);
    void cancel();
    void terminate();
    void remove();
    void removeLater();

    void removeStartedHandler(const DelegateConnection& connection);
    void removeFinishedHandler(const DelegateConnection& connection);
    void removeCanceledHandler(const DelegateConnection& connection);
    void removeFailedHandler(const DelegateConnection& connection);
    void removeTimeoutHandler(const DelegateConnection& connection);
    void removeEndedHandler(const DelegateConnection& connection);
    void removeTerminatedHandler(const DelegateConnection& connection);
    void removeProgressHandler(const DelegateConnection& connection);
};

template<typename TResult>
class AsyncFunction : public AsyncTask {
    friend class AsyncTaskService;

    TResult result;
    bool resultSet = false;

protected:
    AsyncFunction(std::shared_ptr<AsyncTaskService> taskService,
                  std::function<bool(std::shared_ptr<AsyncTask>)> function)
        : AsyncTask(taskService, function)
    {}

    virtual void initialize() override {
        this->onStarted([this](auto) { this->resultSet = false; }, false);

        AsyncTask::initialize();
    }

public:
    template<ManagedTaskBehaviour BehaviourT = ManagedTaskBehaviour::WaitOnExit, typename TagT>
    std::shared_ptr<AsyncTask> compute(TaskManager<TagT>* taskManager,
                                       std::optional<Priority> priority = std::nullopt)
    {
        return this->template run<BehaviourT>(taskManager, priority)
                ->template asFunction<TResult>();
    }

    std::shared_ptr<AsyncFunction<TResult>> computeUnmanaged(
            std::optional<Priority> priority = std::nullopt)
    {
        return this->runUnmanaged(priority)->template asFunction<TResult>();
    }

    std::shared_ptr<AsyncFunction<TResult>> computeSync(
            bool rethrowException             = false,
            std::optional<Priority> priority = std::nullopt)
    {
        return this->runSync(rethrowException, priority)->template asFunction<TResult>();
    }

    TResult getResult() {
        if (!resultSet)
            throw std::runtime_error("Result has not been set.");

        return result;
    }

    void setResult(TResult res) {
        auto s = getState();
        if (s == State::Canceled || s == State::TimedOut || s == State::Terminated)
            return;

        if (s != State::Running)
            throw std::runtime_error("Result can only be set while task is running.");

        result    = res;
        resultSet = true;
        qDebug() << "Task's (" << reinterpret_cast<intptr_t>(this) << ") result set.";
    }

    std::shared_ptr<AsyncFunction<TResult>> onResultAvailable(
            std::function<void(TResult)> callback,
            bool restoreContext = true)
    {
        return onFinished(
                       [this, callback](std::shared_ptr<AsyncTask>) {
                           if (resultSet)
                               callback(this->getResult());
                           else
                               throw std::runtime_error(
                                       "Task finished without providing a result.");
                       },
                       restoreContext)
                ->template asFunction<TResult>();
    }
};

class BasicSignalConnection
    : public QObject
    , public std::enable_shared_from_this<BasicSignalConnection>
{
    Q_OBJECT

    friend class AsyncTaskService;
    friend class AsyncTask;

    DelegateConnection connection;
    std::shared_ptr<util::ContextReceiver> receiver;
    std::shared_ptr<AsyncTask::State> stateWhenInvoked;
    bool restoreContext;

    bool operator==(const BasicSignalConnection& other) const;
    bool operator!=(const BasicSignalConnection& other) const;
    void doDispatch(std::function<void()> onDispatchFinished) const;

    BasicSignalConnection();
    BasicSignalConnection(const DelegateConnection& connection);
    BasicSignalConnection(const DelegateConnection& connection,
                          std::shared_ptr<util::ContextReceiver> receiver,
                          std::shared_ptr<AsyncTask::State> stateWhenInvoked,
                          bool restoreContext);
};

}  // namespace common
