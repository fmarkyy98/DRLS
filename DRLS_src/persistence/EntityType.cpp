#include "EntityType.h"

using namespace db;

constexpr EntityType stringToEntityType(const QString& str) {
    if (str == "Entity")
        return EntityType::Entity;
    if (str == "Administrator")
        return EntityType::Administrator;
    if (str == "Fruit")
        return EntityType::Fruit;
    if (str == "User")
        return EntityType::User;

    std::runtime_error("Unknown EntityType");
}

QString entityTypeToString(const EntityType& e) {
    switch (e) {
    case EntityType::Entity: {
        return "Entity";
    }
    case EntityType::Administrator: {
        return "Administrator";
    }
    case EntityType::Fruit: {
        return "Fruit";
    }
    case EntityType::User: {
        return "User";
    }
    }

    std::runtime_error("Unknown EntityType");
}
