#include "EntityCache.h"

using namespace common;

std::shared_ptr<EntityCache> EntityCache::getInstance() {
    if (instance_ == nullptr)
        instance_ = std::make_shared<EntityCache>();

    return instance_;
}
