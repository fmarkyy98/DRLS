#include <QList>

#include "User.h"

#include "common/src/EntityCache.h"

using namespace db;

User::User(int id)
    : Entity(id)
    , std::enable_shared_from_this<User>()
{}

DEFINE_getType(User)

std::optional<QString> User::getNamePrefix() const {
    return namePrefix_;
}

QString User::getFirstName() const {
    return firstName_;
}

std::optional<QString> User::getMidleName() const {
    return midleName_;
}

QString User::getLastName() const {
    return lastName_;
}

// non trivial getters
QString User::getFullName() const {
    return namePrefix_.value_or("") + firstName_ + midleName_.value_or("") + lastName_;
}

std::shared_ptr<User> User::setNamePrefix(const std::optional<QString>& namePrefix) {
    namePrefix_ = namePrefix;

    return shared_from_this();
}

std::shared_ptr<User> User::setFirstName(const QString& firstName) {
    firstName_ = firstName;

    return shared_from_this();
}

std::shared_ptr<User> User::setMidleName(const std::optional<QString>& midleName) {
    midleName_ = midleName;

    return shared_from_this();
}

std::shared_ptr<User> User::setLastName(const QString& lastName) {
    lastName_ = lastName;

    return shared_from_this();
}

// relations
const QList<std::shared_ptr<Fruit>> User::getFruits() const {
    return entityCache_->getRelatedEntitiesOf<db::User, db::Fruit>(shared_from_this());
}

std::shared_ptr<User> User::clearFruits() {
    entityCache_->clearLinksOf<db::User, db::Fruit>(shared_from_this());

    return shared_from_this();
}

std::shared_ptr<User> User::addFruit(std::shared_ptr<db::User> fruitToAdd) {
    entityCache_->link(shared_from_this(), fruitToAdd);

    return shared_from_this();
}

std::shared_ptr<User> User::removeFruit(std::shared_ptr<db::User> fruitToRemove) {
    entityCache_->unlink(shared_from_this(), fruitToRemove);

    return shared_from_this();
}
