#include<iostream>
#include<Winsock2.h>//socket头文件
#include<cstring>
using namespace std;
#pragma comment(lib,"ws2_32.lib")  //静态加入lib文件，socket库，引用网络相关API  
//全局变量
const int BUFFER_SIZE = 1024;   //缓冲区大小
const int MAX_LINK_NUM = 6;     //服务端最大链接数为5
SOCKET Sock[MAX_LINK_NUM];      //存储套接字 0号为服务端
SOCKADDR_IN Addr[MAX_LINK_NUM]; //存储地址
char ConName[MAX_LINK_NUM][BUFFER_SIZE];  //连接客户端的名字
int total = 0;                  //当前已经链接的客服端服务数
//创建线程函数声明
DWORD WINAPI handlerRequestThread(LPVOID IpParameter);//服务器端处理线程 
int main() 
{
	//说明版本信息
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);//MAKEWORD(主版本号, 副版本号)
 
	//创建服务器socket
	SOCKET servSocket = socket(AF_INET, SOCK_STREAM, 0); 
 
	//服务器地址包装在一个结构体里面
	SOCKADDR_IN servAddr;          //套接字地址
	servAddr.sin_family = AF_INET;
	servAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");//服务端地址设置为本地回环地址
	servAddr.sin_port = htons(10009);//端口

	//绑定socket和服务器地址，给sokect绑定一个事件对象，用来接收客户端链接的事件
	bind(servSocket, (SOCKADDR*)&servAddr, sizeof(servAddr));
	WSAEVENT servEvent = WSACreateEvent();
	WSAEventSelect(servSocket, servEvent, FD_ALL_EVENTS); //绑定socket和事件
	Sock[0] = servSocket; //0号位存的是服务器的socket
 
	//开启监听
	listen(servSocket, 5);//未完成队列的大小为5，意思是监听连接数最大为5
    cout << "聊天室服务器开启" << endl;

	while(1)   //这个循环是因为可能不断有用户连接接入
	{
				WSANETWORKEVENTS networkEvents;
				WSAEnumNetworkEvents(servSocket, servEvent, &networkEvents);//查看是什么事件
				if (networkEvents.lNetworkEvents & FD_ACCEPT)//accept事件
				{
					if (networkEvents.iErrorCode[FD_ACCEPT_BIT] != 0) //判断是否连接成功
					{
						cout <<"发生连接错误：错误代码" << networkEvents.iErrorCode[FD_ACCEPT_BIT] << endl;
						continue;
					}
					//连接成功
					if (total + 1 < MAX_LINK_NUM)     //判断是否能继续加人
					{
						int nextIndex = total + 1;    //新用户端的数组下标
						int addrLen = sizeof(SOCKADDR);
						SOCKET connectSocket = accept(servSocket, (SOCKADDR*)&Addr[nextIndex], &addrLen);
						if (connectSocket != INVALID_SOCKET)  //判断是否是无效socket
						{
							Sock[nextIndex] = connectSocket;
							total++;                 //客户端连接数增加
							CloseHandle(CreateThread(NULL, NULL, handlerRequestThread, (LPVOID)&connectSocket, 0, 0)); //为新连接的客户增加一个线程
						}
					}
					
				}
	}
	//关闭socket库的收尾工作
	WSACleanup();
	return 0;
}
 
DWORD WINAPI handlerRequestThread(LPVOID IpParameter)
{
	//该线程负责处理服务端和各个客户端发生的事件
	SOCKET connectSocket = *(SOCKET*)IpParameter;//将空指针类型转成SOCKET类型
	WSAEVENT connectEvent = WSACreateEvent();    //绑定事件
	WSAEventSelect(connectSocket, connectEvent, FD_ALL_EVENTS);
	int index=total;
	char name[BUFFER_SIZE] = { 0 }; //记录名称

	while (1)  //这个循环是对一个用户，不停的有接收消息或者断开连接结束线程
	{
		if(name!=NULL)
		{
			for(int i = 1; i <=total; i++)
			{
				if(strcmp(name,ConName[i])==0)
				{
					index=i;
					break;
				}
			}
		}
		WSANETWORKEVENTS networkEvents; 
	    WSAEnumNetworkEvents(connectSocket, connectEvent, &networkEvents); //用来查看是什么事件
	    if (networkEvents.lNetworkEvents & FD_CLOSE)                       //客户端被关闭，即断开连接
		{
			 cout << "#" << index << "用户[" << name << "]退出了聊天室,当前连接数："<<total-1 << endl;
			 closesocket(connectSocket);
			 WSACloseEvent(connectEvent);

			//数组调整
			for (int j = index; j < total; j++)
			{
				Sock[j] = Sock[j + 1];
				Addr[j] = Addr[j + 1];
				strcpy(ConName[j],ConName[j+1]);
			}
			total--;
			//给所有客户端发送退出聊天室的消息
			char buf[BUFFER_SIZE] = "[";
			strcat(buf, name);
			strcat(buf, "]退出聊天室");
			for (int j = 1; j <=total; j++)
			{
				send(Sock[j], buf, sizeof(buf), 0);
			}	
			return 0;
		}
		else if (networkEvents.lNetworkEvents & FD_READ)//接收到消息
		{
			char buffer[BUFFER_SIZE] = { 0 };         //字符缓冲区
			char sendbuffer[BUFFER_SIZE] = { 0 };
			int num = recv(connectSocket, buffer, sizeof(buffer), 0);//num是接收到的字节数
			if (num > 0)                             //如果接收到的字符数大于0
			{	
				if(buffer[0]=='n'&&buffer[1]=='a'&&buffer[2]=='m'&&buffer[3]=='e')
				{ //首先接收名字
					strcpy(ConName[index],buffer+4);
					strcpy(name,ConName[index]);
					cout <<"#" << index<< "用户[" << name << "]进入聊天室，当前连接数：" << total << endl;
					//给所有客户端发送欢迎消息
					char buf[BUFFER_SIZE] = "欢迎[";
					strcat(buf, name);
					strcat(buf, "]进入聊天室");
					for (int j = 1; j <=total; j++) //播报给所有人
					{	
						send(Sock[j], buf, sizeof(buf),0);							
					}

				}
				else
				{
					sprintf(sendbuffer,"[%s]%s",ConName[index],buffer);
					//在服务端显示
					cout <<sendbuffer << endl;
					//广播给其他客户端
					for (int k = 1; k <= total; k++)
					{
						send(Sock[k], sendbuffer, sizeof(sendbuffer),0);
					}
				}
			}
		 }
		else
		{
		}
	}
	return 0;
}