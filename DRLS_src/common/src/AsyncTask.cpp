#include "AsyncTask.h"
#include "service/AsyncTaskService.h"
#include "utils/ThreadHelper.h"
#include "utils/Finally.h"

#ifdef DEBUG_SAVE_STACKTRACE
#include "lvtn/utils/StackTraceHelper.h"
#endif

using namespace common;

std::recursive_mutex AsyncTask::aliveTasksCountMutex;
int AsyncTask::aliveTasksCount = 0;

std::mutex AsyncTask::priorityMapMutex;
QMap<QThread*, AsyncTask::Priority> AsyncTask::priorityMap;

QString AsyncTask::priorityToString(const AsyncTask::Priority& priority) {
    switch (priority) {
    case AsyncTask::Priority::Low:
        return "Low";
    case AsyncTask::Priority::BelowNormal:
        return "BelowNormal";
    case AsyncTask::Priority::Normal:
        return "Normal";
    case AsyncTask::Priority::AboveNormal:
        return "AboveNormal";
    case AsyncTask::Priority::High:
        return "High";
    default:
        return "";
    }
}

AsyncTask::AsyncTask(std::shared_ptr<AsyncTaskService> taskService,
                     std::function<bool(std::shared_ptr<AsyncTask>)> function)
    : QObject(nullptr)
    , taskService(taskService)
    , function(function)
    , state(State::NotStarted)
    , stateMutex()
    , waitingForDeletion(false)
{
    {
        std::lock_guard<std::recursive_mutex> lock(aliveTasksCountMutex);
        aliveTasksCount++;
        qDebug() << "Task created: " << reinterpret_cast<intptr_t>(this)
                 << "(alive tasks: " << aliveTasksCount << ")";
    }

#ifdef DEBUG_SAVE_STACKTRACE
    constructedFrom = StackTraceHelper::getStackTrace();
#endif
}

void AsyncTask::initialize() {
    initialized = true;
}

bool AsyncTask::isInitialized() const {
    return initialized;
}

AsyncTask::~AsyncTask() {
    {
        std::lock_guard<std::recursive_mutex> lock(aliveTasksCountMutex);
        aliveTasksCount--;
        qDebug() << "Task destroyed: " << reinterpret_cast<intptr_t>(this)
                 << "(alive tasks: " << aliveTasksCount << ")";
    }

    prepareForDeletion();
}

AsyncTask::State AsyncTask::getState() const {
    std::lock_guard<std::recursive_mutex> lock(stateMutex);
    return state;
}

void AsyncTask::setState(const AsyncTask::State& value) {
    auto sfthis = shared_from_this();
    auto v      = value;
    //    qDebug() << "Set state request on task (" << reinterpret_cast<intptr_t>(this) << ")
    //    from"
    //             << stateToString(state) << "to " << stateToString(v);

    {
        // change the state
        std::lock_guard<std::recursive_mutex> lock(stateMutex);
        if (state == v)
            return;

        // Prevent immediately jumping over states
        if (v == State::Running)
            v = State::Starting;
        if (v == State::Canceled)
            v = State::Cancelling;
        if (v == State::TimedOut)
            v = State::TimingOut;
        if (v == State::Finished)
            v = State::Finishing;
        if (v == State::Failed)
            v = State::Failing;

        // Finished, Failed, Timeout and Canceled is only reachable from Running
        if (state != State::Running && (v == State::Cancelling || v == State::TimingOut ||
                                        v == State::Finishing || v == State::Failing))
        {
            //            qDebug() << "Task's (" << reinterpret_cast<intptr_t>(this) << ") state
            //            DIDN'T change to"
            //                     << stateToString(v) << "because it's not Running.";
            return;
        }

        qDebug() << "Task's (" << reinterpret_cast<intptr_t>(this) << ") state changed from"
                 << stateToString(state) << "to " << stateToString(v);
        state = v;
    }

    auto f = util::finally([=]() {
        qDebug() << "Task's (" << reinterpret_cast<intptr_t>(this) << ") new state is"
                 << stateToString(state);
    });

    bool ended                 = false;
    auto changeAllowedHandlers = [this](auto s) {
        std::lock_guard<std::recursive_mutex> lock(signalHandlerMutex);
        this->allowedHandlers = {s};
    };

    if (v == State::Starting) {
        changeAllowedHandlers(v);
        emitStarted(true);
    } else if (v == State::Finishing) {
        changeAllowedHandlers(v);
        emitFinished(true);
        ended = true;
    } else if (v == State::Cancelling) {
        changeAllowedHandlers(v);
        emitCanceled(true);
        ended = true;
    } else if (v == State::Failing) {
        changeAllowedHandlers(v);
        emitFailed(true);
        ended = true;
    } else if (v == State::TimingOut) {
        changeAllowedHandlers(v);
        emitTimeout(true);
        ended = true;
    } else if (v == State::Terminated) {
        changeAllowedHandlers(v);
        {
            std::lock_guard<std::mutex> lock(this->noPendingHandlerCondVarsMutex);
            for (auto cv : this->noPendingHandlerCondVars)
                cv->notify_all();
        }

        emitTerminated(false);
    }

    if (ended) {
        emitEnded(true, v);
        if (v != State::Finishing && !subtasks.empty()) {
            std::lock_guard<std::recursive_mutex> lock(stateMutex);
            qDebug() << "Canceling" << subtasks.count() << "subtasks of"
                     << reinterpret_cast<intptr_t>(this);
            for (auto subtask : subtasks) {
                auto t = subtask.lock();
                if (t != nullptr)
                    t->cancel();
            }
        }
    } else {
        if (v == State::Terminated && !subtasks.empty()) {
            std::lock_guard<std::recursive_mutex> lock(stateMutex);
            qDebug() << "Terminating" << subtasks.count() << "subtasks of"
                     << reinterpret_cast<intptr_t>(this);
            for (auto subtask : subtasks) {
                auto t = subtask.lock();
                if (t != nullptr)
                    t->terminate();
            }

            qDebug() << "Clearing subtasks on termination of"
                     << reinterpret_cast<intptr_t>(this);
            subtasks.clear();
        }
    }

    QList<State> finalStates = {State::Finished,
                                State::Canceled,
                                State::Failed,
                                State::TimedOut,
                                State::Terminated};

    bool deleteTask = false;
    {
        // change the state
        std::lock_guard<std::recursive_mutex> lock(stateMutex);
        {
            if (state != v)
                return;

            if (state == State::Starting)
                v = State::Running;
            else if (state == State::Finishing)
                v = State::Finished;
            else if (state == State::Cancelling)
                v = State::Canceled;
            else if (state == State::Failing)
                v = State::Failed;
            else if (state == State::TimingOut)
                v = State::TimedOut;

            if (state != v) {
                qDebug() << "Task's (" << reinterpret_cast<intptr_t>(this)
                         << ") state changed from" << stateToString(state) << "to "
                         << stateToString(v);
                state = v;
            }

            if (autoRemove && finalStates.contains(v))
                deleteTask = true;
        }
    }

    changeAllowedHandlers(state);

    if (deleteTask)
        taskService->deleteTask(sfthis);
}

bool AsyncTask::isRunning() const {
    std::lock_guard<std::recursive_mutex> lock(stateMutex);
    return state == State::Running || state == State::Starting;
}

bool AsyncTask::isFinished() const {
    std::lock_guard<std::recursive_mutex> lock(stateMutex);
    return state == State::Finished || state == State::Finishing;
}

bool AsyncTask::isCanceled() const {
    std::lock_guard<std::recursive_mutex> lock(stateMutex);
    return state == State::Canceled || state == State::Cancelling;
}

bool AsyncTask::isFailed() const {
    std::lock_guard<std::recursive_mutex> lock(stateMutex);
    return state == State::Failed || state == State::Failing;
}

bool AsyncTask::isTimeout() const {
    std::lock_guard<std::recursive_mutex> lock(stateMutex);
    return state == State::TimedOut || state == State::TimingOut;
}

bool AsyncTask::isEnded() const {
    std::lock_guard<std::recursive_mutex> lock(stateMutex);
    return (state == State::TimedOut || state == State::TimingOut || state == State::Canceled ||
            state == State::Cancelling || state == State::Failed || state == State::Failing ||
            state == State::Finished || state == State::Finishing);
}

bool AsyncTask::isTerminated() const {
    std::lock_guard<std::recursive_mutex> lock(stateMutex);
    return state == State::Terminated;
}

std::shared_ptr<AsyncTask> AsyncTask::runUnmanaged(std::optional<Priority> priority) {
    if (isWaitingForDeletion()) {
        qWarning() << "Task (" << reinterpret_cast<intptr_t>(this) << ") is prevented"
                   << "from being run because it is under deletion.";
        return shared_from_this();
    }

    taskService->runTask(shared_from_this(), priority);
    return shared_from_this();
}

std::shared_ptr<AsyncTask> AsyncTask::runSync(bool rethrowException,
                                              std::optional<Priority> priority)
{
    if (isWaitingForDeletion()) {
        qWarning() << "Task (" << reinterpret_cast<intptr_t>(this) << ") is prevented"
                   << "from being run because it is under deletion.";
        return shared_from_this();
    }

    taskService->runTaskSync(shared_from_this(), rethrowException, priority);
    return shared_from_this();
}

void AsyncTask::cancel() {
    taskService->cancelTask(shared_from_this());
}

void AsyncTask::terminate() {
    taskService->terminateTask(shared_from_this());
}

void AsyncTask::remove() {
    taskService->deleteTask(shared_from_this());
}

void AsyncTask::removeLater() {
    QTimer::singleShot(0, this, std::bind(&AsyncTask::remove, this));
}

void AsyncTask::removeStartedHandler(const DelegateConnection& connection) {
    std::lock_guard<std::recursive_mutex> lock(signalHandlerMutex);
    BasicSignalConnection conn(connection);
    for (auto& ptr : startedHandlers) {
        if (*ptr == conn) {
            this->startedHandlers.removeOne(ptr);
            break;
        }
    }
}

void AsyncTask::removeFinishedHandler(const DelegateConnection& connection) {
    std::lock_guard<std::recursive_mutex> lock(signalHandlerMutex);
    BasicSignalConnection conn(connection);
    for (auto& ptr : finishedHandlers) {
        if (*ptr == conn) {
            this->finishedHandlers.removeOne(ptr);
            break;
        }
    }
}

void AsyncTask::removeCanceledHandler(const DelegateConnection& connection) {
    std::lock_guard<std::recursive_mutex> lock(signalHandlerMutex);
    BasicSignalConnection conn(connection);
    for (auto& ptr : canceledHandlers) {
        if (*ptr == conn) {
            this->canceledHandlers.removeOne(ptr);
            break;
        }
    }
}

void AsyncTask::removeFailedHandler(const DelegateConnection& connection) {
    std::lock_guard<std::recursive_mutex> lock(signalHandlerMutex);
    BasicSignalConnection conn(connection);
    for (auto& ptr : failedHandlers) {
        if (*ptr == conn) {
            this->failedHandlers.removeOne(ptr);
            break;
        }
    }
}

void AsyncTask::removeTimeoutHandler(const DelegateConnection& connection) {
    std::lock_guard<std::recursive_mutex> lock(signalHandlerMutex);
    BasicSignalConnection conn(connection);
    for (auto& ptr : timeoutHandlers) {
        if (*ptr == conn) {
            this->timeoutHandlers.removeOne(ptr);
            break;
        }
    }
}

void AsyncTask::removeEndedHandler(const DelegateConnection& connection) {
    std::lock_guard<std::recursive_mutex> lock(signalHandlerMutex);
    BasicSignalConnection conn(connection);
    for (auto ptr : endedHandlers) {
        if (*ptr == conn) {
            this->endedHandlers.removeOne(ptr);
            break;
        }
    }
}

void AsyncTask::removeTerminatedHandler(const DelegateConnection& connection) {
    std::lock_guard<std::recursive_mutex> lock(signalHandlerMutex);
    BasicSignalConnection conn(connection);
    for (auto& ptr : terminatedHandlers) {
        if (*ptr == conn) {
            this->terminatedHandlers.removeOne(ptr);
            break;
        }
    }
}

void AsyncTask::removeProgressHandler(const DelegateConnection& connection) {
    std::lock_guard<std::recursive_mutex> lock(signalHandlerMutex);
    BasicSignalConnection conn(connection);
    for (auto ptr : progressHandlers) {
        if (*ptr == conn) {
            this->progressHandlers.removeOne(ptr);
            break;
        }
    }
}

bool AsyncTask::runCore(Priority priority) {
    qDebug() << "Task (" << reinterpret_cast<intptr_t>(this)
             << ") is running on thread:" << QThread::currentThreadId()
             << "with priority:" << priorityToString(priority);

    if (!isInitialized())
        throw std::runtime_error("Cannot run tasks before initialization.");
    if (isWaitingForDeletion())
        throw std::runtime_error("Cannot run tasks which are waiting for deletion.");
    if (isNoOpTask())
        throw std::runtime_error("Cannot run no-op tasks.");

    exception   = nullptr;
    progress    = 0;
    bool result = false;

    if (this->getState() != State::Running) {
        qDebug() << "Task (" << reinterpret_cast<intptr_t>(this)
                 << ") has been cancelled before running.";
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(priorityMapMutex);
        priorityMap[QThread::currentThread()] = priority;
    }

    try {
        result   = function(shared_from_this());
        progress = 100;
    } catch (const std::exception& e) {
        auto t = typeid(e).name();
        qCritical("Exception of type '%s' occurred while running task %ld. Message: %s",
                  t,
                  reinterpret_cast<intptr_t>(this),
                  e.what());
        this->exception = std::current_exception();
    } catch (...) {
        qCritical("Unknown error occurred while running task %ld.",
                  reinterpret_cast<intptr_t>(this));
        this->exception = std::current_exception();
    }

    {
        std::lock_guard<std::mutex> lock(priorityMapMutex);
        priorityMap.remove(QThread::currentThread());
    }

    return result;
}

std::shared_ptr<AsyncTaskService> AsyncTask::getTaskService() const {
    return taskService;
}

QString AsyncTask::getStateString() const {
    std::lock_guard<std::recursive_mutex> lock(stateMutex);
    return stateToString(state);
}

int AsyncTask::getProgress() const {
    std::lock_guard<std::recursive_mutex> lock(stateMutex);
    return progress;
}

bool AsyncTask::isWaitingForDeletion() const {
    std::lock_guard<std::recursive_mutex> lock(stateMutex);
    return waitingForDeletion;
}

void AsyncTask::emitStarted(bool waitForCompletion) const {
    emitBasicSignals(startedHandlers, waitForCompletion, State::Starting);
}

void AsyncTask::emitFinished(bool waitForCompletion) const {
    emitBasicSignals(finishedHandlers, waitForCompletion, State::Finishing);
}

void AsyncTask::emitFailed(bool waitForCompletion) const {
    emitBasicSignals(failedHandlers, waitForCompletion, State::Failing);
}

void AsyncTask::emitCanceled(bool waitForCompletion) const {
    emitBasicSignals(canceledHandlers, waitForCompletion, State::Cancelling);
}

void AsyncTask::emitTimeout(bool waitForCompletion) const {
    emitBasicSignals(timeoutHandlers, waitForCompletion, State::TimingOut);
}

void AsyncTask::emitEnded(bool waitForCompletion, const State& invocationState) const {
    emitBasicSignals(endedHandlers, waitForCompletion, invocationState);
}

void AsyncTask::emitTerminated(bool waitForCompletion) const {
    emitBasicSignals(terminatedHandlers, waitForCompletion, State::Terminated);
}

void AsyncTask::emitProgress(bool waitForCompletion) const {
    emitBasicSignals(progressHandlers, waitForCompletion, State::Running);
}

void AsyncTask::emitBasicSignals(const QList<std::shared_ptr<BasicSignalConnection>>& handlers,
                                 bool waitForCompletion,
                                 const State& invocationState) const
{
    QList<std::shared_ptr<BasicSignalConnection>> copyOfHandlers;
    {
        std::lock_guard<std::recursive_mutex> lock(signalHandlerMutex);
        if (handlers.empty()) {
            //            qDebug() << "No handlers for task" << reinterpret_cast<intptr_t>(this)
            //                     << "- invocationState:" << stateToString(invocationState);
            return;
        }

        qDebug() << "Emitting" << handlers.count() << " handlers of"
                 << reinterpret_cast<intptr_t>(this)
                 << "- invocationState:" << stateToString(invocationState);

        // sometimes it is required to dispose handlers immediately after its execution
        // in this case the destruction must be done from the same thread
        copyOfHandlers = handlers;
    }

    auto pendingHandlers   = std::make_shared<int>(0);
    auto waitHandlersMutex = std::make_shared<std::mutex>();
    auto noPendingHandlers = std::make_shared<std::condition_variable>();

    {
        std::lock_guard<std::mutex> lock(this->noPendingHandlerCondVarsMutex);
        this->noPendingHandlerCondVars.append(noPendingHandlers);
    }

    auto decrease = util::finally([this, noPendingHandlers]() {
        std::lock_guard<std::mutex> lock(this->noPendingHandlerCondVarsMutex);
        this->noPendingHandlerCondVars.removeOne(noPendingHandlers);
    });

    while (copyOfHandlers.count() > 0) {
        BasicSignalConnection* rawHandler;
        {
            std::lock_guard<std::recursive_mutex> lock(signalHandlerMutex);
            if (!allowedHandlers.contains(invocationState)) {
                qDebug() << "Emission of handlers has been interrupted due to changes in task's"
                         << reinterpret_cast<intptr_t>(this) << "state";
                break;
            }

            {
                auto handler               = copyOfHandlers[0];
                *handler->stateWhenInvoked = invocationState;

                // prevent handler from deletion till its execution is finished
                handler->receiver->keepAlive(handler);
                rawHandler = handler.get();
            }
            copyOfHandlers.removeAt(0);
        }

        //            bool restore = rawHandler->restoreContext;
        //            qDebug() << "Doing dispatch - restore:" << restore << " task:"
        //                     << reinterpret_cast<intptr_t>(this) << "pending handlers:" <<
        //                     static_cast<int>(this->pendingHandlers)
        //                     << "- invocationState:" << stateToString(invocationState);

        {
            std::lock_guard<std::mutex> lokk(*waitHandlersMutex);
            (*pendingHandlers)++;
        }

        rawHandler->doDispatch([pendingHandlers, noPendingHandlers, waitHandlersMutex]() {
            {
                std::lock_guard<std::mutex> lokk(*waitHandlersMutex);
                (*pendingHandlers)--;
            }

            (*noPendingHandlers).notify_all();
        });
        //            qDebug() << "Done dispatch - restore:" << restore << " task:"
        //                     << reinterpret_cast<intptr_t>(this) << "pending handlers:" <<
        //                     static_cast<int>(this->pendingHandlers);
    }

    if (waitForCompletion) {
        qDebug() << "Waiting handlers of" << reinterpret_cast<intptr_t>(this) << "to complete";

        // wait all tasks to complete
        std::unique_lock<std::mutex> handlerLock(*waitHandlersMutex);
        (*noPendingHandlers).wait(handlerLock, [this, pendingHandlers] {
            if (this->isTerminated())
                return true;

            return (*pendingHandlers) == 0;
        });

        qDebug() << "Handlers of" << reinterpret_cast<intptr_t>(this)
                 << "completed (or task has been terminated)";
    }
}

void AsyncTask::prepareForDeletion() {
    {
        std::lock_guard<std::recursive_mutex> lock(stateMutex);
        waitingForDeletion = true;

        if (state != State::Terminated)
            subtasks.clear();
    }

    {
        std::lock_guard<std::mutex> lock(maintainedMutex);
        maintainedObjects.clear();
    }

    std::lock_guard<std::recursive_mutex> lock(signalHandlerMutex);
    //    qDebug() << "Task (" << reinterpret_cast<intptr_t>(this) << ") is preparing for
    //    deletion.";
    startedHandlers.clear();
    finishedHandlers.clear();
    failedHandlers.clear();
    canceledHandlers.clear();
    timeoutHandlers.clear();
    endedHandlers.clear();
    progressHandlers.clear();
    terminatedHandlers.clear();
}

AsyncTask::Priority AsyncTask::getCurrentPriority() {
    std::lock_guard<std::mutex> lock(priorityMapMutex);
    auto t = QThread::currentThread();
    return priorityMap.contains(t) ? priorityMap[t] : Priority::Normal;
}

std::shared_ptr<AsyncTask> AsyncTask::onStarted(
        std::function<void(std::shared_ptr<AsyncTask>)> callback,
        bool restoreContext,
        DelegateConnection* connection)
{
    auto stateWhenInvoked = std::make_shared<State>();
    connectBasicSignals(callback,
                        startedHandlers,
                        stateWhenInvoked,
                        restoreContext,
                        true,
                        connection);
    return shared_from_this();
}

std::shared_ptr<AsyncTask> AsyncTask::onFinished(
        std::function<void(std::shared_ptr<AsyncTask>)> callback,
        bool restoreContext,
        DelegateConnection* connection)
{
    auto stateWhenInvoked = std::make_shared<State>();
    connectBasicSignals(callback,
                        finishedHandlers,
                        stateWhenInvoked,
                        restoreContext,
                        true,
                        connection);
    return shared_from_this();
}

std::shared_ptr<AsyncTask> AsyncTask::onFailed(
        std::function<void(std::shared_ptr<AsyncTask>)> callback,
        bool restoreContext,
        DelegateConnection* connection)
{
    auto stateWhenInvoked = std::make_shared<State>();
    connectBasicSignals(callback,
                        failedHandlers,
                        stateWhenInvoked,
                        restoreContext,
                        true,
                        connection);
    return shared_from_this();
}

std::shared_ptr<AsyncTask> AsyncTask::onCanceled(
        std::function<void(std::shared_ptr<AsyncTask>)> callback,
        bool restoreContext,
        DelegateConnection* connection)
{
    auto stateWhenInvoked = std::make_shared<State>();
    connectBasicSignals(callback,
                        canceledHandlers,
                        stateWhenInvoked,
                        restoreContext,
                        true,
                        connection);
    return shared_from_this();
}

std::shared_ptr<AsyncTask> AsyncTask::onTimeout(
        std::function<void(std::shared_ptr<AsyncTask>)> callback,
        bool restoreContext,
        DelegateConnection* connection)
{
    auto stateWhenInvoked = std::make_shared<State>();
    connectBasicSignals(callback,
                        timeoutHandlers,
                        stateWhenInvoked,
                        restoreContext,
                        true,
                        connection);
    return shared_from_this();
}

std::shared_ptr<AsyncTask> AsyncTask::onEnded(
        std::function<void(std::shared_ptr<AsyncTask>, bool)> callback,
        bool restoreContext,
        DelegateConnection* connection)
{
    auto stateWhenInvoked = std::make_shared<State>();
    auto wrapper          = [stateWhenInvoked, callback](std::shared_ptr<AsyncTask> t) {
        callback(t, *stateWhenInvoked == State::Finishing);
    };

    connectBasicSignals(wrapper,
                        endedHandlers,
                        stateWhenInvoked,
                        restoreContext,
                        true,
                        connection);
    return shared_from_this();
}

std::shared_ptr<AsyncTask> AsyncTask::onTerminated(
        std::function<void(std::shared_ptr<AsyncTask>)> callback,
        bool restoreContext,
        DelegateConnection* connection)
{
    auto stateWhenInvoked = std::make_shared<State>();
    connectBasicSignals(callback,
                        terminatedHandlers,
                        stateWhenInvoked,
                        restoreContext,
                        true,
                        connection);
    return shared_from_this();
}

std::shared_ptr<AsyncTask> AsyncTask::onProgress(
        std::function<void(std::shared_ptr<AsyncTask>, int)> callback,
        bool restoreContext,
        DelegateConnection* connection)
{
    auto stateWhenInvoked = std::make_shared<State>();
    auto wrapper          = [stateWhenInvoked, callback](std::shared_ptr<AsyncTask> t) {
        callback(t, t->progress);
    };

    connectBasicSignals(wrapper,
                        progressHandlers,
                        stateWhenInvoked,
                        restoreContext,
                        false,
                        connection);
    return shared_from_this();
}

void AsyncTask::connectBasicSignals(std::function<void(std::shared_ptr<AsyncTask>)> callback,
                                    QList<std::shared_ptr<BasicSignalConnection>>& handlerList,
                                    std::shared_ptr<State> stateWhenInvokedPtr,
                                    bool restoreContext,
                                    bool autoDisconnect,
                                    DelegateConnection* connection)
{
    auto sfthis                       = shared_from_this();
    std::weak_ptr<AsyncTask> weakThis = sfthis;
    auto connHolder                   = std::make_shared<DelegateConnection>();
    auto receiver = std::make_shared<util::ContextReceiver>([this,
                                                             weakThis,
                                                             stateWhenInvokedPtr,
                                                             callback,
                                                             connHolder,
                                                             &handlerList,
                                                             autoDisconnect]() {
        auto task = weakThis.lock();
        if (task == nullptr)
            return;

        //        qDebug() << "Preparing for dispatch of signal related to task ("
        //                    << reinterpret_cast<intptr_t>(this) << ").";

        // if signal handler invocation is suppressed by changes in task's state, do nothing
        {
            std::lock_guard<std::recursive_mutex> lock(signalHandlerMutex);
            if (!allowedHandlers.contains(*stateWhenInvokedPtr)) {
                qDebug() << "Dispatching of signal has been canceled due to changes in task's ("
                         << reinterpret_cast<intptr_t>(this) << ") state.";
                return;
            }
        }

        callback(task);

        // remove callback immediately if task is disposable
        // this ensures that destructor of lambdas will be run on the same thread
        if (autoRemove && autoDisconnect) {
            std::lock_guard<std::recursive_mutex> lock(signalHandlerMutex);
            for (auto ptr : handlerList) {
                if (ptr->connection == *connHolder) {
                    //                    qDebug() << "Removing handler from: " <<
                    //                    reinterpret_cast<intptr_t>(this) <<
                    //                    reinterpret_cast<intptr_t>(that)
                    //                             << "receiver: " <<
                    //                             reinterpret_cast<intptr_t>(ptr->receiver.get());
                    handlerList.removeOne(ptr);
                    break;
                }
            }
        }
    });

    // target thread must be running an event loop
    // update: target thread must always be the main thread
    if (restoreContext && QCoreApplication::instance()->thread() != receiver->thread()) {
        receiver->moveToThread(QCoreApplication::instance()->thread());
        //        throw std::runtime_error("Task's context can be restored only if caller is
        //        belonging to Application thread.");
    }

    auto connRaw = new BasicSignalConnection(DelegateConnection(),
                                             receiver,
                                             stateWhenInvokedPtr,
                                             restoreContext);
    auto conn    = std::shared_ptr<BasicSignalConnection>(connRaw);
    *connHolder  = conn->connection;
    if (connection != nullptr)
        *connection = conn->connection;

    std::lock_guard<std::recursive_mutex> lock(signalHandlerMutex);
    handlerList.append(conn);
}

bool AsyncTask::getAutoRemove() const {
    return autoRemove;
}

std::shared_ptr<AsyncTask> AsyncTask::setAutoRemove(bool value) {
    std::lock_guard<std::recursive_mutex> lock(stateMutex);
    autoRemove = value;
    return shared_from_this();
}

int AsyncTask::getTimeout() const {
    return timeoutMs;
}

std::shared_ptr<AsyncTask> AsyncTask::setTimeout(int timeoutMs) {
    this->timeoutMs = timeoutMs;
    return shared_from_this();
}

void AsyncTask::reportProgress(int percent) {
    {
        std::lock_guard<std::recursive_mutex> lock(stateMutex);
        if (state != AsyncTask::State::Running)
            return;

        this->progress = qMax(0, qMin(100, percent));
    }

    emitProgress(true);
}

bool AsyncTask::isNoOpTask() const {
    return this->function == nullptr;
}

void AsyncTask::throwStoredException() const {
    if (exception != nullptr)
        std::rethrow_exception(exception);
}

std::exception_ptr AsyncTask::getStoredException() const {
    return exception;
}

std::shared_ptr<AsyncTask> AsyncTask::addMaintainedObject(std::shared_ptr<QObject> object,
                                                          bool isPermanent)
{
    std::lock_guard<std::mutex> lock(maintainedMutex);
    this->maintainedObjects[object] = isPermanent;
    return shared_from_this();
}

std::shared_ptr<AsyncTask> AsyncTask::removeMaintainedObject(std::shared_ptr<QObject> object) {
    std::lock_guard<std::mutex> lock(maintainedMutex);
    this->maintainedObjects.remove(object);
    return shared_from_this();
}

std::shared_ptr<AsyncTask> AsyncTask::clearMaintainedObjects(bool clearPermanentObjects) {
    std::lock_guard<std::mutex> lock(maintainedMutex);
    for (auto o : this->maintainedObjects.keys())
        if (clearPermanentObjects || !this->maintainedObjects[o])
            this->maintainedObjects.remove(o);

    return shared_from_this();
}

bool BasicSignalConnection::operator==(const BasicSignalConnection& other) const {
    return this->connection == other.connection;
}

bool BasicSignalConnection::operator!=(const BasicSignalConnection& other) const {
    return this->connection != other.connection;
}

void BasicSignalConnection::doDispatch(std::function<void()> onDispatchFinished) const {
    if (!this->restoreContext) {
        this->receiver->receive();
        if (onDispatchFinished != nullptr)
            onDispatchFinished();
    } else {
        auto dispatcher =
                std::make_shared<util::ContextDispatcher>(this->receiver,
                                                          util::DispatchMethod::Async);
        connect(this->receiver.get(), &util::ContextReceiver::finished, onDispatchFinished);

        dispatcher->doDispatch();
    }
}

BasicSignalConnection::BasicSignalConnection()
    : connection(), receiver(nullptr), stateWhenInvoked(nullptr), restoreContext(false)
{
}

BasicSignalConnection::BasicSignalConnection(const DelegateConnection& connection)
    : connection(connection)
    , receiver(nullptr)
    , stateWhenInvoked(nullptr)
    , restoreContext(false)
{
}

BasicSignalConnection::BasicSignalConnection(const DelegateConnection& connection,
                                             std::shared_ptr<util::ContextReceiver> receiver,
                                             std::shared_ptr<AsyncTask::State> stateWhenInvoked,
                                             bool restoreContext)
    : connection(connection)
    , receiver(receiver)
    , stateWhenInvoked(stateWhenInvoked)
    , restoreContext(restoreContext)
{
}
