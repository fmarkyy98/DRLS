#include "Entity.h"

#include "common/src/EntityCache.h"

using namespace db;

Entity::Entity(int id)
    : entityCache_(common::EntityCache::getInstance())
    , id_(id)
{}

DEFINE_getType(Entity)

int Entity::getId() const {
    return id_;
}
