#include "common/src/service/EntityService.h"
#include "common/src/EntityCache.h"

#include "persistence/User.h"

template<>
std::shared_ptr<db::User> common::EntityService::getById(int id) {
    auto users = entityCache_->getCached<db::User>();
    auto it = std::find_if(users.begin(), users.end(), [id](std::shared_ptr<db::User> user) {
        return user->id_ == id;
    });
    return it != users.end() ? *it : nullptr;
}

template<>
QList<std::shared_ptr<db::User>> common::EntityService::getAll() {
    return entityCache_->getCached<db::User>();
}

template<>
int common::EntityService::getCount<db::User>() {
    return entityCache_->getCached<db::User>().count();
}


template<>
QList<int> common::EntityService::getAllIds<db::User>() {
    QList<int> result;
    auto users = entityCache_->getCached<db::User>();
    std::transform(users.begin(), users.end(),
                   std::back_inserter(result),
                   [](std::shared_ptr<db::User> user) {
        return user->id_;
    });
    return result;
}


template<>
std::shared_ptr<db::User> common::EntityService::create() {
    QList<int> ids = getAllIds<db::User>();
    auto maxId = std::reduce(ids.begin(), ids.end(), 1, std::greater<int>());

    return entityCache_->cache(new db::User(maxId + 1));
}
