 #include <winsock2.h>
#include <iostream>
#include<string>
#include<fstream>
#include<time.h>
using namespace  std;
#pragma comment(lib, "ws2_32.lib")
struct UDP_Head {
	unsigned short type;		// 消息类型,SYN建立连接，ACK确认，PSH消息
	unsigned short len;			// 数据长度，16位
	unsigned short checkSum;		// 校验和，16位
	unsigned short seq;		// 发送序号，16位

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
#define maxSize 10240
unsigned short waitSeq = 0; //初始化序列号
int state = 0; //0表示还没有进行连接，1表示连接可以传输文件，2表示文件传输完毕断开连接

// 校验和：每16位相加后取反,参数是字节型的消息和长度（单位:字节）
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
    SOCKET ServSocket;
    SOCKADDR_IN ServAddress;   
    SOCKADDR_IN ClientAddress; 
    int ClientAddressLen;

    // socket环境初始化
    WSADATA  wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cout << "WSA error:" << GetLastError() << endl;
        return false;
    }
    // 创建socket对象
    ServSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);  //使用UDP协议
    if (ServSocket == INVALID_SOCKET)
    {
        closesocket(ServSocket);
        ServSocket = INVALID_SOCKET;
        return false;
    }
	//设置套接字为非阻塞模式 
	int iMode = 1; //1：非阻塞，0：阻塞 
	ioctlsocket(ServSocket, FIONBIO, (u_long FAR*) & iMode);//非阻塞设置 
 
    // 绑定IP和端口号
    ServAddress.sin_family = AF_INET;
    ServAddress.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
    ServAddress.sin_port = htons(10009);
    bind(ServSocket, (sockaddr*)&ServAddress, sizeof(SOCKADDR));
	ClientAddressLen = sizeof(ClientAddress);

	clock_t start;//服务端记录接收一个完整文件的时间

	// 等待接收消息
	while(1){
		int totalLen = 0; //接收文件的总长度
		char *filename; //接收文件的文件名

		UDP_Head h1; //接收到消息UDP头
		h1.UDP_init();

		char *data = new char[20000000]; //存储一个文件的所有数据，最多接收20MB大小
		char *sendBuf;
		char *recvBuf = new char[maxSize + sizeof(h1)];
		memset(recvBuf, 0, maxSize + sizeof(h1));

		while (1) {
			// 收到消息需要验证校验和及序列号
			int checklen = recvfrom(ServSocket, recvBuf, maxSize+sizeof(h1), 0, (SOCKADDR*)&ClientAddress, &ClientAddressLen);
			if ( checklen > 0) {
			if( totalLen == 0){
				start = clock(); //接收字节数为0时开始计时
			}
			memcpy(&h1, recvBuf, sizeof(h1));
			UDP_Head h2;
			h2.UDP_init();
			h2.type = ACK;
			sendBuf = new char[1024];
			memset(sendBuf, 0, 1024);
			cout<<"接收序列号:"<<h1.seq<<endl;
			int check = checkSumVerify(recvBuf, h1.len+sizeof(h1));
			//cout<<"接收消息的校验和："<<check<<endl;
			if (h1.seq == waitSeq && check == 0) {
				if(state == 0 && h1.type == SYN){ //确认建立连接
					h2.type = SYN_ACK;
					cout<<"【服务器收到连接请求】"<<endl;
					state = 1;
				}
				else if(h1.type == FIN){ //断开连接
					h2.type = FIN_ACK;
					state = 2;
					cout<<"【服务器收到断开连接请求】"<<endl;
				}
				else if(h1.type == END){ //一个文件传输完毕
					h2.type = END_ACK;
					cout<<"-----------文件接收完毕-----------"<<endl;
				}
				h2.seq = waitSeq;
				h2.checkSum = checkSumVerify((char *)&h2, sizeof(h2));
				memcpy(sendBuf, &h2, sizeof(h2));
				sendto(ServSocket, sendBuf, sizeof(h2), 0, (SOCKADDR*)&ClientAddress, sizeof(SOCKADDR));
				if(h2.type == END_ACK){
					filename = new char[h1.len+1];
					memset(filename ,0 ,h1.len+1);
					memcpy(filename, recvBuf + sizeof(h1), h1.len); //接收文件名
					cout<<"【接收到的文件名】："<<filename<<endl;
					double time = (clock() - start)* 1.0 / CLOCKS_PER_SEC;
					cout<<"传输文件总用时："<<time<<"s"<<endl;
					cout<<"平均吞吐率："<<(double)(totalLen/1000)/time<<"KB/s"<<endl;
					break;
				}
				if(state == 2)
					goto label;
				memcpy(data + totalLen, recvBuf + sizeof(h1), h1.len);
				cout<<"接收到文件字节数："<<h1.len<<endl;
				totalLen += (int)h1.len;
				waitSeq = (waitSeq + 1) % 65536;
				//Sleep(100);
			}
			else {	// 差错重传
				if(state == 0 && h1.type == SYN){
					h2.type = SYN_ACK;
					cout<<"服务器收到连接请求，但请求发生错误，要求重传"<<endl;
				}
				else{
					cout<<"发生序列号错误或者文件传输有误！"<<endl;
				}
				h2.seq = (waitSeq - 1)% 65536;
				cout<<h2.seq<<endl;
				h2.checkSum = checkSumVerify((char *)&h2, sizeof(h2));
				memcpy(sendBuf, &h2, sizeof(h2));
				sendto(ServSocket, sendBuf, sizeof(h2), 0, (SOCKADDR*)&ClientAddress, sizeof(SOCKADDR));
				//Sleep(100);
			}
			}
		}
	// 以二进制方式写入文件
    ofstream fout(filename, ofstream::out | ios::binary);
	if (!fout) {
		cout << "不能打开文件" << endl;
		continue;
	}
	fout.write(data, totalLen);
	fout.close();
	}

label:closesocket(ServSocket);
      WSACleanup();
      return 0;
}