#pragma once

#include <QWidget>
#include <QListWidgetItem>

#include "common/src/service/EntityService.h"

#include "persistence/User.h"

namespace view {

namespace Ui { class UsersTab; }

class UserItem : public QListWidgetItem {
public:
    UserItem(std::shared_ptr<db::User> user,
             const QString &text,
             QListWidget *listview = nullptr,
             int type = Type)
        : QListWidgetItem(text, listview, type)
        , user_(user)
    {}

private:
    std::shared_ptr<db::User> user_;
};

class UsersTab : public QWidget
{
    Q_OBJECT

public:
    explicit UsersTab(std::shared_ptr<common::EntityService> entityService,
                      QWidget* parent = nullptr);
    ~UsersTab();

private:
    void initConnections();

private:
    Ui::UsersTab *ui;

    std::shared_ptr<common::EntityService> entityService_;
};

} // namespace view
