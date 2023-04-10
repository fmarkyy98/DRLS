#include "FruitsTab.h"
#include "ui_FruitsTab.h"

using namespace view;

FruitsTab::FruitsTab(std::shared_ptr<common::EntityService> entityService,
                                       QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::FruitsTab)
    , entityService_(common::EntityService::getInstance())
{
    ui->setupUi(this);

    initConnections();
}

FruitsTab::~FruitsTab()
{
    delete ui;
}

void FruitsTab::initConnections() {
    connect(ui->addButton, &QPushButton::clicked, this, [this] {
        auto entity = entityService_->create<db::Fruit>();

        ui->listWidget->addItem(new FruitItem(entity, "New ", ui->listWidget));
        ui->listWidget->selectedItems(); // TODO
    });
}
