#pragma once

#include <persistence/EntityType.h>
#include <QVariant>

namespace common {

enum class ResourceLockType { Read, Write };

struct LockableResource {
    enum class TargetType { Unknown, Entity };

    TargetType targetType;
    db::EntityType targetSet;
    int targetId;

    inline LockableResource() {}

    inline LockableResource(const db::EntityType& entityType)
        : targetType(TargetType::Entity), targetSet(entityType), targetId(-1)
    {}

    inline LockableResource(const db::EntityType& entityType, int id)
        : targetType(TargetType::Entity), targetSet(entityType), targetId(id)
    {}

    db::EntityType entityType() const { return targetSet; }

    friend bool operator<(const LockableResource& a, const LockableResource& b);
};

inline bool operator<(const LockableResource& a, const LockableResource& b) {
    return static_cast<int>(a.entityType()) < static_cast<int>(b.entityType());
}

}  // namespace common
