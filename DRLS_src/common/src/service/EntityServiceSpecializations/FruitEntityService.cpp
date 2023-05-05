#include "common/src/service/EntityService.h"
#include "common/src/EntityCache.h"

#include "persistence/Fruit.h"

template<>
std::shared_ptr<db::Fruit> common::EntityService::getById(int id) {
    auto fruits = entityCache_->getCached<db::Fruit>();
    auto it = std::find_if(fruits.begin(), fruits.end(), [id](std::shared_ptr<db::Fruit> fruit) {
        return fruit->id_ == id;
    });
    return it != fruits.end() ? *it : nullptr;
}

template<>
QList<std::shared_ptr<db::Fruit>> common::EntityService::getAll() {
    return entityCache_->getCached<db::Fruit>();
}

template<>
int common::EntityService::getCount<db::Fruit>() {
    return entityCache_->getCached<db::Fruit>().count();
}


template<>
QList<int> common::EntityService::getAllIds<db::Fruit>() {
    QList<int> result;
    auto fruits = entityCache_->getCached<db::Fruit>();
    std::transform(fruits.begin(), fruits.end(),
                   std::back_inserter(result),
                   [](std::shared_ptr<db::Fruit> fruit) {
        return fruit->id_;
    });
    return result;
}


template<>
std::shared_ptr<db::Fruit> common::EntityService::create() {
    return entityCache_->cache(new db::Fruit());
}
