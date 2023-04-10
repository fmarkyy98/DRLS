#pragma once

#include <QWidget>
#include <QListWidgetItem>

#include "common/src/service/EntityService.h"

#include "persistence/Fruit.h"

namespace view {

namespace Ui { class FruitsTab; }

class FruitItem : public QListWidgetItem {
public:
    FruitItem(std::shared_ptr<db::Fruit> fruit,
             const QString &text,
             QListWidget *listview = nullptr,
             int type = Type)
        : QListWidgetItem(text, listview, type)
        , fruit_(fruit)
    {}

private:
    std::shared_ptr<db::Fruit> fruit_;
};

class FruitsTab : public QWidget
{
    Q_OBJECT

public:
    explicit FruitsTab(std::shared_ptr<common::EntityService> entityService,
                                QWidget* parent = nullptr);
    ~FruitsTab();

private:
    void initConnections();

private:
    Ui::FruitsTab *ui;

    std::shared_ptr<common::EntityService> entityService_;
};

} // namespace view
