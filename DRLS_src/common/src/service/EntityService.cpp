#include "EntityService.h"

using namespace common;

EntityService::EntityService(std::shared_ptr<common::EntityCache> entityCache)
    : entityCache_(entityCache)
{}
