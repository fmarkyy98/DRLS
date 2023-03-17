#include "common/src/service/EntityService.h"
#include "common/src/EntityCache.h"

#include "persistence/Administrator.h"

template<>
std::shared_ptr<db::Administrator> common::EntityService::getById(int id) {
    auto admins = entityCache_->getCached<db::Administrator>();
    auto it = std::find_if(admins.begin(), admins.end(),
                           [id](std::shared_ptr<db::Administrator> admin) {
        return admin->id_ == id;
    });
    return it != admins.end() ? *it : nullptr;
}

template<>
QList<std::shared_ptr<db::Administrator>> common::EntityService::getAll() {
    return entityCache_->getCached<db::Administrator>();
}

template<>
int common::EntityService::getCount<db::Administrator>() {
    return entityCache_->getCached<db::Administrator>().count();
}


template<>
QList<int> common::EntityService::getAllIds<db::Administrator>() {
    QList<int> result;
    auto admins = entityCache_->getCached<db::Administrator>();
    std::transform(admins.begin(), admins.end(),
                   std::back_inserter(result),
                   [](std::shared_ptr<db::Administrator> admin) {
        return admin->id_;
    });
    return result;
}


template<>
std::shared_ptr<db::Administrator> common::EntityService::create() {
    QList<int> ids = getAllIds<db::Administrator>();
    auto maxId = std::reduce(ids.begin(), ids.end(), 1, std::greater<int>());

    return entityCache_->cache(new db::Administrator(maxId + 1));
}
