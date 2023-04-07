#include "UserDetailsWidget.h"
#include "ui_UserDetailsWidget.h"

UserDetailsWidget::UserDetailsWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::UserDetailsWidget)
{
    ui->setupUi(this);
}

UserDetailsWidget::~UserDetailsWidget()
{
    delete ui;
}
