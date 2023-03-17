#include "ThreadHelper.h"

using namespace util;

QList<QThread*> ThreadHelper::eventThreads;

ContextDispatcher::ContextDispatcher(std::shared_ptr<ContextReceiver> receiver,
                                     DispatchMethod method)
    : _receiver(receiver)
    , _method(method)
{
}

void ContextDispatcher::doDispatch() {
    auto sfthis = shared_from_this(); // keep 'this' till the end
    auto rec = _receiver.lock();
    if (rec == nullptr) {
        qWarning() << "ContextDispatcher: Receiver is null.";
        emit finished();
        return;
    }

    bool sameThread =  (this->thread() == rec->thread());
    Qt::ConnectionType connType = Qt::ConnectionType::DirectConnection;
    if (!sameThread) {
        if (_method == DispatchMethod::Blocking)
            connType = Qt::ConnectionType::BlockingQueuedConnection;
        else
            connType = Qt::ConnectionType::QueuedConnection;
    }

    auto connection = QObject::connect(this,
                                       &ContextDispatcher::dispatch,
                                       rec.get(),
                                       &ContextReceiver::receive,
                                       connType);

    // transfer finished signal
    QObject::connect(rec.get(),
                     &ContextReceiver::finished,
                     this,
                     &ContextDispatcher::finished,
                     Qt::ConnectionType::DirectConnection);

//    qDebug() << "Dispatching from Dispatcher (" << reinterpret_cast<intptr_t>(this) << ") to"
//             << "Receiver (" << reinterpret_cast<intptr_t>(rec.get()) << ")"
//             << (sameThread ? " - same thread" : " - different threads");

//    qDebug() << "Context dispatching... receiver:" << reinterpret_cast<intptr_t>(rec.get())
//             << "thread:" << reinterpret_cast<intptr_t>(rec.get()->thread());
    rec->keepAlive(rec);
    emit dispatch();
//    qDebug() << "Context dispatched - receiver:" << reinterpret_cast<intptr_t>(rec.get())
//             << "thread:" << reinterpret_cast<intptr_t>(rec.get()->thread());

    QObject::disconnect(connection);
}

bool ContextDispatcher::isFinished() const {
    auto rec = _receiver.lock();
    if (rec == nullptr) {
        qWarning() << "ContextDispatcher: Receiver is null.";
        return true;
    }

    return rec->isFinished();
}

ContextReceiver::ContextReceiver(std::function<void ()> method)
    : QObject()
    , method(method)
    , keepAliveObjects()
    , receiveFinished(false)
{
//    qDebug() << "receiver created:" << reinterpret_cast<intptr_t>(this);
}

ContextReceiver::~ContextReceiver() {
    //    qDebug() << "receiver died:" << reinterpret_cast<intptr_t>(this);
}

bool ContextReceiver::isFinished() const {
    return this->receiveFinished;
}

void ContextReceiver::keepAlive(std::shared_ptr<const QObject> obj) {
    std::lock_guard<std::mutex> lock(keepAliveMutex);
    this->keepAliveObjects.append(obj);
}

void ContextReceiver::receive() {
    auto sfthis = shared_from_this();
//    qDebug() << "Context receiving..." << reinterpret_cast<intptr_t>(this);

    try {
        if (method)
            method();
    } catch (const std::exception &e) {
        qCritical("Exception occurred while running ContextReceiver::receive %ld. "
                  "Message: %s", reinterpret_cast<intptr_t>(this), e.what());
    } catch (...) {
        qCritical("Unknown error occurred while running ContextReceiver::receive %ld.",
                  reinterpret_cast<intptr_t>(this));
    }

//    qDebug() << "Context received." << reinterpret_cast<intptr_t>(this);

    {
        // let it die... :(
        std::lock_guard<std::mutex> lock(keepAliveMutex);
        this->keepAliveObjects.clear();
    }

    this->receiveFinished = true;
    emit this->finished();
}

void ThreadHelper::checkTargetThread(QThread* target) {
//    qDebug() << "ThreadHelper: dispatch requested from thread " << QThread::currentThreadId();

    // target thread must be running an event loop
    if (QCoreApplication::instance()->thread() != target && !eventThreads.contains(target))
        throw std::runtime_error("ThreadHelper: dispatch requested to thread which does not run an event loop.");
}

void ThreadHelper::addEventThread(QThread *thread) {
    eventThreads.append(thread);
}

bool ThreadHelper::removeEventThread(QThread *thread) {
    return eventThreads.removeOne(thread);
}
