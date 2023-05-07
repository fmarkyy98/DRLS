#include "MainWindow.h"

#include <QApplication>

#include "common/src/service/EntityService.h"

#include "persistence/Administrator.h"
#include "persistence/Fruit.h"
#include "persistence/User.h"

int main(int argc, char** argv) {
    common::EntityService::getInstance()->create<db::Administrator>()
        ->setUsername("admin")
        ->setFullName("Administrator");

    auto aple       = common::EntityService::getInstance()->create<db::Fruit>()
                          ->setName("Alma");
    auto pear       = common::EntityService::getInstance()->create<db::Fruit>()
                          ->setName("Körte");
    auto strawberry = common::EntityService::getInstance()->create<db::Fruit>()
                          ->setName("Eper");
    auto pineaple   = common::EntityService::getInstance()->create<db::Fruit>()
                          ->setName("Ananász");

    auto winnie = common::EntityService::getInstance()->create<db::User>()
                      ->setFirstName("Mici")
                      ->setLastName("Mackó");
    auto piglet = common::EntityService::getInstance()->create<db::User>()
                      ->setFirstName("Félős")
                      ->setLastName("Malacka");
    auto eeyore = common::EntityService::getInstance()->create<db::User>()
                      ->setFirstName("Szomorú")
                      ->setLastName("Füles");
    auto roo    = common::EntityService::getInstance()->create<db::User>()
                      ->setFirstName("Zsebi")
                      ->setLastName("Baba");

    winnie->addFruit(aple)
          ->addFruit(pear)
          ->addFruit(strawberry);

    piglet->addFruit(pineaple)
          ->addFruit(strawberry);

    roo->addFruit(aple)
       ->addFruit(strawberry)
       ->addFruit(pineaple);

    QApplication a(argc, argv);
    view::MainWindow w;
    w.show();
    return a.exec();
}
