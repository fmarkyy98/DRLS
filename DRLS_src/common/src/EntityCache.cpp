#include "EntityCache.h"

using namespace common;

std::shared_ptr<EntityCache> EntityCache::instance_;

std::shared_ptr<EntityCache> EntityCache::getInstance() {
    if (instance_ == nullptr)
        instance_ = std::shared_ptr<EntityCache>(new EntityCache());

    return instance_;
}
