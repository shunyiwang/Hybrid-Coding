#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void on_SelectFileButton_clicked();

    void on_SelectPathButton_clicked();

    void on_Mode1radioButton_clicked();

    void on_Mode2radioButton_clicked();

    void on_RunButton_clicked();

    void on_ConfirmButton_clicked();

    void on_SelectFileButton__clicked();

private:
    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
