#include "MainWindow.h"
#include "./ui_MainWindow.h"

#include "MasterDetailWidget.h"
#include "details/UserDetailsWidget.h"
#include "details/FruitDetailsWidget.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ui->usersTab->layout()->addWidget(new MasterDetailWidget("Users", new UserDetailsWidget()));
    ui->fruitsTab->layout()->addWidget(new MasterDetailWidget("Fruits", new FruitDetailsWidget()));
}

MainWindow::~MainWindow()
{
    delete ui;
}

