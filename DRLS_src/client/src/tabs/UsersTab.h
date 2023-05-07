#pragma once

#include <QWidget>
#include <QListWidgetItem>

#include "base/MasterDetailWidhetBase.h"

#include "common/src/service/EntityService.h"
#include "common/src/service/interface/IResourceLockService.h"

#include "persistence/User.h"

namespace view {

namespace Ui { class UsersTab; }

struct UserItem : public QListWidgetItem {
    UserItem(std::shared_ptr<db::User> user,
             const QString &text,
             QListWidget *listview = nullptr,
             int type = Type)
        : QListWidgetItem(text, listview, type)
        , user(user)
    {}

    std::shared_ptr<db::User> user;
};

class UsersTab
    : public QWidget
    , public common::TaskManager<common::CancellableOnly>
{
    Q_OBJECT

public:
    explicit UsersTab(std::shared_ptr<common::AsyncTaskService> asyncTaskService,
                      std::shared_ptr<common::EntityService> entityService,
                      std::shared_ptr<common::IResourceLockService> resourceLockService,
                      QWidget* parent = nullptr);
    ~UsersTab();

private:
    void initMassEditConnections();
    void initEditorComponentsConnections();
    void initConnections();

    void populateList();

    void refreshFields(std::shared_ptr<db::User> selectedUser);
    void persistFields(std::shared_ptr<db::User> selectedUser);

    std::shared_ptr<db::User> getSelectedUser() const;

    void setEditMode(EditMode editMode);

    void refreshDisplayName();

private:
    Ui::UsersTab *ui;

    std::shared_ptr<common::EntityService> entityService_;
    std::shared_ptr<common::IResourceLockService> resourceLockService_;

    EditMode editMode_ = EditMode::NoEdit;

private:
    static constexpr const char token[] = "UsersTabContextToken";
    static constexpr const char adminUserName[] = "admin";
    static const common::CallerContext context;
};

} // namespace view
