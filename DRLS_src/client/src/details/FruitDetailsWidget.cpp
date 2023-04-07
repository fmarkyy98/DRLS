#include "FruitDetailsWidget.h"
#include "ui_FruitDetailsWidget.h"

FruitDetailsWidget::FruitDetailsWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::FruitDetailsWidget)
{
    ui->setupUi(this);
}

FruitDetailsWidget::~FruitDetailsWidget()
{
    delete ui;
}
