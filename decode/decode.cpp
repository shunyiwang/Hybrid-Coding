#include "decode.h"
#include "md5.h"
#include <iostream>
#include <fstream>
#include <time.h>
#include <string.h>
#include<sstream>

using namespace std;

void Decode::genFileMd5(){
    QString savePath=saveFilePath+saveFileName;
    ifstream in((const char *)savePath.toLocal8Bit(), ios::binary);   //定义文件输入流
    if (!in)
        return;
    MD5 mD5;   //定义MD5的对象
    std::streamsize length;
    char buffer[1024];   //每次读取1024字节
    while (!in.eof()) {   //循环读取文件，直至读取完
        in.read(buffer, 1024);
        length = in.gcount();
        if (length > 0)
            mD5.update(buffer, length);   //调用MD5类的更新MD5编码函数
    }
    in.close();   //关闭文件输入流
    md5=QString(QString::fromLocal8Bit(mD5.toString().c_str()));   //显示生成的MD5编码
}

void Decode::genHashRoot(){
    MD5 md5;   //定义MD5的对象
    string s;
    for(int i=0;i<md5s.size();i++){
        //cout<<"md5s:    "<<md5s[i]<<endl;
        s.append(md5s[i]);
       // cout<<"s:   "<<s<<endl;

    }
    cout<<"md5Sum"<<s<<endl;
    md5.update(s);
    hashRoot=QString(QString::fromLocal8Bit(md5.toString().c_str()));   //显示生成的MD5编码
}

void Decode::genMerkleRoot(){
    vector<string> md5tmps;
    for(int i=0;i<md5s.size();i++){
        md5tmps.push_back(md5s[i]);
    }
    MD5 md5;   //定义MD5的对象
    int end=md5tmps.size();
    vector<string> tmp;
    while(end>1){
        for(int i=0;i<end;i++){
            string s;
            if(i+1<end){
                md5.update(s.append(md5tmps[i]).append(md5tmps[i++]));
                tmp.push_back(md5.toString());
            }else{
                md5.update(s.append(md5tmps[i]));
                tmp.push_back(md5.toString());            }
        }
        end=tmp.size();
        md5tmps.clear();
        for(int i=0;i<end;i++){
            md5tmps.push_back(tmp[i]);
        }
        tmp.clear();
    }
    md5.update(md5tmps[0]);
    merkleRoot=QString(QString::fromLocal8Bit(md5.toString().c_str()));   //显示生成的MD5编码
}

/*
 * 1、通过keyInfo每行改成逆波兰表达式
 * 2、根据逆波兰表达式生成原始块
 * 此处，仅仅进行简单的异或操作
 */
void Decode::decode(){
    ifstream key((const char *)keyInfoPath.toLocal8Bit(), ios::binary);   //获取key的输入流
    if (!key)
           return;
    string s;
    bool frag=true;
    int blockID=1;
    int encodedLen;
    char buffer[2048];//设置编码块缓存

    QString savePath=saveFilePath+saveFileName;
    cout<<"savePath"<<(const char *)savePath.toLocal8Bit()<<endl;
    ofstream out((const char *)savePath.toLocal8Bit(),ios::binary);

    while(getline(key,s)){
        if(frag){//第一行数据为编码块序号,此处执行计算各个编码块的MD5
            vector<string> ids;
            string tmp;
            stringstream input(s);
            while(input>>tmp){
                ids.push_back(tmp);
            }

            /*for(int i=0;i<ids.size();i++){
                cout<<ids[i]<<endl;
            }*/
            //计算编码块的长度
            int num;
            stringstream ss;
            ss<<ids[0];
            ss>>num;
            QString pathTmp=encodeFilePath+QString::number(num, 10);//选择编码块
            ifstream intmp((const char *)pathTmp.toLocal8Bit(), ios::binary);   //定义文件输入流
            intmp.seekg(0 , ios::end);
            encodedLen = intmp.tellg();//编码块长度
            intmp.close();   //关闭文件输入流
            frag=false;

            //计算每个编码块的MD5
            for(int i=0;i<ids.size();i++){
                stringstream stmp;
                stmp<<ids[i];
                stmp>>num;
                //cout<<ids[i]<<" "<<num<<endl;
                QString pathTmp=encodeFilePath+QString::number(num, 10);//选择编码块
               // cout<<"MD5Path:"<<(const char *)pathTmp.toLocal8Bit()<<endl;
                ifstream in((const char *)pathTmp.toLocal8Bit(), ios::binary);  //定义文件输入流
                if (!in)
                    return;
                MD5 md5;   //定义MD5的对象
                std::streamsize length;
                char buffer[1024];   //每次读取1024字节
                while (!in.eof()) {   //循环读取文件，直至读取完
                    in.read(buffer, 1024);
                    length = in.gcount();
                    if (length > 0)
                        md5.update(buffer, length);   //调用MD5类的更新MD5编码函数
                }
                in.close();   //关闭文件输入流
                string md5str=md5.toString();
                md5s.push_back(md5str);
                cout<<"md5:"<<md5str<<endl;
            }
            for(int i=0;i<md5s.size();i++){
                cout<<"md5s"<<md5s[i]<<endl;
            }

        }else{//执行解码操作
            //cout<<"key:"<<s<<endl;
            //之执行异或操作，则将所以数字提取出来即可
            char *src=new char[strlen(s.c_str())+1];
            strcpy(src,s.c_str());
            for(int i=0;i<strlen(s.c_str());i++){
                if(src[i]=='!'){
                    src[i]=' ';
                }else if(src[i]=='('){
                    src[i]=' ';
                }else if(src[i]==')'){
                    src[i]=' ';
                }else if(src[i]=='^'){
                    src[i]=' ';
                }
            }
            //cout<<src<<endl;
            split(src," ");



           for(int i=0;i<resultVec.size();i++){
                //cout<<"encode block id:"<<resultVec[i]<<endl;
           }
           for(int i=0;i<resultVec.size();i++){//此处执行异或操作,
                //cout<<tmp[i];
                //执行异或运算
                int num;
                stringstream ss;
                ss<<resultVec[i];
                ss>>num;
                //cout<<num<<endl;

                QString pathTmp=encodeFilePath+QString::number(num, 10);//选择编码块
               // cout<<"encode path:"<<(const char *)pathTmp.toLocal8Bit()<<endl;
                ifstream in((const char *)pathTmp.toLocal8Bit(), ios::binary);   //定义文件输入流
                char tmpbuf[encodedLen];
                if(i==0){
                    in.read(buffer, encodedLen);
                }else{
                    in.read(tmpbuf, encodedLen);
                    for(int j=0;j<encodedLen;j++){
                        buffer[j]^=tmpbuf[j];
                    }
                    //cout<<buffer<<endl;
                }
                in.close();   //关闭文件输入流

            }

            //cout<<endl;
            //cout<<"finshbuffer"<<buffer<<endl;


            out.write(buffer,encodedLen);
            cout<<endl;
            blockID++;
            resultVec.clear();

        }
    }
    out.close();
    key.close();   //关闭文件输入流
}

void Decode::split(string str, string pattern){
    char *src=new char[strlen(str.c_str())+1];
    strcpy(src,str.c_str());
    char *tmp=strtok(src,pattern.c_str());
    while (tmp!= NULL)
    {
            resultVec.push_back(string(tmp));
           // cout<<"tmp"<<tmp;
            tmp= strtok(NULL, pattern.c_str());
    }
}
void Decode::split(char *src, string pattern){
    char *tmp=strtok(src,pattern.c_str());
    while (tmp!= NULL)
    {
            resultVec.push_back(string(tmp));
            //cout<<"tmp"<<tmp;
            tmp= strtok(NULL, pattern.c_str());
    }
}
