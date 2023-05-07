#include "UsersTab.h"
#include "ui_UsersTab.h"

using namespace view;

const common::CallerContext UsersTab::context = common::CallerContext(token, adminUserName);

UsersTab::UsersTab(std::shared_ptr<common::AsyncTaskService> asyncTaskService,
                   std::shared_ptr<common::EntityService> entityService,
                   std::shared_ptr<common::IResourceLockService> resourceLockService,
                   QWidget* parent)
    : QWidget(parent)
    , TaskManager<common::CancellableOnly>(asyncTaskService)
    , ui(new Ui::UsersTab)
    , entityService_(entityService)
    , resourceLockService_(resourceLockService)
{
    ui->setupUi(this);

    initConnections();
    populateList();
    setEditMode(EditMode::NoEdit);
}

UsersTab::~UsersTab()
{
    delete ui;
}

void UsersTab::initMassEditConnections() {
    connect(ui->namePrefixComboBox, &QComboBox::currentTextChanged, this, [this](const QString& text) {
        auto selectedUser = getSelectedUser();
        if (selectedUser == nullptr || editMode_ != EditMode::MassEdit)
            return;

        selectedUser->setNamePrefix(text != "(none)" ? std::optional(text) : std::nullopt);
        refreshDisplayName();
    });

    connect(ui->firstNameLineEdit, &QLineEdit::editingFinished, this, [this] {
        auto selectedUser = getSelectedUser();
        if (selectedUser == nullptr || editMode_ != EditMode::MassEdit)
            return;

        selectedUser->setFirstName(ui->firstNameLineEdit->text());
        refreshDisplayName();
    });

    connect(ui->midleNameLineEdit, &QLineEdit::editingFinished, this, [this] {
        auto selectedUser = getSelectedUser();
        if (selectedUser == nullptr || editMode_ != EditMode::MassEdit)
            return;

        auto text = ui->midleNameLineEdit->text();
        selectedUser->setMidleName(!text.isEmpty() ? std::optional(text) : std::nullopt);
        refreshDisplayName();
    });

    connect(ui->lastNameLineEdit, &QLineEdit::editingFinished, this, [this] {
        auto selectedUser = getSelectedUser();
        if (selectedUser == nullptr || editMode_ != EditMode::MassEdit)
            return;

        selectedUser->setLastName(ui->lastNameLineEdit->text());
        refreshDisplayName();
    });
}

void UsersTab::initEditorComponentsConnections() {
    connect(ui->massEditButton, &QPushButton::clicked, this, [this] {
        resourceLockService_
            ->acquireLocks({{common::LockableResource(db::EntityType::User),
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
            ->releaseLocks({{common::LockableResource(db::EntityType::User),
                             common::ResourceLockType::Write}},
                           context)
            ->onFinished([this](...){
                setEditMode(EditMode::NoEdit);
            })
            ->run<common::ManagedTaskBehaviour::CancelOnExit>(this);
    });

    connect(ui->editButton, &QPushButton::clicked, this, [this] {
        auto user = getSelectedUser();
        if (user == nullptr)
            return;

        resourceLockService_
            ->acquireLocks({{common::LockableResource(db::EntityType::User),
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
        auto user = getSelectedUser();
        if (user == nullptr)
            return;

        persistFields(user);
        refreshDisplayName();

        resourceLockService_
            ->releaseLocks({{common::LockableResource(db::EntityType::User),
                             common::ResourceLockType::Write}},
                           context)
            ->onFinished([this](...){
                setEditMode(EditMode::NoEdit);
            })
            ->run<common::ManagedTaskBehaviour::CancelOnExit>(this);
    });

    connect(ui->revertButton, &QPushButton::clicked, this, [this] {
        auto user = getSelectedUser();
        if (user == nullptr)
            return;

        refreshFields(user);

        resourceLockService_
            ->releaseLocks({{common::LockableResource(db::EntityType::User),
                             common::ResourceLockType::Write}},
                           context)
            ->onFinished([this](...){
                setEditMode(EditMode::NoEdit);
            })
            ->run<common::ManagedTaskBehaviour::CancelOnExit>(this);
    });
}

void UsersTab::initConnections() {
    initMassEditConnections();

    initEditorComponentsConnections();

    connect(ui->listWidget->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this] {
        auto selectedUser = getSelectedUser();
        refreshFields(selectedUser);
    });

    connect(ui->addButton, &QPushButton::clicked, this, [this] {
        auto newUser =
            entityService_->create<db::User>()
                ->setFirstName("New")
                ->setLastName("User");


        auto item = new UserItem(newUser, newUser->getFullName(), ui->listWidget);
        ui->listWidget->addItem(item);
        ui->listWidget->clearSelection();
        ui->listWidget->selectionModel()->select(ui->listWidget->indexFromItem(item),
                                                 QItemSelectionModel::Select);
    });

    connect(ui->removeButton, &QPushButton::clicked, this, [this] {
        auto user = getSelectedUser();
        if (user == nullptr)
            return;

        auto selectedItems = ui->listWidget->selectedItems();
        if (selectedItems.count() != 1)
            return;

        delete selectedItems.first();

        user->remove();
    });
}

void UsersTab::populateList() {
    ui->listWidget->clear();
    for (auto user : entityService_->getAll<db::User>()) {
        auto item = new UserItem(user, user->getFullName(), ui->listWidget);
        ui->listWidget->addItem(item);
    }
}

void UsersTab::refreshFields(std::shared_ptr<db::User> selectedUser) {
    if (selectedUser == nullptr)
        return;

    ui->namePrefixComboBox->setCurrentIndex(
        ui->namePrefixComboBox->findText(
            selectedUser->getNamePrefix().value_or("(none)")));
    ui->firstNameLineEdit->setText(selectedUser->getFirstName());
    ui->midleNameLineEdit->setText(selectedUser->getMidleName().value_or(""));
    ui->lastNameLineEdit->setText(selectedUser->getLastName());
}

void UsersTab::persistFields(std::shared_ptr<db::User> selectedUser) {
    if (selectedUser == nullptr)
        return;

    auto prefix = ui->namePrefixComboBox->currentText();
    auto firstName = ui->firstNameLineEdit->text();
    auto midleName = ui->midleNameLineEdit->text();
    auto lastName = ui->lastNameLineEdit->text();

    selectedUser
        ->setNamePrefix(prefix != "(none)" ? std::optional(prefix) : std::nullopt)
        ->setFirstName(firstName)
        ->setMidleName(!midleName.isEmpty() ? std::optional(midleName) : std::nullopt)
        ->setLastName(lastName);
}

std::shared_ptr<db::User> UsersTab::getSelectedUser() const {
    auto selectedItems = ui->listWidget->selectedItems();
    if (selectedItems.count() != 1)
        return nullptr;

    return static_cast<UserItem*>(selectedItems.first())->user;
}

void UsersTab::setEditMode(EditMode editMode) {
    editMode_ = editMode;
    switch (editMode_) {
    case EditMode::NoEdit: {
        ui->usersFrame->setEnabled(true);
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
        ui->usersFrame->setEnabled(false);
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
        ui->usersFrame->setEnabled(true);
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

void UsersTab::refreshDisplayName() {
    auto selectedItems = ui->listWidget->selectedItems();
    if (selectedItems.count() != 1)
        return;

    auto selectedItem = selectedItems.first();
    auto user = static_cast<UserItem*>(selectedItems.first())->user;

    selectedItem->setText(user->getFullName());
}
