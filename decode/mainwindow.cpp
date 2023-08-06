#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <qfile.h>
#include <qfiledialog.h>
#include<decode.h>
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

Decode decode;
//选择需要解码的编码块文件夹
void MainWindow::on_SelectFileButton_clicked()
{
    QString filePath=QFileDialog::getExistingDirectory(this,"选择编码块文件路径","../");
    if(filePath.isEmpty()==false){
        ui->FilePathText->setText(filePath);
        decode.encodeFilePath=filePath+"/";
    }
}

//选择需要解码信息所在的路径
void MainWindow::on_SelectKeyButton_clicked()
{
    QString keyPath=QFileDialog::getOpenFileName(this,"打开需要加密的文件","../");
    if(keyPath.isEmpty()==false){
      ui->KeyPathText->setText(keyPath);
      decode.keyInfoPath=keyPath;
    }
}

//选择需要保存内容的路径
void MainWindow::on_SelectSaveButton_clicked()
{
    QString savePath=QFileDialog::getExistingDirectory(this,"打开需要保存的文件夹","../");
    if(savePath.isEmpty()==false){
      ui->SavePathText->setText(savePath);
      decode.saveFilePath=savePath+"/";
    }
}

//确定内容的名字
void MainWindow::on_ConfirmButton_clicked()
{
    QString name=ui->SaveNameText->text();
    if(name.isEmpty()==false){
        decode.saveFileName=name;
    }
}

//执行解码程序
void MainWindow::on_RunButton_clicked()
{
    decode.decode();
    decode.genHashRoot();
    decode.genFileMd5();
    decode.genMerkleRoot();
    ui->HashRootShow->setText(decode.hashRoot);
    ui->MD5Show->setText(decode.md5);
    ui->MerkleRootShow->setText(decode.merkleRoot);
    decode.md5s.clear();
}




