#pragma once

#include <QDialog>

#include "common/src/service/EntityService.h"
#include "common/src/service/interface/IResourceLockService.h"

#include "persistence/Fruit.h"
#include "persistence/User.h"

namespace view {

namespace Ui {
class FruitUserRelationsDialog;
}

class FruitUserRelationsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FruitUserRelationsDialog(std::shared_ptr<common::EntityService> entityService,
                                      std::shared_ptr<common::IResourceLockService> resourceLockService,
                                      QWidget *parent = nullptr);
    ~FruitUserRelationsDialog();

private:
    void initConnections();

    void lockResources();
    void releaseResources();

    void populateLists();

    std::shared_ptr<db::Fruit> getSelectedFruit() const;
    std::shared_ptr<db::User> getSelectedUser() const;

private slots:
    void onSelectionChanged();

private:
    Ui::FruitUserRelationsDialog *ui;

    std::shared_ptr<common::EntityService> entityService_;
    std::shared_ptr<common::IResourceLockService> resourceLockService_;

private:
    static constexpr const char token[] = "FruitUserRelationsDialogContextToken";
    static constexpr const char adminUserName[] = "admin";
    static const common::CallerContext context;
};

} // namespace view
