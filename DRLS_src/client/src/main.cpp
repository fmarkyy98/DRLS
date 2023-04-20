#include "MainWindow.h"

#include <QApplication>

#include "common/src/service/EntityService.h"

#include "persistence/Administrator.h"

int main(int argc, char** argv) {
    auto entityService = common::EntityService::getInstance();

    auto admin = common::EntityService::getInstance()
                    ->create<db::Administrator>()
                    ->setUsername("admin")
                    ->setFullName("Administrator");

    QApplication a(argc, argv);
    view::MainWindow w;
    w.show();
    return a.exec();
}
