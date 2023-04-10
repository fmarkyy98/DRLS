#include "EntityService.h"
#include "common/src/EntityCache.h"

using namespace common;

std::shared_ptr<EntityService> EntityService::instance_;

EntityService::EntityService(std::shared_ptr<common::EntityCache> entityCache)
    : entityCache_(entityCache)
{}

std::shared_ptr<EntityService> EntityService::getInstance() {
    if (instance_ == nullptr)
        instance_ = std::shared_ptr<EntityService>(new EntityService(common::EntityCache::getInstance()));

    return instance_;
}
