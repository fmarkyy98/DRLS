#ifndef FRUITDETAILSWIDGET_H
#define FRUITDETAILSWIDGET_H

#include <QWidget>

namespace Ui {
class FruitDetailsWidget;
}

class FruitDetailsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FruitDetailsWidget(QWidget *parent = nullptr);
    ~FruitDetailsWidget();

private:
    Ui::FruitDetailsWidget *ui;
};

#endif // FRUITDETAILSWIDGET_H
