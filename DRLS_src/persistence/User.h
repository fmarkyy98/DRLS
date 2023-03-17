#pragma once

#include "persistence/Entity.h"

namespace common {
    class EntityService;
}  // namespace common

namespace db {

class User : public Entity {
    friend class common::EntityService;

public:
    User(int id);
};

}  // namespace db
