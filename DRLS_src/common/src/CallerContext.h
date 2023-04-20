#pragma once

#include <QtCore>

namespace common {

struct CallerContext
{
    QString token;
    QString username;

    CallerContext(const QString& token = "", const QString& username = "")
        : token(token)
        , username(username)
    {}
};

}// namespace common
