#include "UsersTab.h"
#include "ui_UsersTab.h"

using namespace view;

UsersTab::UsersTab(std::shared_ptr<common::EntityService> entityService,
                                       QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::UsersTab)
    , entityService_(entityService)
{
    ui->setupUi(this);

    initConnections();
}

UsersTab::~UsersTab()
{
    delete ui;
}

void UsersTab::initConnections() {
    connect(ui->addButton, &QPushButton::clicked, this, [this] {
        auto newUser = entityService_->create<db::User>();

        ui->listWidget->addItem(new UserItem(newUser, "New ", ui->listWidget));
        ui->listWidget->selectedItems(); // TODO
    });
}
