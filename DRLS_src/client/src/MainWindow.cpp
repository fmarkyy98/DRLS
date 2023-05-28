#include "MainWindow.h"
#include "ui_MainWindow.h"

#include "tabs/FruitsTab.h"
#include "tabs/UsersTab.h"
#include "dialogs/FruitUserRelationsDialog.h"

#include "common/src/service/ResourceLockService.h"
#include "common/src/service/DelayedResourceLockService.h"

using namespace view;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ui->usersTab->layout()->addWidget(new UsersTab(common::AsyncTaskService::getInstance(),
                                                   common::EntityService::getInstance(),
                                                   common::ResourceLockService::getInstance()));
    ui->fruitsTab->layout()->addWidget(new FruitsTab(common::AsyncTaskService::getInstance(),
                                                     common::EntityService::getInstance(),
                                                     common::ResourceLockService::getInstance()));

    connect(ui->fruitUserAction, &QAction::triggered, this, [] {
        FruitUserRelationsDialog dialog(common::AsyncTaskService::getInstance(),
                                        common::EntityService::getInstance(),
                                        common::ResourceLockService::getInstance());
        dialog.setModal(true);
        dialog.exec();
    });
    auto resourceLockService = common::ResourceLockService::getInstance();

    connect(resourceLockService->meta(),
            &common::IResourceLockService::Meta::locksChanged,
            this,
            [this] {
                if (logOnCooldown_)
                    return;

                QTimer::singleShot(1000, this, [this] { logOnCooldown_ = false; });
                logOnCooldown_ = true;
                logUsersWithFruitPreferences();
            });
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::logUsersWithFruitPreferences() {
    auto asyncTaskService = common::AsyncTaskService::getInstance();
    auto entityService = common::EntityService::getInstance();
    auto drls = common::DelayedResourceLockService::getInstance();

    auto task = asyncTaskService->createTask([this, entityService](...) {
        std::unique_lock logLock{logMutex};
        qDebug() << "[LIKE]" << "----------------------------------------------------";
        auto users = entityService->getAll<db::User>();
        if (users.isEmpty())
            qDebug() << "[LIKE]" << "no User found";

        for (auto user : users) {
            auto fruits = user->getFruits();
            if (fruits.isEmpty()) {
                qDebug() << "[LIKE]" << user->getFullName() << "does not like any fruit";
                continue;
            }

            qDebug() << "[LIKE]" << user->getFullName() << "like:";
            for (auto fruit : fruits)
                qDebug() << "       -" << fruit->getName();
        }
        qDebug() << "[LIKE]" << "----------------------------------------------------";
    });

    drls->addAsyncSystemLock("MainWindow::logUsersWithFruitPreferences" + QTime::currentTime().toString(),
                             {{common::LockableResource(db::EntityType::Fruit),
                               common::ResourceLockType::Read},
                              {common::LockableResource(db::EntityType::User),
                               common::ResourceLockType::Read}},
                             task,
                             3600'000);
}
