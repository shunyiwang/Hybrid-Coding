#ifndef ENCODE_H
#define ENCODE_H
#include <QMainWindow>
#include<vector>
#include<iostream>
using namespace std;
class Encode{
    public:
        int paramN,paramM;//原始块和编码块个数
        QString readFilePath;//加密文件的路径
        QString encodedBlockPath;//保存编码块和映射文件的路径
        QString originalBlockPath;//保存编码块和映射文件的路径
        QString blockPrefix;//编码块保存的前缀
        QString FileMD5;//文件的MD5
        int mode;//选择编码的模式
        vector<int> d;
        //生成文件的MD5
        void setFileMD5();

        //分割函数,将内容分割成多个小块
        void split();

        //度分布函数，根据度分布获取编码块的度
        void degree();

        //编码函数
        void encode();
};

#endif // ENCODE_H

