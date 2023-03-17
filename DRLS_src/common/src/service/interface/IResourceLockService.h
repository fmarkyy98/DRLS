#pragma once

#include <QtCore>

#include "common/src/service/interface/IService.h"
#include "common/src/AsyncTask.h"
#include "common/src/LockableResource.h"
#include "common/src/CallerContext.h"

#include "utils/Callback.h"

namespace common {
namespace details {

class IResourceLockServiceMeta : public common::details::IServiceMeta {
    Q_OBJECT

signals:
    void locksChanged();
};

}  // namespace details

class IResourceLockService
        : public common::ITypedService<IResourceLockService, details::IResourceLockServiceMeta>
{
public:

    virtual ~IResourceLockService() {}

    virtual AsyncFuncPtr<bool> acquireLocks(QMap<common::LockableResource,common::ResourceLockType> resources, common::CallerContext context) = 0;

    virtual AsyncFuncPtr<bool> renewLocksIfPossible(QMap<common::LockableResource,common::ResourceLockType> resources, common::CallerContext context) = 0;

    virtual AsyncTaskPtr releaseLocks(QMap<common::LockableResource,common::ResourceLockType> resources, common::CallerContext context) = 0;

    virtual AsyncFuncPtr<bool> acquireSystemLocks(QMap<common::LockableResource,common::ResourceLockType> resources, QString tag) = 0;

    virtual AsyncTaskPtr releaseSystemLocks(QMap<common::LockableResource,common::ResourceLockType> resources, QString tag) = 0;

    virtual AsyncFuncPtr<QSet<QPair<QString,QString>>> getConcurrentLockOwnerNames(QMap<common::LockableResource,common::ResourceLockType> resources, common::CallerContext context) = 0;

    virtual AsyncTaskPtr listenLocksChanged(QString token, util::Callback<void()> callback, QList<db::EntityType> filter = {}, bool ignoreOwnedLocks = true) = 0;

    virtual AsyncTaskPtr stopListenLocksChanged(util::Callback<void()> callback) = 0;

    virtual AsyncFuncPtr<QMap<int,QString>> getLocks(db::EntityType entityType) = 0;
};
}
