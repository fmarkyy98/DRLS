#include "MainWindow.h"

#include <QApplication>

#include "common/src/service/EntityService.h"

#include "persistence/Administrator.h"
#include "persistence/Fruit.h"
#include "persistence/User.h"

int main(int argc, char** argv) {
    auto entityService = common::EntityService::getInstance();

    entityService->create<db::Administrator>()
        ->setUsername("admin")
        ->setFullName("Administrator");

    auto aple       = entityService->create<db::Fruit>()
                          ->setName("Alma");
    auto pear       = entityService->create<db::Fruit>()
                          ->setName("Körte");
    auto strawberry = entityService->create<db::Fruit>()
                          ->setName("Eper");
    auto pineaple   = entityService->create<db::Fruit>()
                          ->setName("Ananász");

    auto winnie = entityService->create<db::User>()
                      ->setFirstName("Mici")
                      ->setLastName("Mackó");
    auto piglet = entityService->create<db::User>()
                      ->setFirstName("Félős")
                      ->setLastName("Malacka");
    auto eeyore = entityService->create<db::User>()
                      ->setFirstName("Szomorú")
                      ->setLastName("Füles");
    auto roo    = entityService->create<db::User>()
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
