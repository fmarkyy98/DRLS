#include "FruitsTab.h"
#include "ui_FruitsTab.h"

using namespace view;

const common::CallerContext FruitsTab::context = common::CallerContext(token, adminUserName);

FruitsTab::FruitsTab(std::shared_ptr<common::AsyncTaskService> asyncTaskService,
                     std::shared_ptr<common::EntityService> entityService,
                     std::shared_ptr<common::IResourceLockService> resourceLockService,
                     QWidget* parent)
    : QWidget(parent)
    , TaskManager<common::CancellableOnly>(asyncTaskService)
    , ui(new Ui::FruitsTab)
    , entityService_(entityService)
    , resourceLockService_(resourceLockService)
{
    ui->setupUi(this);

    initConnections();
    populateList();
    setEditMode(EditMode::NoEdit);
}

FruitsTab::~FruitsTab()
{
    delete ui;
}

void FruitsTab::initMassEditConnections() {
    connect(ui->nameLineEdit, &QLineEdit::editingFinished, this, [this] {
        auto selectedFruit = getSelectedFruit();
        if (selectedFruit == nullptr || editMode_ != EditMode::MassEdit)
            return;

        selectedFruit->setName(ui->nameLineEdit->text());
        refreshDisplayName();
    });
}

void FruitsTab::initEditorComponentsConnections() {
    connect(ui->massEditButton, &QPushButton::clicked, this, [this] {
        resourceLockService_
            ->acquireLocks({{common::LockableResource(db::EntityType::Fruit ),
                             common::ResourceLockType::Write}},
                           context)
            ->onResultAvailable([this](bool result) {
                if (!result)
                    return;

                setEditMode(EditMode::MassEdit);
            })
            ->run<common::ManagedTaskBehaviour::CancelOnExit>(this);
    });

    connect(ui->finishMassEditButton, &QPushButton::clicked, this, [this] {
        resourceLockService_
            ->releaseLocks({{common::LockableResource(db::EntityType::Fruit),
                             common::ResourceLockType::Write}},
                           context)
            ->onFinished([this](...){
                setEditMode(EditMode::NoEdit);
            })
            ->run<common::ManagedTaskBehaviour::CancelOnExit>(this);
    });

    connect(ui->editButton, &QPushButton::clicked, this, [this] {
        auto fruit = getSelectedFruit();
        if (fruit == nullptr)
            return;

        resourceLockService_
            ->acquireLocks({{common::LockableResource(db::EntityType::Fruit),
                             common::ResourceLockType::Write}},
                           context)
            ->onResultAvailable([this](bool result) {
                if (!result)
                    return;

                setEditMode(EditMode::SingleEdit);
            })
            ->run<common::ManagedTaskBehaviour::CancelOnExit>(this);
    });

    connect(ui->saveButton, &QPushButton::clicked, this, [this] {
        auto fruit = getSelectedFruit();
        if (fruit == nullptr)
            return;

        persistFields(fruit);
        refreshDisplayName();

        resourceLockService_
            ->releaseLocks({{common::LockableResource(db::EntityType::Fruit),
                             common::ResourceLockType::Write}},
                           context)
            ->onFinished([this](...){
                setEditMode(EditMode::NoEdit);
            })
            ->run<common::ManagedTaskBehaviour::CancelOnExit>(this);
    });

    connect(ui->revertButton, &QPushButton::clicked, this, [this] {
        auto fruit = getSelectedFruit();
        if (fruit == nullptr)
            return;

        refreshFields(fruit);

        resourceLockService_
            ->releaseLocks({{common::LockableResource(db::EntityType::Fruit),
                             common::ResourceLockType::Write}},
                           context)
            ->onFinished([this](...){
                setEditMode(EditMode::NoEdit);
            })
            ->run<common::ManagedTaskBehaviour::CancelOnExit>(this);
    });
}

void FruitsTab::initConnections() {
    initMassEditConnections();

    initEditorComponentsConnections();

    connect(ui->listWidget->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this] {
        auto selectedFruit = getSelectedFruit();
        refreshFields(selectedFruit);
    });

    connect(ui->addButton, &QPushButton::clicked, this, [this] {
        auto newFruit =
            entityService_->create<db::Fruit>()
                ->setName("New Fruit");

        auto item = new FruitItem(newFruit, newFruit->getName(), ui->listWidget);
        ui->listWidget->addItem(item);
        ui->listWidget->clearSelection();
        ui->listWidget->selectionModel()->select(ui->listWidget->indexFromItem(item), QItemSelectionModel::Select);
    });

    connect(ui->removeButton, &QPushButton::clicked, this, [this] {
        auto fruit = getSelectedFruit();
        if (fruit == nullptr)
            return;

        auto selectedItems = ui->listWidget->selectedItems();
        if (selectedItems.count() != 1)
            return;

        delete selectedItems.first();

        fruit->remove();
    });
}

void FruitsTab::populateList() {
    ui->listWidget->clear();
    for (auto user : entityService_->getAll<db::Fruit>()) {
        auto item = new FruitItem(user, user->getName(), ui->listWidget);
        ui->listWidget->addItem(item);
    }
}

void FruitsTab::refreshFields(std::shared_ptr<db::Fruit> selectedFruit) {
    if (selectedFruit == nullptr)
        return;

    ui->nameLineEdit->setText(selectedFruit->getName());
}

void FruitsTab::persistFields(std::shared_ptr<db::Fruit> selectedFruit) {
    if (selectedFruit == nullptr)
        return;

    auto name = ui->nameLineEdit->text();

    selectedFruit->setName(name);
}

std::shared_ptr<db::Fruit> FruitsTab::getSelectedFruit() const {
    auto selectedItems = ui->listWidget->selectedItems();
    if (selectedItems.count() != 1)
        return nullptr;

    return static_cast<FruitItem*>(selectedItems.first())->fruit;
}

void FruitsTab::setEditMode(EditMode editMode) {
    editMode_ = editMode;
    switch (editMode_) {
    case EditMode::NoEdit: {
        ui->fruitsFrame->setEnabled(true);
        ui->detailsFrame->setEnabled(false);

        ui->massEditButton->setVisible(true);
        ui->massEditButton->setEnabled(true);

        ui->finishMassEditButton->setVisible(false);

        ui->editButton->setVisible(true);
        ui->editButton->setEnabled(true);

        ui->saveButton->setVisible(false);

        ui->revertButton->setVisible(false);

        ui->massEditLabel->setVisible(false);
    } break;
    case EditMode::SingleEdit: {
        ui->fruitsFrame->setEnabled(false);
        ui->detailsFrame->setEnabled(true);

        ui->massEditButton->setVisible(true);
        ui->massEditButton->setEnabled(false);

        ui->finishMassEditButton->setVisible(false);

        ui->editButton->setVisible(false);
        ui->editButton->setEnabled(true);

        ui->saveButton->setVisible(true);

        ui->revertButton->setVisible(true);

        ui->massEditLabel->setVisible(false);
    } break;
    case EditMode::MassEdit: {
        ui->fruitsFrame->setEnabled(true);
        ui->detailsFrame->setEnabled(true);

        ui->massEditButton->setVisible(false);
        ui->massEditButton->setEnabled(true);

        ui->finishMassEditButton->setVisible(true);

        ui->editButton->setVisible(false);
        ui->editButton->setEnabled(true);

        ui->saveButton->setVisible(false);

        ui->revertButton->setVisible(false);

        ui->massEditLabel->setVisible(true);
    } break;
    }
}

void FruitsTab::refreshDisplayName() {
    auto selectedItems = ui->listWidget->selectedItems();
    if (selectedItems.count() != 1)
        return;

    auto selectedItem = selectedItems.first();
    auto fruit = static_cast<FruitItem*>(selectedItems.first())->fruit;

    QString name = fruit->getName();

    selectedItem->setText(name);
}
