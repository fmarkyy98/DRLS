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

    common::EntityService::getInstance()->create<db::Fruit>()
        ->setName("Alma");
    common::EntityService::getInstance()->create<db::Fruit>()
        ->setName("Körte");
    common::EntityService::getInstance()->create<db::Fruit>()
        ->setName("Eper");
    common::EntityService::getInstance()->create<db::Fruit>()
        ->setName("Ananász");

    common::EntityService::getInstance()->create<db::User>()
        ->setFirstName("Mici")
        ->setLastName("Mackó");
    common::EntityService::getInstance()->create<db::User>()
        ->setLastName("Malacka");
    common::EntityService::getInstance()->create<db::User>()
        ->setLastName("Füles");
    common::EntityService::getInstance()->create<db::User>()
        ->setFirstName("Zsebi")
        ->setLastName("Baba");

    QApplication a(argc, argv);
    view::MainWindow w;
    w.show();
    return a.exec();
}
