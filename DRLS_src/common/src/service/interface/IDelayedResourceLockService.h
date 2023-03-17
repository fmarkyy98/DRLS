#pragma once

#include <QMap>

#include "common/src/CallerContext.h"
#include "common/src/AsyncTask.h"
#include "common/src/LockableResource.h"


namespace common {

class IDelayedResourceLockService {
public:
  virtual void addAsyncLock(CallerContext context,
                            QMap<common::LockableResource, common::ResourceLockType> resources,
                            AsyncTaskPtr task,
                            int timeoutMs,
                            AsyncTaskPtr timeoutTask = nullptr) = 0;
  virtual void addAsyncSystemLock(
            QString tag,
            QMap<LockableResource, ResourceLockType> resources,
            AsyncTaskPtr task,            int timeoutMs,
            AsyncTaskPtr timeoutTask = nullptr) = 0;
};
} // namespace common
