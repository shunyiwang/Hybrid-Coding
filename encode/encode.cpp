#include "encode.h"
#include "md5.h"
#include <iostream>
#include <fstream>
#include <time.h>
using namespace std;
void Encode::setFileMD5(){
    ifstream in((const char *)readFilePath.toLocal8Bit(), ios::binary);   //定义文件输入流
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
    FileMD5=QString(QString::fromLocal8Bit(md5.toString().c_str()));   //显示生成的MD5编码
}


//按照填充分割的方式分割
void Encode::split(){
    ifstream intmp((const char *)readFilePath.toLocal8Bit(), ios::binary);   //定义文件输入流
    intmp.seekg(0 , ios::end);
    int len = intmp.tellg();//文件总长度
    intmp.close();   //关闭文件输入流

    int blockLen=1024;//编码块长度
    int n=paramN;
    if(len%n==0){
        blockLen=len/n;
    }else{
        blockLen=len/n+1;
    }
    QString pathtmp;
    char buffer[blockLen];   //每次读取blockLen字节
    ifstream in((const char *)readFilePath.toLocal8Bit(), ios::binary);   //定义文件输入流
    std::streamsize length;
    int index=1;
    for(int i=0;i<n;i++) {   //循环读取文件，直至读取完
        in.read(buffer, blockLen);
        length = in.gcount();
        pathtmp=originalBlockPath+QString::number(index, 10);
        cout<<(const char *)pathtmp.toLocal8Bit()<<endl;
        ofstream out((const char *)pathtmp.toLocal8Bit(),ios::binary);
        out.write(buffer,blockLen);
        out.close();
        index++;
    }
    in.close();   //关闭文件输入流
}

//按照不同的模式选择度分布函数,生成度分布数组
void Encode::degree(){
    int n=paramN;
    int m=paramM;
 //   cout<<n<<m<<endl;
    double p[n];
    double q[n];
    if(mode==1){
        //均匀分布
        for(int i=0;i<n;i++){
            p[i]=1.0/n;
        }
        //积累概率
        for(int i=0;i<n;i++){
            for(int j=0;j<=i;j++){
                q[i]+=p[j];
            }
        }
        //生成随机数
        srand(time(0));
        for(int i=0;i<m;i++){
            double r=rand()%100/(double)101;
            for(int j=0;j<n;j++){
                if(r<=q[j]){
                    d.push_back(j+1);
                    break;
                }
            }
            //cout<<d[i]<<endl;
        }
    }else if(mode==2){

    }
}

//进行编码
//用混合编码进行，其中单数取非
void Encode::encode(){
    cout<<endl;
    cout<<"----------Map----------"<<endl;
    for(int i=0;i<paramM;i++){
        int degreeTmp=d[i];//获取每个编码块的度
        int num[paramN];//生成原始块标识
        vector<int> flags;//保存每个编码块原始块标识
        for(int k=0;k<paramN;k++){
            num[k]=k+1;
        }
        int size=paramN;
        for(int j=0;j<degreeTmp;j++){//获取原始块
            srand(time(0)+(i+1)*(j+1));
            int r=rand()%size;
            flags.push_back(num[r]);
            //将第r个放到最后
            int tmp=num[size-1];
            num[size-1]=num[r];
            num[r]=tmp;
            size--;
        }

        if(degreeTmp%2==1){
            cout<<i<<"=(";
        }else{
            cout<<i<<"=";
        }

        for(int k=0;k<flags.size();k++){
            if(k!=flags.size()-1){
                cout<<flags[k]<<"^";
            }else{
                cout<<flags[k];
            }
        }
        if(degreeTmp%2==1){
            cout<<")!"<<endl;
        }else{
            cout<<endl;
        }


        int encodedLen;
        char buffer[2048];//设置编码块缓存
        QString pathTmp=originalBlockPath+QString::number(flags[0], 10);//选择原始块
        ifstream intmp((const char *)pathTmp.toLocal8Bit(), ios::binary);   //定义文件输入流
        intmp.seekg(0 , ios::end);
        encodedLen = intmp.tellg();//编码块长度
        intmp.close();   //关闭文件输入流

        //执行异或运算
        for(int k=0;k<flags.size();k++){
            pathTmp=originalBlockPath+QString::number(flags[k], 10);//选择原始块
            ifstream in((const char *)pathTmp.toLocal8Bit(), ios::binary);   //定义文件输入流
            char tmp[encodedLen];

            if(k==0){
                in.read(buffer, encodedLen);
            }else{
                in.read(tmp, encodedLen);
                //cout<<tmp<<endl;
               // cout<<buffer<<endl;
                for(int j=0;j<encodedLen;j++){
                    buffer[j]^=tmp[j];
                }
            }
            in.close();   //关闭文件输入流
        }
        //cout<<buffer<<endl;
        QString encodePath=encodedBlockPath+QString::number(i+1, 10);
        ofstream out((const char *)encodePath.toLocal8Bit(),ios::binary);
        out.write(buffer,encodedLen);
        out.close();

    }
    cout<<"-----------------------"<<endl;
    cout<<endl;
}
