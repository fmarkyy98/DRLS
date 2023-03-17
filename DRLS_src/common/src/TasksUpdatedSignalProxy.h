#pragma once

#include <QObject>

namespace common {

class TasksUpdatedSignalProxy : public QObject {
    Q_OBJECT

public:
    TasksUpdatedSignalProxy();

signals:
    void tasksUpdated();
};

}  // namespace common
