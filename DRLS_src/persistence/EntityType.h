#pragma once

#include <QString>

namespace db {

enum class EntityType { Entity, Administrator, Fruit, User };


constexpr EntityType stringToEntityType(const QString& str);
constexpr QString entityTypeToString(const EntityType& e);

} // namespace db
