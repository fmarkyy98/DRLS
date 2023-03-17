#include "common/src/Delegate.h"

using namespace common;

QUuid DelegateConnection::getToken() const {
    return token;
}

bool DelegateConnection::operator==(const DelegateConnection& other) const {
    return this->token == other.token;
}

bool DelegateConnection::operator!=(const DelegateConnection& other) const {
    return this->token != other.token;
}

bool DelegateConnection::operator<(const DelegateConnection& other) const {
    return this->token < other.token;
}

bool DelegateConnection::operator>(const DelegateConnection& other) const {
    return this->token > other.token;
}
