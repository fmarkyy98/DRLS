#pragma once

#include <functional>

#include <QString>

namespace util {

template<typename Signature_T>
class Callback : public std::function<Signature_T> {
public:
    Callback(QString token, std::function<Signature_T> func)
        : std::function<Signature_T>(func)
        , token_(token)
    {}

    Callback(const Callback&) = default;
    Callback(Callback&&) = default;

    Callback& operator=(const Callback&) = default;
    Callback& operator=(Callback&&) = default;

    friend bool operator==(const Callback& a, const Callback& b);

private:
    QString token_;
};

template<typename Signature_T>
bool operator==(const Callback<Signature_T>& a, const Callback<Signature_T>& b) {
    return a.token_ == b.token_ ;
}

}  // namespace util
