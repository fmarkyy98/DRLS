#pragma once

#include "persistence/Entity.h"

namespace common {
    class EntityService;
}  // namespace common

namespace db {
class User;

class Fruit : public Entity, std::enable_shared_from_this<Fruit> {
    friend class common::EntityService;

private:
    Fruit(int id);

public:
    ~Fruit() = default;

    DELETE_COPY_MOVE_SEMANTICS(Fruit)

    EntityType getType() const override;

    QString getName() const;

    std::shared_ptr<Fruit> setName(const QString& name);

    // Relations
    const QList<std::shared_ptr<User> > getUsers() const;

    std::shared_ptr<Fruit> clearUsers();
    std::shared_ptr<Fruit> addUser(std::shared_ptr<db::User> userToAdd);
    std::shared_ptr<Fruit> removeUser(std::shared_ptr<db::User> userToRemove);

private:
    QString name_;

private:
    static constexpr EntityType entityType_ = EntityType::Fruit;
};

}  // namespace db
