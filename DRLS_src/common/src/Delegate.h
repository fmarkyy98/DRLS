#pragma once

#include <QtCore>
#include <functional>
#include <memory>

namespace common {

class DelegateConnection {
    QUuid token;

public:
    DelegateConnection() { token = QUuid::createUuid(); }

    QUuid getToken() const;

    bool operator==(const DelegateConnection& other) const;
    bool operator!=(const DelegateConnection& other) const;
    bool operator<(const DelegateConnection& other) const;
    bool operator>(const DelegateConnection& other) const;
};

}  // namespace common
