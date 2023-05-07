#include "FruitUserRelationsDialog.h"
#include "ui_FruitUserRelationsDialog.h"

#include <QMessageBox>

#include "client/src/tabs/FruitsTab.h"
#include "client/src/tabs/UsersTab.h"

#include "common/src/service/AsyncTaskService.h"

using namespace view;

const common::CallerContext FruitUserRelationsDialog::context = common::CallerContext(token, adminUserName);

FruitUserRelationsDialog::FruitUserRelationsDialog(
            std::shared_ptr<common::AsyncTaskService> asyncTaskService,
            std::shared_ptr<common::EntityService> entityService,
            std::shared_ptr<common::IResourceLockService> resourceLockService,
            QWidget *parent)
    : QDialog(parent)
    , TaskManager<common::CancellableOnly>(asyncTaskService)
    , ui(new Ui::FruitUserRelationsDialog)
    , entityService_(entityService)
    , resourceLockService_(resourceLockService)
{
    ui->setupUi(this);

    lockResources();

    initConnections();

    populateLists();

    onSelectionChanged();
}

FruitUserRelationsDialog::~FruitUserRelationsDialog()
{
    releaseResources();
    delete ui;
}

void FruitUserRelationsDialog::initConnections() {
    connect(ui->fruitList->selectionModel(),
            &QItemSelectionModel::selectionChanged,
            this,
            &FruitUserRelationsDialog::onSelectionChanged);

    connect(ui->userList->selectionModel(),
            &QItemSelectionModel::selectionChanged,
            this,
            &FruitUserRelationsDialog::onSelectionChanged);

    connect(ui->linkButton, &QPushButton::clicked, this, [this]{
        auto fruit = getSelectedFruit();
        auto user = getSelectedUser();
        if (user == nullptr || fruit == nullptr)
            return;

        if (user->getFruits().contains(fruit))
            user->removeFruit(fruit);
         else
            user->addFruit(fruit);

        onSelectionChanged();
    });
}

void FruitUserRelationsDialog::lockResources() {
    resourceLockService_
        ->acquireLocks({{common::LockableResource(db::EntityType::Fruit),
                         common::ResourceLockType::Write},
                        {common::LockableResource(db::EntityType::User),
                         common::ResourceLockType::Write}},
                       context)
        ->onResultAvailable([this](bool result) {
            if (result)
                return;

            QMessageBox::warning(this,
                                 "Unlockable resources",
                                 "Failes to lock resources.\n"
                                 "Dialog will close automatically");
            close();
        })
        ->run<common::ManagedTaskBehaviour::CancelOnExit>(this);
}

void FruitUserRelationsDialog::releaseResources() {
    resourceLockService_
        ->releaseLocks({{common::LockableResource(db::EntityType::Fruit),
                         common::ResourceLockType::Write},
                        {common::LockableResource(db::EntityType::User),
                         common::ResourceLockType::Write}},
                       context)
        ->runUnmanaged();
}

void FruitUserRelationsDialog::populateLists() {
    ui->fruitList->clear();
    for (auto fruit : entityService_->getAll<db::Fruit>()) {
        auto item = new FruitItem(fruit, fruit->getName(), ui->fruitList);
        ui->fruitList->addItem(item);
    }

    ui->userList->clear();
    for (auto user : entityService_->getAll<db::User>()) {
        auto item = new UserItem(user, user->getFullName(), ui->userList);
        ui->userList->addItem(item);
    }
}

std::shared_ptr<db::Fruit> FruitUserRelationsDialog::getSelectedFruit() const {
    auto selectedItems = ui->fruitList->selectedItems();
    if (selectedItems.count() != 1)
        return nullptr;

    return static_cast<FruitItem*>(selectedItems.first())->fruit;
}

std::shared_ptr<db::User> FruitUserRelationsDialog::getSelectedUser() const {
    auto selectedItems = ui->userList->selectedItems();
    if (selectedItems.count() != 1)
        return nullptr;

    return static_cast<UserItem*>(selectedItems.first())->user;
}

void FruitUserRelationsDialog::onSelectionChanged() {
    auto fruit = getSelectedFruit();
    auto user = getSelectedUser();

    if (user == nullptr || fruit == nullptr) {
        ui->linkLabel->setText("Invalid Selectiobn");

        ui->linkButton->setEnabled(false);
        ui->linkButton->setText("Invalid Selectiobn");

        return;
    }

    ui->linkButton->setEnabled(true);

    if (user->getFruits().contains(fruit)) {
        ui->linkLabel->setText(fruit->getName() + "----" + user->getFullName());
        ui->linkButton->setText("Unlink");
    } else {
        ui->linkLabel->setText(fruit->getName() + "-   -" + user->getFullName());
        ui->linkButton->setText("Link");
    }
}
