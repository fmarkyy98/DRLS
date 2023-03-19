#include "AsyncTaskService.h"

#include "common/src/Delegate.h"

#include "utils/Finally.h"

#include <QtConcurrent/QtConcurrentRun>
#include <condition_variable>

using namespace common;

#define MIN_THREAD_COUNT 4

AsyncTaskService::AsyncTaskService()
    : pool(QThreadPool::globalInstance()), tasks(), runningTasks(), taskMutex()
{
    qRegisterMetaType<std::shared_ptr<AsyncTask>>("std::shared_ptr<AsyncTask>");
    mainThread = QThread::currentThread();
    qDebug() << "Initializing AsyncTaskService - main thread is " << QThread::currentThreadId()
             << "ptr:" << reinterpret_cast<intptr_t>(QThread::currentThread());

    // we use at least 4 threads
    if (pool->maxThreadCount() < MIN_THREAD_COUNT)
        pool->setMaxThreadCount(MIN_THREAD_COUNT);
}

AsyncTaskService::~AsyncTaskService() {
    deleteAllTasks();
}

void AsyncTaskService::handleSubtaskTimeout(QList<std::shared_ptr<AsyncTask>> tasks,
                                            QObject* parent)
{
    // this method is a little bit complex...
    // the reason is that QTimer is hardly usable in multithreaded environment
    // to interact with QTimer, we have to use signal-slot pattern

    QTimer* timer = new QTimer();
    timer->setSingleShot(true);

    for (auto& task : tasks) {
        // invoker is for signal transmission to caller thread, which timer belongs to
        QObject* invoker = new QObject();

        invoker->setParent(timer);

        connect(invoker, &QObject::destroyed, [task, timer]() {
            // stop timer for previous subtask
            timer->stop();
            timer->disconnect(SIGNAL(timeout()));
            qDebug() << "Subtask timer stopped, new task:"
                     << reinterpret_cast<intptr_t>(task.get());

            // if subtask has a timeout, use the timer
            if (task->getTimeout() > 0) {
                qDebug() << "Subtask has a timeout:" << task->getTimeout();
                connect(timer, &QTimer::timeout, [task]() {
                    qDebug() << "Subtask timeout:" << reinterpret_cast<intptr_t>(task.get());
                    task->setState(AsyncTask::State::TimingOut);
                });
                timer->start(task->getTimeout());
            }
        });

        task->onStarted([invoker](auto) { invoker->deleteLater(); }, false);
    }

    timer->moveToThread(mainThread);

    // as invoker objects are descendants of timer, the destruction of timer would cause
    // the same thing as the started signal of tasks, which is not desired
    // keep in mind the destruction chain: sequence->timerGuard->timer->invoker
    connect(timer, &QObject::destroyed, [timer]() {
        // prevent further timer operations...
        for (auto child : timer->children())
            child->disconnect(SIGNAL(destroyed(QObject*)));
    });

    // signal transmission to caller thread, which timer belongs to
    connect(parent, SIGNAL(destroyed(QObject*)), timer, SLOT(deleteLater()));
}

bool AsyncTaskService::checkOrigin(QList<std::shared_ptr<AsyncTask>> tasks) {
    for (auto& task : tasks)
        if (task->taskService != shared_from_this())
            return false;

    return true;
}

void AsyncTaskService::modifyWorkersCount(int diff) {
    std::lock_guard<std::mutex> lock(poolMutex);
    pool->setMaxThreadCount(qMax(MIN_THREAD_COUNT, pool->maxThreadCount() + diff));
    qDebug() << "AsyncTaskService: number of workers changed to" << pool->maxThreadCount();
}

void AsyncTaskService::doAsynchronously(std::function<void()> function, bool onExtraThread) {
    std::function<void()> action;
    if (!onExtraThread)
        action = function;
    else {
        action = [this, function]() {
            ExtraThreadLock extraThread(shared_from_this());
            function();
        };
    }

    Q_UNUSED(QtConcurrent::run(pool, action));
}

void AsyncTaskService::emitExceptionSignals(std::shared_ptr<AsyncTask> task) {
    try {
        task->throwStoredException();
    } catch (...) {
        // intentionally left blank
    }
}

bool AsyncTaskService::initSubtasks(std::shared_ptr<AsyncTask> self,
                                    const QList<std::shared_ptr<AsyncTask>>& tasks,
                                    const QString compositeType)
{
    std::lock_guard<std::recursive_mutex> lock(self->stateMutex);
    if (self->getState() != AsyncTask::State::Running) {
        qDebug() << compositeType << "(" << reinterpret_cast<intptr_t>(self.get())
                 << ") has been cancelled before running. Subtasks:";
        for (auto& task : tasks) {
            qDebug() << "    - " << reinterpret_cast<intptr_t>(task.get());
            if (task->getAutoRemove())
                this->deleteTask(task);
        }

        return false;
    }

    qDebug() << "Running" << compositeType << "(" << reinterpret_cast<intptr_t>(self.get())
             << ") with " << tasks.count() << " subtasks:";
    for (auto& task : tasks) {
        qDebug() << "    - " << reinterpret_cast<intptr_t>(task.get());
        self->subtasks.append(task);
    }

    return true;
}

std::shared_ptr<AsyncTask> AsyncTaskService::createSequence(
        QList<std::shared_ptr<AsyncTask>> tasks)
{
    if (tasks.count() < 1)
        return this->createNoOpTask();
    if (tasks.count() == 1)
        return tasks.first();

    if (!checkOrigin(tasks))
        throw std::runtime_error("Cannot create Sequence of foreign tasks.");

    auto sequence = createTask<true>([this, tasks](std::shared_ptr<AsyncTask> self) {
        bool result                  = true;
        std::exception_ptr exception = nullptr;

        if (!this->initSubtasks(self, tasks, "Sequence"))
            return false;

        for (auto& task : tasks) {
            auto autoremove = task->getAutoRemove();
            auto fin        = util::finally([this, task, autoremove] {
                if (autoremove)
                    deleteTask(task);
            });

            // if sequence aborted, also abort all further tasks
            if (!result) {
                qDebug() << "Sequence is prevented from running its next subtask:"
                         << reinterpret_cast<intptr_t>(task.get());
                continue;
            }

            qDebug() << "Sequence is running its next subtask:"
                     << reinterpret_cast<intptr_t>(task.get());

            // prevent deletion of task, run it synchronously
            task->setAutoRemove(false);
            runTask(task, false, AsyncTask::getCurrentPriority());

            auto state = task->getState();

            // if current task has been aborted, also abort the sequence itself
            if (state == AsyncTask::State::Canceled || state == AsyncTask::State::TimedOut ||
                state == AsyncTask::State::Terminated) {
                self->setState(state);
                result = false;
            }

            // if current task has failed, abort the sequence
            if (state == AsyncTask::State::Failed) {
                result    = false;
                exception = task->exception;
            }
        }

        if (exception != nullptr) {
            qDebug() << "Sequence (" << reinterpret_cast<intptr_t>(self.get())
                     << ") rethrowing exception stored in the failed subtask.";
            std::rethrow_exception(exception);
        }

        qDebug() << "Sequence (" << reinterpret_cast<intptr_t>(self.get()) << ") returned.";
        return result;
    });

    // sequence consists of multiple subtasks with individual timeouts
    handleSubtaskTimeout(tasks, sequence.get());
    return sequence;
}

std::shared_ptr<AsyncTask> AsyncTaskService::createFallback(
        QList<std::shared_ptr<AsyncTask>> tasks)
{
    if (tasks.count() < 1)
        return this->createNoOpTask();
    if (tasks.count() == 1)
        return tasks.first();

    if (!checkOrigin(tasks))
        throw std::runtime_error("Cannot create Fallback of foreign tasks.");

    auto fallback = createTask<true>([this, tasks](std::shared_ptr<AsyncTask> self) {
        bool result = false;

        if (!this->initSubtasks(self, tasks, "Fallback"))
            return false;

        for (auto& task : tasks) {
            auto autoremove = task->getAutoRemove();
            auto fin        = util::finally([this, task, autoremove] {
                if (autoremove)
                    deleteTask(task);
            });

            // if fallback aborted, also abort all further tasks
            auto selfState = self->getState();
            if (selfState == AsyncTask::State::Canceled ||
                selfState == AsyncTask::State::TimedOut ||
                selfState == AsyncTask::State::Finished ||
                selfState == AsyncTask::State::Terminated) {
                qDebug() << "Fallback is prevented from running its next subtask:"
                         << reinterpret_cast<intptr_t>(task.get());
                continue;
            }

            qDebug() << "Fallback is running its next subtask:"
                     << reinterpret_cast<intptr_t>(task.get());

            // prevent deletion of task, run it synchronously
            task->setAutoRemove(false);
            runTask(task, false, AsyncTask::getCurrentPriority());

            auto state = task->getState();

            // if current task succeeded, fallback is finished
            if (state == AsyncTask::State::Finished) {
                self->setState(AsyncTask::State::Finishing);
                result = true;
            }
        }

        qDebug() << "Fallback (" << reinterpret_cast<intptr_t>(self.get()) << ") returned.";
        return result;
    });

    // fallback consists of multiple subtasks with individual timeouts
    handleSubtaskTimeout(tasks, fallback.get());
    return fallback;
}

std::shared_ptr<AsyncTask> AsyncTaskService::createParallel(
        QList<std::shared_ptr<AsyncTask>> tasks)
{
    if (tasks.count() < 1)
        return this->createNoOpTask();
    if (tasks.count() == 1)
        return tasks.first();

    if (!checkOrigin(tasks))
        throw std::runtime_error("Cannot create Parallel of foreign tasks.");

    auto parallel = createTask<true>([this, tasks](std::shared_ptr<AsyncTask> self) {
        auto result        = std::make_shared<bool>(true);
        auto selfFailed    = std::make_shared<bool>(false);
        auto alreadyFailed = std::make_shared<bool>(false);
        auto allFinished   = std::make_shared<std::condition_variable>();
        auto tasksEnded    = std::make_shared<QMap<AsyncTaskPtr, bool>>();
        auto mutex         = std::make_shared<std::mutex>();
        auto exception     = std::make_shared<std::exception_ptr>(nullptr);

        DelegateConnection selfEndedConn, selfTerminatedConn;
        QMap<AsyncTaskPtr, QList<DelegateConnection>> startedConns, endedConns, terminatedConns;

        if (!this->initSubtasks(self, tasks, "Parallel"))
            return false;

        auto childEnded = [result,
                           alreadyFailed,
                           tasks,
                           mutex,
                           tasksEnded,
                           allFinished,
                           exception](AsyncTaskPtr t, bool success) {
            bool cancelTasks = false;

            {
                std::lock_guard<std::mutex> lock(*mutex);
                if (!success) {
                    // handle failure
                    *result = false;

                    // cancel all tasks if just failed
                    if (!(*alreadyFailed)) {
                        *alreadyFailed = true;
                        cancelTasks    = true;
                        *exception     = t->exception;
                    }
                }

                tasksEnded->insert(t, true);
            }

            if (cancelTasks) {
                for (auto task : tasks) {
                    if (task == t)
                        continue;

                    task->cancel();

                    std::lock_guard<std::mutex> lock(*mutex);
                    tasksEnded->insert(task, true);
                }
            }

            allFinished->notify_one();
        };

        // get tasks ready
        for (auto task : tasks) {
            tasksEnded->insert(task, false);
            DelegateConnection startedConn, endedConn, terminatedConn;
            std::weak_ptr<AsyncTask> selfWeak = self;

            task->onStarted(
                    [selfWeak](std::shared_ptr<AsyncTask> t) {
                        auto s = selfWeak.lock();
                        if (s == nullptr)
                            return;

                        if (s->getState() != AsyncTask::State::Running) {
                            qDebug() << "Parallel cancelled before running its subtask:"
                                     << reinterpret_cast<intptr_t>(t.get());
                            t->setState(s->getState());
                        }
                    },
                    false,
                    &startedConn);

            task->onEnded([childEnded](std::shared_ptr<AsyncTask> t,
                                       bool success) { childEnded(t, success); },
                          false,
                          &endedConn);

            task->onTerminated(
                    [selfWeak](std::shared_ptr<AsyncTask> t) {
                        auto s = selfWeak.lock();
                        if (s == nullptr)
                            return;

                        if (!s->isTerminated())
                            s->terminate();
                    },
                    false,
                    &terminatedConn);

            startedConns[task].append(startedConn);
            endedConns[task].append(endedConn);
            terminatedConns[task].append(terminatedConn);
        }

        // this task failed (timeout, cancellation)
        self->onEnded(
                [tasks, selfFailed](std::shared_ptr<AsyncTask> t, bool success) {
                    if (!success) {
                        *selfFailed = true;
                        for (auto task : tasks)
                            task->cancel();
                    }
                },
                false,
                &selfEndedConn);

        // this task has been terminated
        self->onTerminated(
                [selfFailed, allFinished](std::shared_ptr<AsyncTask> t) {
                    *selfFailed = true;
                    allFinished->notify_one();
                },
                false,
                &selfTerminatedConn);

        // if self is already cancelled / timed out, do nothing else
        if (self->getState() != AsyncTask::State::Running) {
            *selfFailed = true;
            for (auto task : tasks)
                task->cancel();
        }

        if (!(*selfFailed)) {
            // adding extra thread - this control thread will do nothing
            ExtraThreadLock extraThread(shared_from_this());

            qDebug() << "Parallel (" << reinterpret_cast<intptr_t>(self.get())
                     << ") running its children.";

            // run all tasks asynchronously
            for (auto task : tasks)
                runTask(task, AsyncTask::getCurrentPriority());

            // wait all tasks to complete
            std::unique_lock<std::mutex> taskLock(*mutex);
            allFinished->wait(taskLock, [tasksEnded, self] {
                if (self->isTerminated())
                    return true;

                auto map = tasksEnded->values();
                for (bool state : map)
                    if (!state)
                        return false;

                return true;
            });
        }

        qDebug() << "Parallel (" << reinterpret_cast<intptr_t>(self.get())
                 << ") finished running all its children.";

        // disconnect remaining connections
        for (auto task : startedConns.keys())
            for (auto& conn : startedConns[task])
                task->removeStartedHandler(conn);

        for (auto task : endedConns.keys())
            for (auto& conn : endedConns[task])
                task->removeEndedHandler(conn);

        for (auto task : terminatedConns.keys())
            for (auto& conn : terminatedConns[task])
                task->removeTerminatedHandler(conn);

        startedConns.clear();
        endedConns.clear();
        terminatedConns.clear();

        self->removeEndedHandler(selfEndedConn);
        self->removeTerminatedHandler(selfTerminatedConn);

        if (*exception != nullptr) {
            qDebug() << "Parallel (" << reinterpret_cast<intptr_t>(self.get())
                     << ") rethrowing exception stored in the failed child task.";
            std::rethrow_exception(*exception);
        }

        //        qDebug() << "Parallel (" << reinterpret_cast<intptr_t>(self.get()) << ")
        //        returned.";
        return !(*selfFailed) && (*result);
    });

    return parallel;
}

std::shared_ptr<AsyncTask> AsyncTaskService::createAttempt(
        QList<std::shared_ptr<AsyncTask>> tasks)
{
    if (tasks.count() < 1)
        return this->createNoOpTask();
    if (tasks.count() == 1)
        return tasks.first();

    if (!checkOrigin(tasks))
        throw std::runtime_error("Cannot create Attempt of foreign tasks.");

    auto attempt = createTask<true>([this, tasks](std::shared_ptr<AsyncTask> self) {
        auto result           = std::make_shared<bool>(false);
        auto selfFailed       = std::make_shared<bool>(false);
        auto alreadySucceeded = std::make_shared<bool>(false);
        auto allFinished      = std::make_shared<std::condition_variable>();
        auto tasksEnded       = std::make_shared<QMap<AsyncTaskPtr, bool>>();
        auto mutex            = std::make_shared<std::mutex>();

        DelegateConnection selfEndedConn, selfTerminatedConn;
        QMap<AsyncTaskPtr, QList<DelegateConnection>> startedConns, endedConns, terminatedConns;

        if (!this->initSubtasks(self, tasks, "Attempt"))
            return false;

        auto childEnded = [result,
                           alreadySucceeded,
                           tasks,
                           mutex,
                           tasksEnded,
                           allFinished](AsyncTaskPtr t, bool success) {

            bool cancelTasks = false;

            {
                std::lock_guard<std::mutex> lock(*mutex);
                if (success) {
                    // handle success
                    *result = true;

                    // cancel all tasks if just succeeded
                    if (!(*alreadySucceeded)) {
                        *alreadySucceeded = true;
                        cancelTasks       = true;
                    }
                }

                tasksEnded->insert(t, true);
            }

            if (cancelTasks) {
                for (auto& task : tasks) {
                    if (task == t)
                        continue;

                    task->cancel();

                    std::lock_guard<std::mutex> lock(*mutex);
                    tasksEnded->insert(task, true);
                }
            }

            allFinished->notify_one();
        };

        // get tasks ready
        for (auto& task : tasks) {
            tasksEnded->insert(task, false);
            DelegateConnection startedConn, endedConn, terminatedConn;
            std::weak_ptr<AsyncTask> selfWeak = self;

            task->onStarted(
                    [selfWeak](std::shared_ptr<AsyncTask> t) {
                        auto s = selfWeak.lock();
                        if (s == nullptr)
                            return;

                        if (s->getState() != AsyncTask::State::Running) {
                            qDebug() << "Attempt cancelled before running its subtask:"
                                     << reinterpret_cast<intptr_t>(t.get());
                            t->setState(s->getState());
                        }
                    },
                    false,
                    &startedConn);

            task->onEnded([childEnded](std::shared_ptr<AsyncTask> t,
                                       bool success) { childEnded(t, success); },
                          false,
                          &endedConn);

            task->onTerminated([childEnded](
                                       std::shared_ptr<AsyncTask> t) { childEnded(t, false); },
                               false,
                               &terminatedConn);

            startedConns[task].append(startedConn);
            endedConns[task].append(endedConn);
            terminatedConns[task].append(terminatedConn);
        }

        // this task failed (timeout, cancellation)
        self->onEnded(
                [tasks, selfFailed](std::shared_ptr<AsyncTask> t, bool success) {
                    if (!success) {
                        *selfFailed = true;
                        for (auto task : tasks)
                            task->cancel();
                    }
                },
                false,
                &selfEndedConn);

        // this task has been terminated
        self->onTerminated(
                [selfFailed, allFinished](std::shared_ptr<AsyncTask> t) {
                    *selfFailed = true;
                    allFinished->notify_one();
                },
                false,
                &selfTerminatedConn);

        // if self is already cancelled / timed out, do nothing else
        if (self->getState() != AsyncTask::State::Running) {
            *selfFailed = true;
            for (auto task : tasks)
                task->cancel();
        }

        if (!(*selfFailed)) {
            // adding extra thread - this control thread will do nothing
            ExtraThreadLock extraThread(shared_from_this());

            qDebug() << "Attempt (" << reinterpret_cast<intptr_t>(self.get())
                     << ") running its children.";

            // run all tasks asynchronously
            for (auto& task : tasks)
                runTask(task, AsyncTask::getCurrentPriority());

            // wait all tasks to complete
            std::unique_lock<std::mutex> taskLock(*mutex);
            allFinished->wait(taskLock, [tasksEnded, self] {
                if (self->isTerminated())
                    return true;

                auto map = tasksEnded->values();
                for (bool state : map)
                    if (!state)
                        return false;

                return true;
            });
        }

        qDebug() << "Attempt (" << reinterpret_cast<intptr_t>(self.get())
                 << ") finished running all its children.";

        // disconnect remaining connections
        for (auto task : startedConns.keys())
            for (auto& conn : startedConns[task])
                task->removeStartedHandler(conn);

        for (auto task : endedConns.keys())
            for (auto& conn : endedConns[task])
                task->removeEndedHandler(conn);

        for (auto task : terminatedConns.keys())
            for (auto& conn : terminatedConns[task])
                task->removeTerminatedHandler(conn);

        startedConns.clear();
        endedConns.clear();
        terminatedConns.clear();

        self->removeEndedHandler(selfEndedConn);
        self->removeTerminatedHandler(selfTerminatedConn);

        //        qDebug() << "Attempt (" << reinterpret_cast<intptr_t>(self.get()) << ")
        //        returned.";
        return !(*selfFailed) && (*result);
    });

    return attempt;
}

bool AsyncTaskService::addTask(std::shared_ptr<AsyncTask> task) {
    //    qDebug() << "AsyncTaskService::addTask(" << reinterpret_cast<intptr_t>(task.get()) <<
    //    ")";

    if (task->taskService != shared_from_this())
        throw std::runtime_error("Cannot adopt foreign tasks.");

    task->initialize();
    {
        std::lock_guard<std::recursive_mutex> lock(taskMutex);
        auto taskIt = tasks.find(task);
        if (taskIt != tasks.end())
            return false;

        std::weak_ptr<AsyncTask> weak = task;
        connect(task.get(),
                &QObject::destroyed,
                this,
                [this, weak]() {
                    {
                        std::lock_guard<std::recursive_mutex> lock(taskMutex);
                        auto taskIt = tasks.find(weak);
                        if (taskIt != tasks.end())
                            tasks.erase(taskIt);
                    }

                    emit this->numberOfTasksChanged(getNumberOfRegisteredTasks(),
                                                    getNumberOfRunningTasks());
                },
                Qt::DirectConnection);

        tasks.insert(weak);
    }

    emit this->numberOfTasksChanged(getNumberOfRegisteredTasks(), getNumberOfRunningTasks());
    return true;
}

bool AsyncTaskService::deleteTask(std::shared_ptr<AsyncTask> task) {
    //    qDebug() << "AsyncTaskService::deleteTask(" << reinterpret_cast<intptr_t>(task.get())
    //    << ")";

    bool removed = false;
    {
        std::lock_guard<std::recursive_mutex> lock(taskMutex);
        auto taskIt = tasks.find(task);
        if (taskIt != tasks.end()) {
            tasks.erase(taskIt);
            removed = true;
        }
    }

    if (removed)
        task->prepareForDeletion();

    emit this->numberOfTasksChanged(getNumberOfRegisteredTasks(), getNumberOfRunningTasks());
    return removed;
}

void AsyncTaskService::deleteAllTasks() {
    //    qDebug() << "AsyncTaskService::deleteAllTasks()";
    {
        std::lock_guard<std::recursive_mutex> lock(taskMutex);
        auto copyOfTasks = tasks;
        for (auto& task : copyOfTasks) {
            auto t = task.lock();
            if (t != nullptr)
                deleteTask(t);
        }

        tasks.clear();
    }

    emit this->numberOfTasksChanged(getNumberOfRegisteredTasks(), getNumberOfRunningTasks());
}

int AsyncTaskService::getNumberOfAllExecutors() const {
    return pool->maxThreadCount();
}

int AsyncTaskService::getNumberOfIdleExecutors() const {
    return pool->maxThreadCount() - pool->activeThreadCount();
    //    return AsyncTask::aliveTasksCount; // debug purposes - actual task count
}

int AsyncTaskService::getNumberOfRegisteredTasks() const {
    std::lock_guard<std::recursive_mutex> tasksLock(taskMutex);
    return tasks.size();
}

int AsyncTaskService::getNumberOfRunningTasks() const {
    std::lock_guard<std::recursive_mutex> tasksLock(taskMutex);
    return runningTasks.count();
}

void AsyncTaskService::setRunOnMainThreadEnabled(bool enabled) {
    runOnMainThreadEnabled = enabled;
}

bool AsyncTaskService::getRunOnMainThreadEnabled() const {
    return runOnMainThreadEnabled;
}

void AsyncTaskService::runTask(std::shared_ptr<AsyncTask> task,
                               std::optional<AsyncTask::Priority> priority)
{
    runTask(task, true, priority);
}

void AsyncTaskService::runTaskSync(std::shared_ptr<AsyncTask> task,
                                   bool rethrowException,
                                   std::optional<AsyncTask::Priority> priority)
{
    if (!runOnMainThreadEnabled &&
        QThread::currentThread() == QCoreApplication::instance()->thread())
    {
        throw std::runtime_error(
                "Tried to run a task synchronously on the main thread. "
                "This is not supported due to performance reasons.");
    }

    std::exception_ptr exception = nullptr;
    if (rethrowException) {
        task->onFailed([&exception](auto t) { exception = t->getStoredException(); }, false);
    }

    runTask(task, false, priority);

    if (rethrowException && exception != nullptr)
        std::rethrow_exception(exception);
}

void AsyncTaskService::runTask(std::shared_ptr<AsyncTask> task,
                               bool async,
                               std::optional<AsyncTask::Priority> priority)
{
    if (task->taskService != shared_from_this())
        throw std::runtime_error("Cannot run foreign tasks.");

    if (task->isWaitingForDeletion())
        throw std::runtime_error("Cannot run AsyncTask under deletion.");

    if (task->getState() == AsyncTask::State::Running)
        throw std::runtime_error("Cannot re-run AsyncTask while it is still running.");

    task->setState(AsyncTask::State::Starting);
    if (task->isNoOpTask()) {
        qDebug() << "Task (" << reinterpret_cast<intptr_t>(task.get())
                 << ") does nothing... so it's finished.";
        task->setState(AsyncTask::State::Finishing);
        return;
    }

    //    qDebug() << "Task" << reinterpret_cast<intptr_t>(task.get()) << "is called "
    //             << (async ? "asynchronously" : "synchronously") << " from thread:" <<
    //             QThread::currentThreadId();

    QObject* timerGuard = nullptr;
    QTimer* timer       = nullptr;

    if (task->getTimeout() > 0 && async) {
        qDebug() << "Task has a timeout:" << task->getTimeout();
        timerGuard = new QObject();
        timer      = new QTimer();
        timer->moveToThread(mainThread);
        timer->setSingleShot(true);
        timer->setInterval(task->getTimeout());
        connect(timer, &QTimer::timeout, [task]() {
            qDebug() << "Task timeout: " << reinterpret_cast<intptr_t>(task.get());
            task->setState(AsyncTask::State::TimingOut);
        });

        // timerGuard is for signal transmission to thread, which timer belongs to
        connect(timerGuard, SIGNAL(destroyed(QObject*)), timer, SLOT(deleteLater()));

        // start the timer on its thread
        QObject* timerStarter = new QObject();
        connect(timerStarter, SIGNAL(destroyed(QObject*)), timer, SLOT(start()));
        delete timerStarter;
    }

    // calculate inherited or desired priority
    auto effectivePriority = priority ? priority.value() : AsyncTask::getCurrentPriority();

    auto function = [this, task, timerGuard, effectivePriority]() {
        if (task->isWaitingForDeletion()) {
            qDebug() << "Task (" << reinterpret_cast<intptr_t>(this)
                     << ") has been removed during async dispatching.";
            return;
        }

        {
            std::lock_guard<std::recursive_mutex> tasksLock(taskMutex);
            this->runningTasks.insert(task);
        }
        emit this->numberOfTasksChanged(getNumberOfRegisteredTasks(),
                                        getNumberOfRunningTasks());

        //        qDebug() << "AsyncTaskService running task" <<
        //        reinterpret_cast<intptr_t>(task.get())
        //                 << " state:" << task->getStateString() << " deleted:" <<
        //                 task->isWaitingForDeletion()
        //                 << "thread:" << QThread::currentThreadId();

        auto clear = util::finally([this, task]() {
            {
                std::lock_guard<std::recursive_mutex> tasksLock(taskMutex);
                this->runningTasks.remove(task);
            }
            emit this->numberOfTasksChanged(getNumberOfRegisteredTasks(),
                                            getNumberOfRunningTasks());

            task->clearMaintainedObjects(false);

            std::lock_guard<std::recursive_mutex> lock(task->stateMutex);
            if (task->getState() != AsyncTask::State::Terminated && !task->subtasks.empty()) {
                qDebug() << "Clearing subtasks of" << reinterpret_cast<intptr_t>(task.get());
                task->subtasks.clear();
            }
        });

        std::optional<bool> ok;
        try {
            // this call is exception safe if called on initialized task instances
            ok = task->runCore(effectivePriority);

        } catch (std::runtime_error& e) {
            qWarning() << "AsyncTask" << reinterpret_cast<intptr_t>(task.get())
                       << "is prevented from being run (maybe because it has already been "
                          "deleted).";
        }

        if (ok && !ok.value()) {
#ifdef DEBUG_SAVE_STACKTRACE
            auto stream = qDebug() << "AsyncTask (" << reinterpret_cast<intptr_t>(task.get())
                                   << ") was constructed in:" << endl;
            for (auto& s : task->constructedFrom)
                stream << "   " << s.toUtf8().data() << endl;
#endif

            emitExceptionSignals(task);
        }

        // stop and delete timer for task
        if (timerGuard != nullptr)
            delete timerGuard;

        if (ok)
            task->setState(ok.value() ? AsyncTask::State::Finishing
                                      : AsyncTask::State::Failing);
    };

    //    qDebug() << "AsyncTaskService enqued task" << reinterpret_cast<intptr_t>(task.get())
    //             << "async:" << async
    //             << " state:" << task->getStateString() << " deleted:" <<
    //             task->isWaitingForDeletion()
    //             << "thread:" << QThread::currentThreadId();

    if (async) {
        auto runnable = std::make_shared<AsyncRunnable*>();
        *runnable     = new AsyncRunnable([function, runnable] {
            function();
            delete *runnable;
        });
        pool->start(*runnable, static_cast<int>(effectivePriority));
    } else
        function();
}

void AsyncTaskService::cancelTask(std::shared_ptr<AsyncTask> task) {
    if (task->taskService != shared_from_this())
        throw std::runtime_error("Cannot cancel foreign tasks.");

    task->setState(AsyncTask::State::Cancelling);
}

void AsyncTaskService::terminateTask(std::shared_ptr<AsyncTask> task) {
    if (task->taskService != shared_from_this())
        throw std::runtime_error("Cannot terminate foreign tasks.");

    task->setState(AsyncTask::State::Terminated);
}

QThread* AsyncTaskService::getMainThread() const {
    return mainThread;
}

void AsyncTaskService::setMainThread(QThread* value) {
    mainThread = value;
}

AsyncTaskService::ExtraThreadLock::ExtraThreadLock(std::shared_ptr<AsyncTaskService> service)
    : service(service)
{
    service->modifyWorkersCount(+1);
}

AsyncTaskService::ExtraThreadLock::~ExtraThreadLock() {
    service->modifyWorkersCount(-1);
}

AsyncTaskService::AsyncRunnable::AsyncRunnable(std::function<void()> function)
    : QRunnable(), function(function)
{
    setAutoDelete(false);
}

void AsyncTaskService::AsyncRunnable::run() {
    if (function != nullptr)
        function();
}
