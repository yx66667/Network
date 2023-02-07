#include <winsock2.h>
#include <iostream>
#include<string>
#include<fstream>
#include<time.h>
using namespace  std;
#pragma comment(lib, "ws2_32.lib")
struct UDP_Head {
	unsigned short type;		// 消息类型,SYN建立连接，ACK确认，PSH消息，FIN断开连接，END文件发送完毕
	unsigned short len;			// 数据长度，16位
	unsigned short checkSum;    // 校验和，16位
	unsigned short seq;		    // 序列号，16位

	void UDP_init(){
		this->type = 0;
		this->len = 0;
		this->checkSum = 0;
		this ->seq = 0;
	}
};
#define SYN 1  //设置消息类型
#define SYN_ACK 2
#define ACK 3
#define PSH 4
#define FIN 5
#define FIN_ACK 6
#define END 7
#define END_ACK 8
#define maxSize 10240 //设置文件最大传输量为10KB
#define maxTime 1000
unsigned short sendSeq = 0; //初始化序列号
int state = 0; //0表示还没有进行连接，1表示连接成功可以传输文件，2表示文件传输完毕断开连接
// 校验和：每16位相加后取反
u_short checkSumVerify(char* msg, int length) {
	int count = 0;
	char *temp;
	if(length % 2 == 1){ //判断一下奇偶条件
		temp = new char[length+1];
        count = (length + 1) / 2; //循环的次数
	    memset(temp, 0, (length + 1)); //将后几位设置为0
	    memcpy(temp, msg, length);
	}
	else{
		temp = new char[length];
		count = length / 2;
		memset(temp, 0, length);
		memcpy(temp, msg, length);
	}
	u_short *buf = (u_short *)temp;
	u_long checkSum = 0;
	while (count--) {
		checkSum += *buf++;
		if (checkSum & 0xffff0000) {
			checkSum &= 0xffff;
			checkSum++;
		}
	}
	return ~(checkSum & 0xffff);
}

int main() {
    SOCKET ClientSocket;
    SOCKADDR_IN ServAddress; //远程地址
    int ServAddressLen;

    // socket环境初始化
    WSADATA  wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cout << "WSA error:" << GetLastError() << endl;
        return false;
    }
    // 创建socket对象
    ClientSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (ClientSocket == INVALID_SOCKET)
    {
        closesocket(ClientSocket);
        ClientSocket = INVALID_SOCKET;
        return false;
    }
	//设置套接字为非阻塞模式 
	int iMode = 1; //1：非阻塞，0：阻塞 
	ioctlsocket(ClientSocket, FIONBIO, (u_long FAR*) & iMode);//非阻塞设置 

    // 服务端地址
    ServAddress.sin_family = AF_INET;
    ServAddress.sin_port = htons(10009);
	ServAddress.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
    ServAddressLen = sizeof(ServAddress);

Label: while(1){
		int sentLen = 0; //原始文件已经传过去的长度
		char *data = NULL; //要传的数据
		int length = 0; //要传数据的大小（字节）
		char *s = new char[20];
		cout<<"请输入要传输的文件名：";
		cin>>s;
		if(strcmp(s,"q") == 0){
			state = 2;
		}
		else{
			ifstream fin(s, ifstream::in | ios::binary);
			if (!fin) {
				cout << "不能打开文件" << endl;
				continue;
			}
			fin.seekg(0, fin.end);		// 指针定位在文件尾
			length = fin.tellg();	// 获取文件大小（字节）
			fin.seekg(0, fin.beg);		// 指针定位在文件头
			data = new char[length];
			memset(data, 0, sizeof(data));
			fin.read(data, length);
			fin.close();
		}

		clock_t start;
	
 
        while (1) {
		// 设置信息头
        UDP_Head h;
		h.UDP_init();
        h.seq = sendSeq;
		char *sendBuf;
		if(state == 0){ //建立连接
			h.type = SYN;  //连接标志
			h.len = 0;
			sendBuf = new char[1024];
			memset(sendBuf, 0, 1024);
			memcpy(sendBuf, &h, sizeof(h));
		}
		else if(state == 2){//准备断开连接
			h.type = FIN;
			h.len = 0;
			sendBuf = new char[1024];
			memset(sendBuf, 0, 1024);
			memcpy(sendBuf, &h, sizeof(h));
		}
		else{ //state =1 发送消息
			h.type = PSH;
			if(sentLen == 0) cout<<"------------------开始发送文件!------------------"<<endl;
			if((length - sentLen) > maxSize){ //文件这部分太大需要分段发送
				h.len = maxSize;
			}
			else if((length - sentLen) <= 0){  //文件发送完毕
				h.type = END;
				h.len = strlen(s);
			}
			else
				h.len =length - sentLen;  //文件这部分在规定大小内
			sendBuf = new char[h.len+sizeof(h)];
			memset(sendBuf, 0, h.len+sizeof(h));
			memcpy(sendBuf, &h, sizeof(h));
			// data是要传输的消息，s是文件名，sentLen是已发送的长度，作为分批次发送的偏移量
			if((length - sentLen) <= 0)
				memcpy(sendBuf + sizeof(h), s, h.len);
			else
				memcpy(sendBuf + sizeof(h), data + sentLen, h.len);
			sentLen += (int)h.len;
		}
		// 计算校验和
		h.checkSum = checkSumVerify(sendBuf, sizeof(h) + h.len);
		memcpy(sendBuf, &h, sizeof(h));
 
		//开始计时	
        start = clock();
		// 发送消息
		int len = sendto(ClientSocket, sendBuf, sizeof(h) + h.len, 0, (SOCKADDR*)&ServAddress, sizeof(SOCKADDR));
		cout<<"发送文件的字节数："<<h.len<<endl;
		if (len == -1) {
			cout << "发送消息失败" << endl;
			return false;
		}
		else{
			if(state == 0){
				cout << "【客户端发送连接】: [SYN] Seq=0" << endl;
			}
			if(state == 2){
				cout << "【客户端发送断开连接】: [FIN] Seq="<<h.seq << endl;
			}
			Sleep(100);
		}
		// 等待接收消息
		char *recvBuf = new char[1024];
	    memset(recvBuf, 0, 1024);
		int sendcount = 0; //重传文件的次数

		while (1){
			int checklen = recvfrom(ClientSocket, recvBuf, 1024, 0, (SOCKADDR*)&ServAddress, &ServAddressLen);
			if (checklen > 0 ) { //接收到了消息
				UDP_Head h2;
				h2.UDP_init();
				memcpy(&h2, recvBuf, sizeof(h2));
				// 验证消息的序列号和校验和
				cout<<"接收序列号:"<<h2.seq<<endl;
				int check = checkSumVerify(recvBuf,sizeof(h2));
				if (h2.seq == sendSeq && check == 0) {
					if(state == 0 && h2.type == SYN_ACK){ //类型检查，接收连接确认
						cout << "【服务端已经接收连接】: [SYN, ACK] Seq=0 Ack=0" << endl;
						cout<<"连接用时："<<(clock() - start) * 1000.0 / CLOCKS_PER_SEC << "ms" << endl;
						state = 1;
					}
					else if(state ==2 && h2.type == FIN_ACK){ //类型检查，断开连接确认
						cout << "【服务端已经断开连接】: [FIN, ACK] Ack="<<h2.seq << endl;
						return 0;
					}
					else if(h2.type == END_ACK){
						cout<<"------------------文件传输完毕!------------------"<<endl;
						goto Label;
					}
					else{ //接收消息
						cout<<"服务器已经接收到消息！发送接收消息用时："<<(clock() - start)* 1000.0 / CLOCKS_PER_SEC<<"ms"<<endl;
					}
					sendSeq = (sendSeq + 1) % 65536;
					break;
			    }
				else{  //出现了序列号错误或者校验和错误，什么也不做
					cout<<"接收序列号或者校验和有误"<<endl;
				}
			}
			double time = (clock() - start)* 1000.0 / CLOCKS_PER_SEC;
		    if(time > maxTime){   // 超时重传并重新计时
				sendcount++;
				if(sendcount > 5){
					break; //如果重传次数大于5，则丢弃该数据包
				}
				cout<<"传输消息超时：用时"<<time<<"ms"<<endl;
			    // 重新计时
				start = clock();
				// 重新发送数据
				cout<<"重新发送数据"<<endl;
				if (sendto(ClientSocket, sendBuf, sizeof(h) + h.len, 0, (SOCKADDR*)&ServAddress,sizeof(SOCKADDR)) == -1) {
			          cout << "发送消息失败" << endl;
			          return false;
		        }
			}
		}
	  }
	}
    closesocket(ClientSocket);
    WSACleanup();
    return 0;
}
