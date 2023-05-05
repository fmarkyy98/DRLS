#include "MainWindow.h"
#include "ui_MainWindow.h"

#include "tabs/FruitsTab.h"
#include "tabs/UsersTab.h"
#include "dialogs/FruitUserRelationsDialog.h"

#include "common/src/service/ResourceLockService.h"

using namespace view;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ui->usersTab->layout()->addWidget(new UsersTab(common::EntityService::getInstance(),
                                                   common::ResourceLockService::getInstance()));
    ui->fruitsTab->layout()->addWidget(new FruitsTab(common::EntityService::getInstance(),
                                                     common::ResourceLockService::getInstance()));

    connect(ui->fruitUserAction, &QAction::triggered, this, [] {
        FruitUserRelationsDialog dialog(common::EntityService::getInstance(),
                                        common::ResourceLockService::getInstance());
        dialog.setModal(true);
        dialog.exec();
    });
}

MainWindow::~MainWindow()
{
    delete ui;
}

