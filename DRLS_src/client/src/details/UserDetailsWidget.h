#ifndef USERDETAILSWIDGET_H
#define USERDETAILSWIDGET_H

#include <QWidget>

namespace Ui {
class UserDetailsWidget;
}

class UserDetailsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit UserDetailsWidget(QWidget *parent = nullptr);
    ~UserDetailsWidget();

private:
    Ui::UserDetailsWidget *ui;
};

#endif // USERDETAILSWIDGET_H
