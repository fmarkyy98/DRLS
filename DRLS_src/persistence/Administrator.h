#pragma once

#include "persistence/Entity.h"

namespace common {
    class EntityService;
}  // namespace common

namespace db {

class Administrator
    : public Entity
    , public std::enable_shared_from_this<Administrator>
{
    friend class common::EntityService;

private:
    Administrator(int id);

public:
    ~Administrator() = default;

    DELETE_COPY_MOVE_SEMANTICS(Administrator)

    EntityType getType() const override;

    QString getUsername() const;
    QString getFullName() const;

    std::shared_ptr<Administrator> setUsername(const QString& username);
    std::shared_ptr<Administrator> setFullName(const QString& fullName);

    void remove() override;

private:
    QString username_;
    QString fullName_;

private:
    static constexpr EntityType entityType_ = EntityType::Administrator;
};

}  // namespace db
