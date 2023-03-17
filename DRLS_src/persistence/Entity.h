#pragma once

#include <memory>

#include "EntityType.h"

#include "common/src/EntityCache.h"

#define DELETE_COPY_MOVE_SEMANTICS(ENTITY_TYPE)      \
ENTITY_TYPE(const ENTITY_TYPE&) = delete;            \
ENTITY_TYPE(ENTITY_TYPE&&) = delete;                 \
                                                     \
ENTITY_TYPE& operator=(const ENTITY_TYPE&) = delete; \
ENTITY_TYPE& operator=(ENTITY_TYPE&&) = delete;

#define DEFINE_getType()             \
EntityType Entity::getType() const { \
    return entityType_;              \
}

namespace db {

class Entity {
protected:
    Entity(int id);

public:
    virtual ~Entity() = default;

    DELETE_COPY_MOVE_SEMANTICS(Entity)

    virtual EntityType getType() const;

    int getId() const;

protected:
    std::shared_ptr<common::EntityCache> entityCache_;

    int id_;

private:
    static constexpr EntityType entityType_ = EntityType::Entity;
};

}  // namespace db