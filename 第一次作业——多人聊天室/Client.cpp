// 聊天程序客户端
#include<iostream>
#include<Winsock2.h>//socket头文件
#include<cstring>
#include <cstdio>
#include <ctime>
using namespace std;
#pragma comment(lib,"ws2_32.lib")   //socket库
const int BUFFER_SIZE = 1024;//缓冲区大小
DWORD WINAPI receiveThread(LPVOID IpParameter);
int main() {
	//说明版本信息
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);//MAKEWORD(主版本号, 副版本号)
 
	//创建客户端socket
	SOCKET cliSocket = socket(AF_INET, SOCK_STREAM, 0);//面向网路的流式套接字,第三个参数代表自动选择协议
 
	//包装地址
	//客户端
	SOCKADDR_IN cliAddr;
	cliAddr.sin_family = AF_INET;
	cliAddr.sin_addr.s_addr = inet_addr("127.0.0.1");//IP地址
	cliAddr.sin_port = htons(10009);//端口号
	//服务端
	SOCKADDR_IN servAddr;
	servAddr.sin_family = AF_INET;//sin_family表示协议簇，AF_INET表示TCP/IP协议。
	servAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
	servAddr.sin_port = htons(10009);
 
	if (connect(cliSocket, (SOCKADDR*)&servAddr, sizeof(SOCKADDR)) == SOCKET_ERROR) 
	{
		cout << "链接出现错误，错误代码:" << WSAGetLastError() << endl;
	}
 
	//创建接受消息线程
	CloseHandle(CreateThread(NULL, NULL, receiveThread, (LPVOID)&cliSocket, 0, 0));
	//主线程用于输入要发送的消息
	cout<<"请输入'name+你的名称'来确定确认身份:"<<endl;
	while (1) 
	{
		char buf[BUFFER_SIZE] = { 0 };
		cin.getline(buf,sizeof(buf));
		if (strcmp(buf, "886") == 0)    //结束通话
		{
			break;
		}
		else if(buf[0]=='n'&&buf[1]=='a'&&buf[2]=='m'&&buf[3]=='e')   //报上姓名
		{
			send(cliSocket, buf, sizeof(buf), 0);
		}
		else  //通话消息，加上时间标签
		{
			time_t rawtime;
			struct tm *tminfo;
			time(&rawtime);
			tminfo = localtime(&rawtime);
			char sendbuf[BUFFER_SIZE] = { 0 };
			sprintf(sendbuf,"%2d-%2d-%2d %2d:%2d:%2d %s",tminfo->tm_year + 1900, tminfo->tm_mon + 1, tminfo->tm_mday,
			tminfo->tm_hour, tminfo->tm_min, tminfo->tm_sec,buf);
			send(cliSocket, sendbuf, sizeof(sendbuf), 0);
		}

	}
	closesocket(cliSocket);
	WSACleanup();
	return 0;
}

DWORD WINAPI receiveThread(LPVOID IpParameter) //接收消息的线程
{
	SOCKET cliSocket = *(SOCKET*)IpParameter;
	while (1)
	{
		char buffer[BUFFER_SIZE] = { 0 };
		int num = recv(cliSocket, buffer, sizeof(buffer), 0);//num是接收到的字节数
		if (num> 0)//如果接收到的字符数大于0
		{
			cout << buffer << endl;
		}
		else if (num<0)//如果接收到的字符数小于0就说明断开连接
		{
			cout << "与服务器断开连接" << endl;
			break;
		}
	}
	return 0;
}
