#include "ResourceLockService.h"

#include "utils/Finally.h"


#include "persistence/Administrator.h"
#include "persistence/User.h"

using namespace common;

bool ResourceLockService::ResourceLock::operator==(const ResourceLock& other) const {
    return (this->adminId == other.adminId && this->adminToken == other.adminToken &&
            this->acquired == other.acquired && this->timeout == other.timeout &&
            this->tag == other.tag && this->resource == other.resource &&
            this->type == other.type);
}

bool ResourceLockService::ResourceLock::operator<(const ResourceLock& other) const {
    if (this->adminId != other.adminId)
        return this->adminId < other.adminId;

    if (this->adminToken != other.adminToken)
        return this->adminToken < other.adminToken;

    if (this->acquired != other.acquired)
        return this->acquired < other.acquired;

    if (this->timeout != other.timeout)
        return this->timeout < other.timeout;

    if (this->tag != other.tag)
        return this->tag < other.tag;

    if (this->resource != other.resource)
        return this->resource < other.resource;

    if (static_cast<int>(this->type) != static_cast<int>(other.type))
        return static_cast<int>(this->type) < static_cast<int>(other.type);

    return false;
}

const int ResourceLockService::SecondsToLive = 120;

std::shared_ptr<ResourceLockService> ResourceLockService::instance_;

std::shared_ptr<ResourceLockService> ResourceLockService::getInstance() {
    if (instance_ == nullptr)
        instance_ = std::shared_ptr<ResourceLockService>(
            new ResourceLockService(common::EntityService::getInstance(),
                                    common::AsyncTaskService::getInstance()));

    return instance_;
}

ResourceLockService::ResourceLockService(
        std::shared_ptr<EntityService> entityService,
        std::shared_ptr<common::AsyncTaskService> asyncTaskService)
    : entityService_(entityService)
    , asyncTaskService_(asyncTaskService)
{
    connectToChangedSignal();
}

AsyncTaskPtr ResourceLockService::listenLocksChanged(QString token,
                                                     util::Callback<void()> callback,
                                                     QList<db::EntityType> filter,
                                                     bool ignoreOwnedLocks)
{
    return asyncTaskService_->createTask(
            [this, token, &callback, filter, ignoreOwnedLocks](AsyncTaskPtr f) {

                if (callback == nullptr)
                    throw std::invalid_argument("Callback is not specified.");

                // if callback is cleaned up, we should forget it
                const util::Callback<void()> originalCallback{token, std::move(callback)};
                callback = std::move<util::Callback<void()>>({token,
                                                              [this,
                                                              token,
                                                              callback,
                                                              filter,
                                                              ignoreOwnedLocks,
                                                              f,
                                                              originalCallback]() {
                    originalCallback();

                    std::lock_guard<std::recursive_mutex> guard(lockMutex_);

                    locksChangedCallbacks_.remove(token,
                                                 {{callback, filter},
                                                  ignoreOwnedLocks});
                }});

                {
                    std::lock_guard<std::recursive_mutex> guard(lockMutex_);

                    locksChangedCallbacks_.insert(token,
                                                        {{callback, filter},
                                                         ignoreOwnedLocks});
                }
            });
}

AsyncTaskPtr ResourceLockService::stopListenLocksChanged(util::Callback<void()> callback) {
    return asyncTaskService_->createTask([this, callback](AsyncTaskPtr f) {
        if (callback == nullptr)
            throw std::invalid_argument("Callback is not specified.");

        std::scoped_lock guard(lockMutex_);
        auto callbackToEraseIt =
                std::find_if(locksChangedCallbacks_.begin(),
                             locksChangedCallbacks_.end(),
                             [callback]
                             (QPair<QPair<std::function<void()>, QList<db::EntityType>>,
                                    bool> p) {
            return &p.first.first == &callback;
        });

        locksChangedCallbacks_.erase(callbackToEraseIt);
    });
}

void ResourceLockService::connectToChangedSignal() {
    connect(this,
            &ResourceLockService::locksChanged,
            [this](QList<QPair<std::optional<ResourceLock>, std::optional<ResourceLock>>>
                           changedLocks) {
                QMultiMap<QString, QPair<QPair<util::Callback<void()>,
                                               QList<db::EntityType>>,
                                         bool>>
                    callbacksWithToken;

                {
                    std::lock_guard<std::recursive_mutex> guard(lockMutex_);
                    if (this->locksChangedCallbacks_.empty())
                        return;

                    callbacksWithToken = locksChangedCallbacks_;
                }

                if (callbacksWithToken.empty())
                    return;

                for (auto data : changedLocks) {
                    QString adminToken = "";

                    if (data.first != std::nullopt)
                        adminToken = data.first->adminToken;
                    else if (data.second != std::nullopt)
                        adminToken = data.second->adminToken;

                    for (auto token : callbacksWithToken.uniqueKeys()) {
                        auto values = callbacksWithToken.values(token);
                        for (auto value : values) {
                            bool ignoreOwnedLocks = value.second;
                            if (ignoreOwnedLocks && token == adminToken)
                                continue;

                            auto callback   = value.first;
                            bool doCallback = false;
                            if (!callback.second.isEmpty()) {
                                QList<QString> typeListString;
                                for (auto type : callback.second)
                                    typeListString.append(db::entityTypeToString(type));

                                if (data.first != std::nullopt) {
                                    QString resource = data.first->resource;
                                    int pos          = resource.indexOf("#");
                                    if (pos == -1)
                                        pos = resource.indexOf("*");
                                    doCallback = doCallback ||
                                                 (pos >= 0 &&
                                                  typeListString.contains(resource.left(pos)));
                                }
                                if (data.second != std::nullopt) {
                                    QString resource = data.second->resource;
                                    int pos          = resource.indexOf("#");
                                    if (pos == -1)
                                        pos = resource.indexOf("*");
                                    doCallback = doCallback ||
                                                 (pos >= 0 &&
                                                  typeListString.contains(resource.left(pos)));
                                }
                            }

                            if (doCallback || callback.second.isEmpty())
                                callback.first();
                        }
                    }
                }

                emit meta()->locksChanged();
            });
}

AsyncFuncPtr<bool> ResourceLockService::acquireLocks(
        std::map<common::LockableResource, common::ResourceLockType> resources,
        common::CallerContext context)
{
    return asyncTaskService_->createFunction<bool>([this, resources, context]
                                                   (AsyncFuncPtr<bool> f) {
        QList<QPair<std::optional<ResourceLock>, std::optional<ResourceLock>>> changedLocks;

        auto fin = util::finally([this, &changedLocks] {
            if (changedLocks.size() > 0)
                emit locksChanged(changedLocks);
        });

        {
            std::lock_guard<std::recursive_mutex> guard(lockMutex_);
            QDateTime now = QDateTime::currentDateTime();

            auto resourcesToLock =
                    getResourcesToLock(resources,
                                       changedLocks,
                                       now,
                                       [this, context](ResourceLock lock) -> bool
            {
                auto lockOwner =
                        entityService_->getById<db::Administrator>(lock.adminId);

                entityService_->getAll<db::User>();

                return lockOwner != nullptr &&
                       lockOwner->getUsername() == context.username &&
                       lock.adminToken == context.token;
            });

            if (!resourcesToLock) {
                f->setResult(false);
                return;
            }

            auto admin = getAdmynByUsername(context.username);
            if (admin == nullptr)
                throw std::invalid_argument("Administrator does not exist.");

            // acquire new locks
            for (const auto& [res, type] : resourcesToLock.value()) {
                auto lock     = ResourceLock();
                lock.acquired = now;
                lock.type     = resources.at(res) == common::ResourceLockType::Read
                                    ? common::ResourceLockType::Read
                                    : common::ResourceLockType::Write;
                lock.resource   = getResourceName(res);
                lock.timeout    = now.addSecs(SecondsToLive);
                lock.adminId    = admin->getId();
                lock.adminToken = context.token;
                locksByAdmins_[admin->getId()].append(lock);

                changedLocks.append({std::nullopt, lock});
            }

            f->setResult(true);
        }

    });
}

AsyncFuncPtr<bool> ResourceLockService::renewLocksIfPossible(
        std::map<common::LockableResource, common::ResourceLockType> resources,
        common::CallerContext context)
{
    return asyncTaskService_->createFunction<bool>([this,
                                                    resources,
                                                    context](AsyncFuncPtr<bool> f) {
        QList<QPair<std::optional<ResourceLock>, std::optional<ResourceLock>>> changedLocks;

        auto fin = util::finally([this, &changedLocks] {
            if (changedLocks.size() > 0)
                emit locksChanged(changedLocks);
        });

        {
            std::lock_guard<std::recursive_mutex> guard(lockMutex_);
            QDateTime now = QDateTime::currentDateTime();

            auto resourcesToLock = getResourcesToLock(
                    resources, changedLocks, now, [this, context](ResourceLock lock) -> bool {
                        auto lockOwner =
                                entityService_->getById<db::Administrator>(lock.adminId);
                        return lockOwner != nullptr &&
                               lockOwner->getUsername() == context.username &&
                               lock.adminToken == context.token;
                    });

            if (!resourcesToLock || resourcesToLock->size() > 0) {  //if we would need to lock something it is failed
                f->setResult(false);
                return;
            }
            f->setResult(true);
            return;
        }

    });
}

AsyncTaskPtr ResourceLockService::releaseLocks(
        std::map<common::LockableResource, common::ResourceLockType> resources,
        common::CallerContext context)
{
    return asyncTaskService_->createTask([this,
                                          resources,
                                          context](AsyncTaskPtr f) {
        //        qDebug() << "[LOCKS] Locks before releasing...";
        //        this->printLocks(context);
        QList<QPair<std::optional<ResourceLock>, std::optional<ResourceLock>>> changedLocks;
        {
            std::lock_guard<std::recursive_mutex> guard(lockMutex_);

            auto admin = getAdmynByUsername(context.username);
            if (admin == nullptr)
                throw std::invalid_argument("Administrator does not exist.");

            for (const auto& [res, type] : resources) {
                auto resourceName = getResourceName(res);

                for (auto lock : locksByAdmins_[admin->getId()]) {
                    if (lock.resource == resourceName && lock.type == type &&
                        lock.adminToken == context.token) {
                        locksByAdmins_[admin->getId()].removeOne(lock);
                        changedLocks.append({lock, std::nullopt});
                    }
                }
            }

            //        qDebug() << "[LOCKS] Locks after releasing...";
            //      this->printLocks(context,f);}
        }
        if (changedLocks.size() > 0)
            emit locksChanged(changedLocks);

    });
}

AsyncFuncPtr<bool> ResourceLockService::acquireSystemLocks(
        std::map<common::LockableResource, common::ResourceLockType> resources,
        QString tag)
{
    return asyncTaskService_->createFunction<bool>([this,
                                                    resources,
                                                    tag](AsyncFuncPtr<bool> f) {
        QList<QPair<std::optional<ResourceLock>, std::optional<ResourceLock>>> changedLocks;

        auto fin = util::finally([this, &changedLocks] {
            if (changedLocks.size() > 0)
                emit locksChanged(changedLocks);
        });

        {
            std::lock_guard<std::recursive_mutex> guard(lockMutex_);
            auto now = QDateTime::currentDateTime();

            auto resourcesToLock =
                    getResourcesToLock(resources, changedLocks, now, [tag](auto lock) {
                        return lock.tag == tag;
                    });

            if (!resourcesToLock) {
                f->setResult(false);
                return;
            }

            // acquire new locks
            for (const auto& [res, type] : resourcesToLock.value()) {
                auto lock     = ResourceLock();
                lock.acquired = now;
                lock.type     = resources.at(res) == common::ResourceLockType::Read
                                    ? common::ResourceLockType::Read
                                    : common::ResourceLockType::Write;
                lock.resource = getResourceName(res);
                lock.timeout  = now.addSecs(SecondsToLive);
                // system locks have -1 admin id
                lock.adminId    = -1;
                lock.adminToken = "";
                lock.tag        = tag;
                locksByAdmins_[-1].append(lock);
            }

            f->setResult(true);
        }
        emit meta()->locksChanged();

    });
}

AsyncTaskPtr ResourceLockService::releaseSystemLocks(
        std::map<common::LockableResource, common::ResourceLockType> resources,
        QString tag)
{
    return asyncTaskService_->createTask([this,
                                          resources,
                                          tag](AsyncTaskPtr f) {
        {
            std::lock_guard<std::recursive_mutex> guard(lockMutex_);

            for (const auto& [res, type] : resources) {
                auto resourceName = getResourceName(res);

                for (auto lock : locksByAdmins_[-1]) {
                    if (lock.resource == resourceName && lock.type == type && lock.tag == tag)
                        locksByAdmins_[-1].removeOne(lock);
                }
            }
        }
        emit meta()->locksChanged();

    });
}

AsyncFuncPtr<std::map<int, QString>> ResourceLockService::getLocks(db::EntityType entityType) {
    return asyncTaskService_->createFunction<std::map<int, QString>>(
            [this, entityType](AsyncFuncPtr<std::map<int, QString>> f) {
                std::lock_guard<std::recursive_mutex> guard(lockMutex_);

                std::map<int, QString> res;

                QList<ResourceLock> locks;
                for (const auto& [_, lockList] : locksByAdmins_) {
                    for (auto lock : lockList) {
                        if (lock.type == common::ResourceLockType::Write &&
                            lock.resource.contains(db::entityTypeToString(entityType) + "#"))
                            locks.append(lock);
                    }
                }

                for (auto lock : locks) {
                    int pos = lock.resource.indexOf("#") + 1;
                    int id  = lock.resource.mid(pos).toInt();
                    if (id > 0 && lock.adminId > 0) {
                        auto admin = entityService_->getById<db::Administrator>(lock.adminId);
                        res[id] = admin->getUsername();
                    }
                }
                f->setResult(res);

            });
}

AsyncFuncPtr<QSet<QPair<QString, QString>>> ResourceLockService::getConcurrentLockOwnerNames(
        std::map<common::LockableResource, common::ResourceLockType> resources,
        common::CallerContext context)
{
    return asyncTaskService_->createFunction<
            QSet<QPair<QString, QString>>>([this,
                                            resources,
                                            context]
                                       (AsyncFuncPtr<QSet<QPair<QString, QString>>> f) {
        std::lock_guard<std::recursive_mutex> guard(lockMutex_);

        QDateTime now = QDateTime::currentDateTime();
        QSet<QPair<QString, QString>> admins;

        for (const auto& [res, type] : resources) {
            auto existing = getConcurrentLocks(res, type);
            for (const auto& [lock, _] : existing) {
                // if lock is expired, remove it
                if (lock.timeout < now) {
                    locksByAdmins_[lock.adminId].removeOne(lock);
                    continue;
                }

                // lock is compatible
                if (existing[lock])
                    continue;

                auto lockOwner = entityService_->getById<db::Administrator>(lock.adminId);
                if (lock.adminToken != context.token) {
                    if (lockOwner == nullptr) {
                        auto systemName = QString{"[%1]"}.arg(
                                QT_TRANSLATE_NOOP("ResourceLock", "System"));
                        admins.insert({systemName, systemName});
                    } else {
                        admins.insert({lockOwner->getUsername(), lockOwner->getFullName()});
                    }
                }
            }
        }

        f->setResult(admins);

    });
}

bool ResourceLockService::compatible(common::ResourceLockType existing,
                                     common::ResourceLockType lock)
{
    return existing == common::ResourceLockType::Read && lock == common::ResourceLockType::Read;
}

QString ResourceLockService::getResourceName(common::LockableResource resource) {
    if (resource.targetType == common::LockableResource::TargetType::Entity)
        return db::entityTypeToString(resource.targetSet) + "#" + QString::number(resource.targetId);

    throw std::runtime_error("Unknown resource type");
}

std::map<ResourceLockService::ResourceLock, bool> ResourceLockService::getConcurrentLocks(
        common::LockableResource resource,
        common::ResourceLockType lock) const
{
    std::map<ResourceLockService::ResourceLock, bool> result;

    auto resourceName = getResourceName(resource);

    auto lockType = lock == common::ResourceLockType::Read ? common::ResourceLockType::Read
                                                           : common::ResourceLockType::Write;

    for (const auto& [adminId, lockList] : locksByAdmins_) {
        for (auto currentLock : lockList) {
            if (checkIfLockIsValid(currentLock, resource)) {
                result[currentLock] = compatible(currentLock.type, lockType);
            }
        }
    }

    return result;
}

bool ResourceLockService::checkIfLockIsValid(ResourceLockService::ResourceLock lock,
                                             common::LockableResource res) const
{
    if (res.targetType != common::LockableResource::TargetType::Entity)
        throw std::runtime_error("Unknown resource type");

    if ((res.targetType == common::LockableResource::TargetType::Entity &&
         (lock.resource == getResourceName(res) ||
          lock.resource == (db::entityTypeToString(res.targetSet) + "*"))))
        return true;
    return false;
}

std::optional<std::map<common::LockableResource, common::ResourceLockType>>
ResourceLockService::getResourcesToLock(
        const std::map<common::LockableResource, common::ResourceLockType>& resources,
        QList<QPair<std::optional<ResourceLock>, std::optional<ResourceLock>>>& changedLocks,
        const QDateTime& now,
        std::function<bool(ResourceLock lock)> isLockOurs)
{
    QList<ResourceLock*> locksToRenew;
    std::map<common::LockableResource, common::ResourceLockType> resourcesToLock;

    for (const auto& [res, lockType] : resources) {
        bool hasLock = false;

        for (auto& [adminId, lockList] : locksByAdmins_) {
            for (int i = 0; i < lockList.length(); i++) {
                if (checkIfLockIsValid(lockList.at(i), res)) {
                    if (isLockOurs(lockList.at(i))) {
                        // if lock is ours and of the same type, just renew it
                        if (lockType == lockList.at(i).type) {
                            locksToRenew.append(&(lockList[i]));
                            hasLock = true;
                            continue;
                        }
                    } else {
                        // if lock is expired, remove it
                        if (lockList.at(i).timeout < now) {
                            changedLocks.append({lockList.at(i), std::nullopt});
                            lockList.removeAt(i);
                            i--;
                            continue;
                        }

                        // lock is compatible
                        if (compatible(lockList.at(i).type, lockType))
                            continue;

                        // incompatible lock found, locking failed
                        return std::nullopt;
                    }
                }
            }
        }

        // if does not have lock yet, prepare for creation
        if (!hasLock)
            resourcesToLock[res] = resources.at(res);
    }

    // renew if locking didn't failed
    for (auto lock : locksToRenew) {
        lock->timeout = now.addSecs(SecondsToLive);
    }

    return resourcesToLock;
}

void ResourceLockService::printLocks(const common::CallerContext& context, AsyncTaskPtr task) {
    std::lock_guard<std::recursive_mutex> guard(lockMutex_);

    auto admin = getAdmynByUsername(context.username);
    if (admin == nullptr)
        throw std::invalid_argument("Administrator does not exist.");

    qDebug() << "[LOCKS] Resource locks of " << context.username << " - " << context.token;

    for (auto lock : locksByAdmins_[admin->getId()]) {
        if (lock.adminToken != context.token)
            continue;
        qDebug() << "[LOCKS]" << lock.resource << "valid until:" << lock.timeout;
    }
}

std::shared_ptr<db::Administrator> ResourceLockService::getAdmynByUsername(
    const QString& username) const
{
    auto admins = entityService_->getAll<db::Administrator>();
    auto it = std::find_if(admins.begin(), admins.end(),
                           [username](std::shared_ptr<db::Administrator> admin) {
                               return admin->getUsername() == username;
                           });

    return it != admins.end() ? *it : nullptr;
}
