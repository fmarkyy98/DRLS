#pragma once

#include <QMainWindow>

namespace view {

namespace Ui { class MainWindow; }

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    void logUsersWithFruitPreferences();

private:
    Ui::MainWindow *ui;

    std::mutex logMutex;
};

} // namespace view
