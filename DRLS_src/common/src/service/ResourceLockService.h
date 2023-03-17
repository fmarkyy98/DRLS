#pragma once

#include <mutex>
#include <shared_mutex>
#include <optional>

#include "common/src/service/interface/IResourceLockService.h"
#include "common/src/service/EntityService.h"
#include "common/src/service/AsyncTaskService.h"

namespace db {
class Administrator;
class Fruit;
class User;
}

namespace common {

class ResourceLockService
    : public QObject
    , public IResourceLockService
{
    Q_OBJECT

private:
    struct ResourceLock {
        QDateTime acquired;
        QDateTime timeout;
        QString tag;
        QString resource;
         ResourceLockType type;

        bool operator==(const ResourceLock& other) const;
        bool operator<(const ResourceLock& other) const;

        int adminId;
        QString adminToken;
    };

public:
    ResourceLockService(std::shared_ptr<EntityService> entityService,
                        std::shared_ptr< AsyncTaskService> asyncTaskService);

    AsyncFuncPtr<bool> acquireLocks(
            QMap< LockableResource,  ResourceLockType> resources,
             CallerContext context) override;

    AsyncFuncPtr<bool> renewLocksIfPossible(
            QMap< LockableResource,  ResourceLockType> resources,
             CallerContext context) override;

    AsyncTaskPtr releaseLocks(
            QMap< LockableResource,  ResourceLockType> resources,
             CallerContext context) override;


    AsyncFuncPtr<bool> acquireSystemLocks(
            QMap< LockableResource,  ResourceLockType> resources,
            QString tag) override;

    AsyncTaskPtr releaseSystemLocks(
            QMap< LockableResource,  ResourceLockType> resources,
            QString tag) override;

    AsyncFuncPtr<QSet<QPair<QString, QString>>> getConcurrentLockOwnerNames(
            QMap< LockableResource,  ResourceLockType> resources,
             CallerContext context) override;

    AsyncTaskPtr listenLocksChanged(QString token,
                                    util::Callback<void()> callback,
                                    QList<db::EntityType> filter = {},
                                    bool ignoreOwnedLocks        = true) override;
    AsyncTaskPtr stopListenLocksChanged(util::Callback<void()> callback) override;

    AsyncFuncPtr<QMap<int, QString>> getLocks(db::EntityType entityType) override;

signals:
    // this signal is considered internal, and supports only direct connections
    void locksChanged(QList<QPair<std::optional<ResourceLock>, std::optional<ResourceLock>>>
                              changedLocks);

private:
    static bool compatible( ResourceLockType existing,  ResourceLockType lock);
    static QString getResourceName( LockableResource resource);
    QMap<ResourceLockService::ResourceLock, bool> getConcurrentLocks(
             LockableResource resource,
             ResourceLockType lock) const;
    bool checkIfLockIsValid(ResourceLockService::ResourceLock lock,
                             LockableResource res) const;

    std::optional<QMap< LockableResource,  ResourceLockType>> getResourcesToLock(
            const QMap< LockableResource,  ResourceLockType>& resources,
            QList<QPair<std::optional<ResourceLock>, std::optional<ResourceLock>>>&
                    changedLocks,
            const QDateTime& now,
            std::function<bool(ResourceLock lock)> isLockOurs);

    // debug
    void printLocks(const  CallerContext& context, AsyncTaskPtr task);

    void connectToChangedSignal();

    std::shared_ptr<db::Administrator> getAdmynByUsername(const QString& username) const;

private:
    static const int SecondsToLive;

    std::shared_ptr<EntityService> entityService_;
    std::shared_ptr<AsyncTaskService> asyncTaskService_;

    std::recursive_mutex lockMutex_;

    QMultiMap<QString, QPair<QPair<util::Callback<void()>, QList<db::EntityType>>, bool>>
            locksChangedCallbacks_;
    std::recursive_mutex changedMutex_;

    QMap<int, QList<ResourceLock>> locksByAdmins_;
};

}  // namespace common
