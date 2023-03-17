#pragma once

#include <QtCore>

namespace common {

struct CallerContext
{
    QString token;
    QString username;

    CallerContext() = default;
    CallerContext(const QString& token) : token(token) {}
};

}// namespace common
