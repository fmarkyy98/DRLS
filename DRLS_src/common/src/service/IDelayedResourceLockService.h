#pragma once

#include <QMap>
#include <common/service/ServiceIncludes.h>
#include <common/utils/LockableResource.h>

namespace common {

class IDelayedResourceLockService {
public:
  virtual void addAsyncLock(CallerContext context,
                            QMap<LockableResource, ResourceLockType> resources,
                            AsyncTaskPtr task, int timeoutMs,
                            AsyncTaskPtr timeoutTask = nullptr) = 0;
  virtual void addAsyncSystemLock(
      QString tag, QMap<LockableResource, ResourceLockType> resources,
      AsyncTaskPtr task, int timeoutMs, AsyncTaskPtr timeoutTask = nullptr) = 0;
};
} // namespace common
