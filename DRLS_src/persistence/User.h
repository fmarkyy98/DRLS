#pragma once

#include <optional>

#include "persistence/Entity.h"

namespace common {
    class EntityService;
}  // namespace common

namespace db {
class Fruit;

class User
    : public Entity
    , public std::enable_shared_from_this<User>
{
    friend class common::EntityService;

private:
    User(int id);

public:
    ~User() = default;

    DELETE_COPY_MOVE_SEMANTICS(User)

    EntityType getType() const override;

    std::optional<QString> getNamePrefix() const;
    QString getFirstName() const;
    std::optional<QString> getMidleName() const;
    QString getLastName() const;

    // non trivial getters
    QString getFullName() const;

    std::shared_ptr<User> setNamePrefix(const std::optional<QString> &namePrefix);
    std::shared_ptr<User> setFirstName(const QString& firstName);
    std::shared_ptr<User> setMidleName(const std::optional<QString> &midleName);
    std::shared_ptr<User> setLastName(const QString& lastName);

    void remove() override;

    // relations
    const QList<std::shared_ptr<Fruit>> getFruits() const;

    std::shared_ptr<User> clearFruits();
    std::shared_ptr<User> addFruit(std::shared_ptr<db::User> fruitToAdd);
    std::shared_ptr<User> removeFruit(std::shared_ptr<db::User> fruitToRemove);

private:
    std::optional<QString> namePrefix_;
    QString firstName_;
    std::optional<QString> midleName_;
    QString lastName_;

private:
    static constexpr EntityType entityType_ = EntityType::User;
};

}  // namespace db
