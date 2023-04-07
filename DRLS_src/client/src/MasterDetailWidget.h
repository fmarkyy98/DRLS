#ifndef MASTERDETAILWIDGET_H
#define MASTERDETAILWIDGET_H

#include <QWidget>

namespace Ui {
class MasterDetailWidget;
}

class MasterDetailWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MasterDetailWidget(const QString& listLabelText,
                                QWidget* detailsWidget,
                                QWidget* parent = nullptr);
    ~MasterDetailWidget();

private:
    Ui::MasterDetailWidget *ui;

    QWidget* detailsWidget_;
};

#endif // MASTERDETAILWIDGET_H
