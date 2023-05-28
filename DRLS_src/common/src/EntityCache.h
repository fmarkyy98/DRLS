#pragma once

#include <type_traits>
#include <memory>
#include <unordered_set>
#include <set>
#include <ranges>

#include <QList>

#include "persistence/Fruit.h"
#include "persistence/User.h"

namespace db {
    class Entity;

    class Administrator;
    class Fruit;
    class User;
}  // namespace db

namespace common {

class EntityCache {
public:
    static std::shared_ptr<EntityCache> getInstance();

private:
    EntityCache() = default;

public:
    template<typename Entity_T>
    requires std::is_base_of_v<db::Entity, Entity_T>
    const QList<std::shared_ptr<Entity_T>> getCached() {
        return getListOfType<Entity_T>();
    }

    template<typename Entity_T, typename Related_T>
    requires std::is_base_of_v<db::Entity, Entity_T> &&
             std::is_base_of_v<db::Entity, Related_T>
    const QList<std::shared_ptr<Related_T>> getRelatedEntitiesOf(std::shared_ptr<const Entity_T> entity) {
        std::set<int> relatedEntityIdSet;
        if constexpr (std::is_same_v<Entity_T, db::Fruit> &&
                      std::is_same_v<Related_T, db::User>)
        {
            for (auto relation : fruitUserRelations_ | std::views::filter([entity](std::pair<int, int> relation) {
                                     return entity->getId() == relation.first;
                                 }))
            {
                relatedEntityIdSet.insert(relation.second);
            }
        }
        else if constexpr (std::is_same_v<Entity_T, db::User> &&
                           std::is_same_v<Related_T, db::Fruit>)
        {
            for (const auto& relation : fruitUserRelations_
                                        | std::views::filter([entity](std::pair<int, int> relation) {
                                            return entity->getId() == relation.second;
                                        }))
            {
                relatedEntityIdSet.insert(relation.first);
            }
        }

        QList<std::shared_ptr<Related_T>> relatedEntities;
        for (auto related : getListOfType<Related_T>()
                            | std::views::filter([relatedEntityIdSet,
                                                  &relatedEntities](std::shared_ptr<Related_T> related) {
                                return std::find_if(relatedEntityIdSet.begin(), relatedEntityIdSet.end(),
                                                    [related](int relatedId) {
                                                        return related->getId() == relatedId;
                                                    }) != relatedEntityIdSet.end();
                            }))
        {
            relatedEntities.append(related);
        }

        return relatedEntities;
    }

    template<typename Entity_T>
    requires std::is_base_of_v<db::Entity, Entity_T>
    std::shared_ptr<Entity_T> cache(Entity_T* entityRawPtr) {
        auto entity = std::shared_ptr<Entity_T>(entityRawPtr);
        getListOfType<Entity_T>().append(entity);
        return entity;
    }

    template<typename Entity_T>
    requires std::is_base_of_v<db::Entity, Entity_T>
    void remove(std::shared_ptr<Entity_T> entity) {
        getListOfType<Entity_T>().removeOne(entity);
        if constexpr (std::is_same_v<Entity_T, db::Fruit>) {
            std::erase_if(fruitUserRelations_, [entity](std::pair<int, int> pair) {
                return pair.first == entity->getId();
            });
        } else if constexpr (std::is_same_v<Entity_T, db::User>) {
            std::erase_if(fruitUserRelations_, [entity](std::pair<int, int> pair) {
                return pair.second == entity->getId();
            });
        }
    }

    template<typename Entity_T, typename Related_T>
    requires std::is_base_of_v<db::Entity, Entity_T> &&
             std::is_base_of_v<db::Entity, Related_T>
    void link(std::shared_ptr<Entity_T> a, std::shared_ptr<Related_T> b) {
        if constexpr (std::is_same_v<Entity_T, db::Fruit> &&
                      std::is_same_v<Related_T, db::User>)
        {
            fruitUserRelations_.insert(std::pair{a->getId(), b->getId()});
        }
        else if constexpr (std::is_same_v<Entity_T, db::User> &&
                           std::is_same_v<Related_T, db::Fruit>)
        {
            fruitUserRelations_.insert({b->getId(), a->getId()});
        }
    }

    template<typename Entity_T, typename Related_T>
    requires std::is_base_of_v<db::Entity, Entity_T> &&
             std::is_base_of_v<db::Entity, Related_T>
    void unlink(std::shared_ptr<Entity_T> a, std::shared_ptr<Related_T> b) {
        if constexpr (std::is_same_v<Entity_T, db::Fruit> &&
                      std::is_same_v<Related_T, db::User>)
        {
            fruitUserRelations_.erase(std::pair{a->getId(), b->getId()});
        }
        else if constexpr (std::is_same_v<Entity_T, db::User> &&
                           std::is_same_v<Related_T, db::Fruit>)
        {
            fruitUserRelations_.erase({b->getId(), a->getId()});
        }
    }

    template<typename Entity_T, typename Related_T>
    requires std::is_base_of_v<db::Entity, Entity_T> &&
             std::is_base_of_v<db::Entity, Related_T>
    void clearLinksOf(std::shared_ptr<Entity_T> entity) {
        if constexpr (std::is_same_v<Entity_T, db::Fruit> &&
                      std::is_same_v<Related_T, db::User>)
        {
            for (auto it = fruitUserRelations_.begin(); it != fruitUserRelations_.end();) {
                if (entity->getId() == it->first) {
                    it = fruitUserRelations_.erase(it);
                    continue;
                }

                ++it;
            }
        }
        if constexpr (std::is_same_v<Entity_T, db::User> &&
                      std::is_same_v<Related_T, db::Fruit>)
        {
            for (auto it = fruitUserRelations_.begin(); it != fruitUserRelations_.end();) {
                if (entity->getId() == it->second) {
                    it = fruitUserRelations_.erase(it);
                    continue;
                }

                ++it;
            }
        }
    }

private:
    template<typename Entity_T>
    requires std::is_base_of_v<db::Entity, Entity_T>
        QList<std::shared_ptr<Entity_T>>& getListOfType() {
        if constexpr (std::is_same_v<Entity_T, db::Administrator>)
            return admins_;
        if constexpr (std::is_same_v<Entity_T, db::Fruit>)
            return fruits_;
        if constexpr (std::is_same_v<Entity_T, db::User>)
            return users_;

        std::invalid_argument("Invalid template argument");
    }

private:
    static std::shared_ptr<EntityCache> instance_;

private:
    QList<std::shared_ptr<db::Administrator>> admins_;
    QList<std::shared_ptr<db::Fruit>> fruits_;
    QList<std::shared_ptr<db::User>> users_;

    std::set<std::pair<int, int>> fruitUserRelations_;
};

} // namespace common
