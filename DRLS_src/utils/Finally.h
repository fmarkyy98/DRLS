#pragma once

#include <utility>

namespace util {
template <class Func_T>
class finally
{
public:
    finally(Func_T func) :func_(func) {}
    finally(const finally&) = delete;
    finally(finally&& other) : func_(std::move(other.func_)) {
        other.func_ = []{};
    }

    ~finally() { func_(); }

private:
    Func_T func_;
};
}
