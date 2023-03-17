#include "Entity.h"

using namespace db;

Entity::Entity(int id)
    : entityCache_(common::EntityCache::getInstance())
    , id_(id)
{}

DEFINE_getType()

int Entity::getId() const {
    return id_;
}
