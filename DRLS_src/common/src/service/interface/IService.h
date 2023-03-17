#pragma once

#include <QObject>
#include <memory>
#include <typeindex>

namespace common {
namespace details {

class IServiceMeta : public QObject {
    Q_OBJECT

signals:
};

}  // namespace details

class IService {
public:
    using Meta = details::IServiceMeta;

    virtual ~IService() = default;

    Meta* meta() { return meta_.get(); }
    virtual std::type_index getServiceType() { return typeid(void); }

protected:
    std::unique_ptr<Meta> meta_;
};

template<typename ServiceT, typename MetaT>
class ITypedService : public IService {
public:
    using Meta = MetaT;

    virtual MetaT* meta() { return static_cast<MetaT*>(meta_.get()); }

    ITypedService() { meta_ = std::make_unique<MetaT>(); }

    std::type_index getServiceType() override { return typeid(ServiceT); }
};

}  // namespace common
