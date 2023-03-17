#pragma once

#include <QtCore>
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>


namespace util {
class ContextReceiver;

enum class DispatchMethod { Blocking, Async };

class ContextDispatcher
    : public QObject
    , public std::enable_shared_from_this<ContextDispatcher>
{
    Q_OBJECT

public:
    ContextDispatcher(std::shared_ptr<ContextReceiver> receiver, DispatchMethod method);

    void doDispatch();
    bool isFinished() const;

signals:
    void dispatch();
    void finished();

private:
    std::weak_ptr<ContextReceiver> _receiver;
    DispatchMethod _method;
};

class ContextReceiver
    : public QObject
    , public std::enable_shared_from_this<ContextReceiver>
{
    Q_OBJECT

public:
    ContextReceiver(std::function<void()> method);
    ~ContextReceiver();

    bool isFinished() const;

public slots:
    void keepAlive(std::shared_ptr<const QObject> obj);
    void receive();

signals:
    void finished();

private:
    std::function<void()> method;
    QList<std::shared_ptr<const QObject>>
            keepAliveObjects;  // to keep alive when dispatching in async mode
    std::mutex keepAliveMutex;
    std::atomic_bool receiveFinished;
};

class ThreadHelper {
    static QList<QThread*> eventThreads;

public:
    static void checkTargetThread(QThread* target);
    static void addEventThread(QThread* thread);
    static bool removeEventThread(QThread* thread);

    template<typename... ArgsT>
    inline static void dispatchToThread(QThread* target,
                                        DispatchMethod method,
                                        std::function<void(ArgsT...)> function,
                                        ArgsT... args)
    {
        if (QThread::currentThread() == target) {
            //            qDebug() << "ThreadHelper: dispatch to self " <<
            //            QThread::currentThreadId();
            function(args...);
            return;
        }

        checkTargetThread(target);
        auto fun = [function, args...]() {
            //            qDebug() << "ThreadHelper: dispatched to thread " <<
            //            QThread::currentThreadId();
            function(args...);
        };

        auto dispatcher = std::make_shared<ContextDispatcher>();
        auto receiver   = std::make_shared<ContextReceiver>(fun);
        receiver->moveToThread(target);
        dispatcher->doDispatch(receiver, method);
    }

    inline static void dispatchToThread(QThread* target,
                                        DispatchMethod method,
                                        std::function<void()> function)
    {
        if (QThread::currentThread() == target) {
            //            qDebug() << "ThreadHelper: dispatch to self " <<
            //            QThread::currentThreadId();
            function();
            return;
        }

        checkTargetThread(target);
        auto fun = [function]() {
            //            qDebug() << "ThreadHelper: dispatched to thread " <<
            //            QThread::currentThreadId();
            function();
        };

        auto receiver   = std::make_shared<ContextReceiver>(fun);
        auto dispatcher = std::make_shared<ContextDispatcher>(receiver, method);
        receiver->moveToThread(target);
        dispatcher->doDispatch();
    }
};
}  // namespace common
