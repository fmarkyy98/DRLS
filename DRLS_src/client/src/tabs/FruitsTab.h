#pragma once

#include <QWidget>
#include <QListWidgetItem>

#include "base/MasterDetailWidhetBase.h"

#include "common/src/service/EntityService.h"
#include "common/src/service/interface/IResourceLockService.h"

#include "persistence/Fruit.h"

namespace view {

namespace Ui { class FruitsTab; }

struct FruitItem : public QListWidgetItem {
    FruitItem(std::shared_ptr<db::Fruit> fruit,
             const QString &text,
             QListWidget *listview = nullptr,
             int type = Type)
        : QListWidgetItem(text, listview, type)
        , fruit(fruit)
    {}

    std::shared_ptr<db::Fruit> fruit;
};

class FruitsTab
    : public QWidget
    , public common::TaskManager<common::CancellableOnly>
{
    Q_OBJECT

public:
    explicit FruitsTab(std::shared_ptr<common::AsyncTaskService> asyncTaskService,
                       std::shared_ptr<common::EntityService> entityService,
                       std::shared_ptr<common::IResourceLockService> resourceLockService,
                       QWidget* parent = nullptr);
    ~FruitsTab();

private:
    void initMassEditConnections();
    void initEditorComponentsConnections();
    void initConnections();

    void populateList();

    void refreshFields(std::shared_ptr<db::Fruit> selectedFruit);
    void persistFields(std::shared_ptr<db::Fruit> selectedFruit);

    std::shared_ptr<db::Fruit> getSelectedFruit() const;

    void setEditMode(EditMode editMode);

    void refreshDisplayName();

private:
    Ui::FruitsTab *ui;

    std::shared_ptr<common::EntityService> entityService_;
    std::shared_ptr<common::IResourceLockService> resourceLockService_;

    EditMode editMode_ = EditMode::NoEdit;

private:
    static constexpr const char token[] = "FruitsTabContextToken";
    static constexpr const char adminUserName[] = "admin";
    static const common::CallerContext context;
};

} // namespace view
