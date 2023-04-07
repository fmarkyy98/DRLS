#include "MasterDetailWidget.h"
#include "ui_MasterDetailWidget.h"

MasterDetailWidget::MasterDetailWidget(const QString& listLabelText,
                                       QWidget* detailsWidget,
                                       QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::MasterDetailWidget)
    , detailsWidget_(detailsWidget)
{
    ui->setupUi(this);
    ui->listLabel->setText(listLabelText);
    ui->detailsFrame->layout()->addWidget(detailsWidget_);
}

MasterDetailWidget::~MasterDetailWidget()
{
    delete ui;
}
