#include "Administrator.h"

using namespace db;

Administrator::Administrator(int id)
    : Entity(id)
    , std::enable_shared_from_this<Administrator>()
{}

DEFINE_getType(Administrator)

QString Administrator::getUsername() const {
    return username_;
}

QString Administrator::getFullName() const {
    return fullName_;
}

std::shared_ptr<Administrator> Administrator::setUsername(const QString& username) {
    username_ = username;

    return shared_from_this();
}

std::shared_ptr<Administrator> Administrator::setFullName(const QString& fullName) {
    fullName_ = fullName;

    return shared_from_this();
}
