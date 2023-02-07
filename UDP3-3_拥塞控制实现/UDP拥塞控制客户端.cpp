#include <winsock2.h>
#include <iostream>
#include<string>
#include<fstream>
#include<time.h>
#include<queue>
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
struct buf{  //buf结构体用于将缓冲区数据存到队列中
	bool isACK;
	char *sendbuf;
	int len;
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
#define maxTime  1000 //设置超时重传的时间为1s
#define maxwindow 20 //设置通告的流量控制窗口最大为20

unsigned short nextseqnum = 1; //发送缓冲区后沿，每次成功发送一条消息，+1
unsigned short base = 1; //发送缓冲区前沿,每次成功接收到ACK，+1
int state = 0; //客户端的状态：0表示还没有进行连接，1表示连接成功可以传输文件，2表示文件传输完毕断开连接,3表示一个文件发送完了
//（状态设置3主要是因为：程序设计的总是最后发文件名，为了保证文件名发完之后，在等待接收ACK消息的时候不会重复发送文件名，在下一次发送文件时候再置1）
queue<buf> sndpkt; //利用队列表示缓冲区

float cwnd = 1.0; //TCP拥塞控制窗口
int ssthresh = 16; //阈值
int dupACKcount = 0; //重复ACK的个数
int status = 1; //状态：1表示慢启动阶段，2表示拥塞避免阶段，3表示快速恢复阶段

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

    // 路由器地址
    ServAddress.sin_family = AF_INET;
    ServAddress.sin_port = htons(10009);
	ServAddress.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
    ServAddressLen = sizeof(ServAddress);

	clock_t start; //计时器：只记开始时间

 Label:while(1){
		if(state == 3)  //状态3表示刚发送完一个文件，要发送下一个文件了，状态置1
			state = 1;
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
		char *recvBuf = new char[1024]; //接收缓冲区
	    memset(recvBuf, 0, 1024);
		int sendcount = 0; //重传消息的次数，最大不能超过5
		char *sendBuf; //发送缓冲区，在发送时分配

        while (1) {
			//发送消息
			if(nextseqnum < base + cwnd && state!=3){ //状态为3时不能发送消息，但可以接收消息
				UDP_Head h; // 发送消息的头
				h.UDP_init();
				h.seq = nextseqnum;
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
						state = 3;
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
					if(base == nextseqnum) //滑动窗口大小此时为0，发送以后开始计时	
						start = clock();
					Sleep(100);
					nextseqnum = (nextseqnum + 1) % 65536; //发送端口后沿+1
					buf b1;
					b1.isACK = false; //表示还没有接收过确认消息
					b1.sendbuf = sendBuf;
					b1.len = h.len + sizeof(h);
					sndpkt.push(b1); //将发送的消息存入队列，以便之后超时重传
					cout<<"发送窗口大小："<<nextseqnum - base<<endl;
				}
			}


			// 接收消息
			int checklen = recvfrom(ClientSocket, recvBuf, 1024, 0, (SOCKADDR*)&ServAddress, &ServAddressLen);
			if (checklen > 0 ) { //接收到了消息
				UDP_Head h2;
				h2.UDP_init();
				memcpy(&h2, recvBuf, sizeof(h2));
				// 验证消息的序列号和校验和
				cout<<"接收序列号:"<<h2.seq<<endl;
				int check = checkSumVerify(recvBuf,sizeof(h2));

				//接收到的校验和和序列号无误,ACK序列号在滑动窗口内
				if ( h2.seq >= base && h2.seq < nextseqnum && check == 0 ) { 

					/*
					拥塞控制：三种状态分别讨论
					*/
					if(status == 1){ //慢启动状态：每收到一个ACK，cwnd加1
						cwnd = cwnd + 1;
						dupACKcount = 0;
						if(cwnd >=ssthresh){
							status = 2;
						}
					}
					else if(status == 2){ //拥塞避免状态：每个RTT，cwnd加1
						cwnd = cwnd + 1.0 / cwnd ;
						dupACKcount = 0;
					}
					else{
						//快速恢复状态:new reno算法，当实际窗口大小为0时（所有发出的消息都被确认了），
				        //才到拥塞避免状态，否则，维持快速恢复状态
						dupACKcount = 0;
						if(base == nextseqnum){
							cwnd = ssthresh;
							status = 2;
						}else{
							cwnd = cwnd + 1;
						}
					}
					if(cwnd > maxwindow){ //拥塞窗口如果大于通告的流量窗口，那么取较小值
						cwnd = maxwindow;
					}

					/*
					ACK正确时，分两种情况：1、落在窗口内但序列号不是base 2、序列号为base
					*/
					if(h2.seq != base){ //落在窗口内但序列号不是base时，循环将队列遍历一遍，将那个收到的消息标记
						for(unsigned short i = base; i < nextseqnum; i++){
							if(i == h2.seq){
								sndpkt.front().isACK = true; //标记已经接收ACK消息了

							}
						    sndpkt.push(sndpkt.front());
							sndpkt.pop();
						}
					}
					else{
						unsigned short temp = base;
						for(unsigned short i = temp; i < nextseqnum; i++){ //在队列中循环，遇到连续的已经被接收的进行消息处理
							if(i != temp && sndpkt.front().isACK == false) break;  //遇到第一个没有被接收的消息，退出循环
							sendcount = 0; //重传次数清零
							if(state == 0 && h2.type == SYN_ACK){ //类型检查，接收连接确认
								cout << "【服务端已经接收连接】: [SYN, ACK] Seq=0 Ack=0" << endl;
								state = 1;
							}
							else if(state ==2 && h2.type == FIN_ACK){ //类型检查，断开连接确认
								cout << "【服务端已经断开连接】: [FIN, ACK] Ack="<<h2.seq << endl;
								return 0;
							}
							else if(h2.type == END_ACK){
								cout<<"------------------文件传输完毕!------------------"<<endl;
								sndpkt.pop();
								base = (base + 1) % 65536;  //发送端口前沿+1
								if(base != nextseqnum) //更新计时器
									start = clock();	
								cout<<"发送窗口大小："<<nextseqnum - base<<endl;
								goto Label;
							}
							else{ //接收消息
								cout<<"服务器已经接收到消息！从发送到处理消息用时："<<(clock() - start)* 1000.0 / CLOCKS_PER_SEC<<"ms"<<endl;
							}
							sndpkt.pop();
							base = (base + 1) % 65536;  //发送端口前沿+1
							if(base != nextseqnum) //更新计时器
								start = clock();	
							cout<<"发送窗口大小："<<nextseqnum - base<<"    cwnd："<<cwnd<<"    ssthresh："<<ssthresh<<endl;
						}
					}
			    }

				else{  //出现了序列号错误或者校验和错误，什么也不做
					cout<<"接收序列号或者校验和有误"<<endl;
					if(check == 0){

					/*
					check为0说明校验和无误，只是收到重复ACK，要进行拥塞控制
					*/
						if(status == 1 || status == 2){ //慢启动状态和拥塞避免状态
							dupACKcount++;
							if(dupACKcount == 3){
								ssthresh = cwnd / 2;
								cwnd = ssthresh + 3;
								status = 3;
							}
						}
						else{ //快速恢复状态
							cwnd++;
						}
						if(cwnd > maxwindow){ //拥塞窗口如果大于通告的流量窗口，那么取较小值
							cwnd = maxwindow;
					    }
						cout<<"发送窗口大小："<<nextseqnum - base<<"    cwnd："<<cwnd<<"    ssthresh："<<ssthresh<<endl;
					}
				}
			}

			// 超时重传并重新计时
			double time = (clock() - start)* 1000.0 / CLOCKS_PER_SEC;
		    if(time > maxTime){  
				int queueSize = sndpkt.size(); //要重发的消息数量，只有缓冲区的数量大于0时，才重发消息并重新计时
				cout<<"重发消息的数量："<<queueSize<<endl;
				if(queueSize > 0){
					sendcount++;
					if(sendcount > 5){
						break; //如果重传次数大于5，则丢弃该数据包
					}
					cout<<"传输消息超时：用时"<<time<<"ms"<<endl;
					// 重新计时
					start = clock();
					// 重新发送数据
					cout<<"重新发送数据"<<endl;
					while(queueSize > 0){
					    buf b2 = sndpkt.front();
						int len = b2.len;  //要重发消息的长度
						char *temp = b2.sendbuf;
						UDP_Head h;
						h.UDP_init();
						memcpy(&h, temp, sizeof(h));
						cout<<"重发消息的序列号："<<h.seq<<endl;
						if (sendto(ClientSocket, temp, len, 0, (SOCKADDR*)&ServAddress,sizeof(SOCKADDR)) == -1) {
							cout << "发送消息失败" << endl;
							return false;
						}
						sndpkt.push(sndpkt.front());
						sndpkt.pop();
						queueSize --;
						//Sleep(100);
					}

					/*
					进行拥塞控制
					*/
					ssthresh = cwnd / 2;
					cwnd = 1.0;
					dupACKcount = 0;
					status = 1; //进入慢启动阶段
				}
			}
		}
	}
    closesocket(ClientSocket);
    WSACleanup();
    return 0;
}
