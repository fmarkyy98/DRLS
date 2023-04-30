#pragma once

#include <QObject>
#include <QWidget>

namespace view {

enum class EditMode {
    NoEdit,
    SingleEdit,
    MassEdit
};

template<typename Ui>
requires true
class MasterDetailWidhetBase : public QWidget {
    Q_OBJECT
public:
    MasterDetailWidhetBase(QWidget* parent = nullptr)
        : QWidget(parent)
    {}
};

}// namespace view
