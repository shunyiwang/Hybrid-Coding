#ifndef DECODE_H
#define DECODE_H
#include <QMainWindow>
#include <vector>
#include <iostream>
#include<string>
using namespace std;
class Decode{
public:
    QString encodeFilePath;//编码块所在的文件夹
    QString keyInfoPath;//解码信息的路径
    QString saveFilePath;//内容保存的路径
    QString saveFileName;//内容保存的名字

    QString merkleRoot;
    QString hashRoot;
    QString md5;
    vector<string> ids;

    vector<string> md5s;//编码块的MD5集合
    //生成编码块的merkleRoot
    void genMerkleRoot();
    //生成编码块的hashRoot
    void genHashRoot();
    //生成内容的MD5
    void genFileMd5();
    //解码编码块
    void decode();
private:
    vector<string> resultVec;
    void split(string str,string pattern);//切分每行key
    void split(char *src,string pattern);//切分每行key

};

#endif // DECODE_H

