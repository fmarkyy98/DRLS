#pragma once

#include <memory>

#include <QList>

#include "persistence/Entity.h"

namespace common {
class EntityCache;

class EntityService {
public:
    static std::shared_ptr<EntityService> getInstance();

private:
    EntityService(std::shared_ptr<common::EntityCache> entityCache);

public:
    template<typename Entity_T>
    std::shared_ptr<Entity_T> getById(int id);

    template<typename Entity_T>
    QList<std::shared_ptr<Entity_T>> getAll();

    template<typename Entity_T>
    int getCount();

    template<typename Entity_T>
    QList<int> getAllIds();

    template<typename Entity_T>
    std::shared_ptr<Entity_T> create();

private:
    template<typename Entity_T>
    requires std::is_base_of_v<Entity_T, db::Entity>
    static QList<std::shared_ptr<db::Entity>> downcastEntityList(
            const QList<std::shared_ptr<Entity_T>>& list)
    {
        QList<std::shared_ptr<db::Entity>> result;
        std::transform(list.begin(), list.end(),
                       std::back_inserter(result),
                       [](std::shared_ptr<Entity_T> entity) {
            return std::static_pointer_cast<db::Entity>(entity);
        });
        return result;
    }

    template<typename Entity_T>
    requires std::is_base_of_v<Entity_T, db::Entity>
    static QList<std::shared_ptr<Entity_T>> upcastEntityList(
        const QList<std::shared_ptr<db::Entity>>& list)
    {
        QList<std::shared_ptr<Entity_T>> result;
        std::transform(list.begin(), list.end(),
                       std::back_inserter(result),
                       [](std::shared_ptr<db::Entity> entity) {
            return std::static_pointer_cast<Entity_T>(entity);
        });
        return result;
    }

private:
    static std::shared_ptr<EntityService> instance_;

private:
    std::shared_ptr<common::EntityCache> entityCache_;
};

}  // namespace common
