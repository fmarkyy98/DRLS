#include <QList>

#include "Fruit.h"

#include "common/src/EntityCache.h"

using namespace db;

int Fruit::nextId_ = 1;

Fruit::Fruit()
    : Entity(nextId_++)
{}

DEFINE_getType(Fruit)

QString Fruit::getName() const {
    return name_;
}

std::shared_ptr<db::Fruit> Fruit::setName(const QString& name) {
    name_ = name;

    return shared_from_this();
}

DEFINE_remove(Fruit)

// relations
const QList<std::shared_ptr<db::User>> Fruit::getUsers() const {
    return entityCache_->getRelatedEntitiesOf<db::Fruit, db::User>(shared_from_this());
}

std::shared_ptr<Fruit> Fruit::clearUsers() {
    entityCache_->clearLinksOf<db::Fruit, db::User>(shared_from_this());

    return shared_from_this();
}

std::shared_ptr<Fruit> Fruit::addUser(std::shared_ptr<db::User> userToAdd) {
    entityCache_->link(shared_from_this(), userToAdd);

    return shared_from_this();
}

std::shared_ptr<Fruit> Fruit::removeUser(std::shared_ptr<db::User> userToRemove) {
    entityCache_->unlink(shared_from_this(), userToRemove);

    return shared_from_this();
}
