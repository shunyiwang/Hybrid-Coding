#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "encode.h"
#include <qfile.h>
#include <qfiledialog.h>
MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    delete ui;
}

Encode encode;
void MainWindow::on_SelectFileButton_clicked()//选择需要加密的文件
{
    QString openPath=QFileDialog::getOpenFileName(this,"打开需要加密的文件","../");
    if(openPath.isEmpty()==false){
        QFile file(openPath);
        bool isOK=file.open(QIODevice::ReadOnly);
        if(isOK==true){
            file.readAll();   //读所有的值，返回一个字节数组
            ui->FilePathText->setText(openPath);
        }
    }
    encode.readFilePath=openPath;
    encode.setFileMD5();
    ui->MD5Text->setText(encode.FileMD5);
}

void MainWindow::on_SelectPathButton_clicked()//选择保存文件夹的路径
{
    QString savePath=QFileDialog::getExistingDirectory(this,"选择编码块文件保存路径","../");
    if(savePath.isEmpty()==false){
        ui->SavePathText->setText(savePath);
    }


    encode.encodedBlockPath=savePath+"/encoded/";
    encode.originalBlockPath=savePath+"/original/";
}

//选择模式1,采用均匀分布
void MainWindow::on_Mode1radioButton_clicked()
{
    if(ui->Mode1radioButton->isChecked()){
       // ui->MD5Text->setText("1");//测试
        encode.mode=1;
    }
}

//选择模式2,采用鲁棒分布
void MainWindow::on_Mode2radioButton_clicked()
{
    if(ui->Mode2radioButton->isChecked()){
       // ui->MD5Text->setText("2");//测试
        encode.mode=2;
    }
}


//获取参数n以及m
//调用编码函数，根据输入参数生成编码块
void MainWindow::on_RunButton_clicked()
{
    bool ok;
    encode.paramN=(ui->ParmNText->toPlainText()).toInt(&ok,10);;//获取原始块个数
    encode.paramM=(ui->ParmMText->toPlainText()).toInt(&ok,10);;//获取编码块个数
    encode.split();
    encode.degree();
    encode.encode();

}
