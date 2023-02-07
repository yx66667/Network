// �������ͻ���
#include<iostream>
#include<Winsock2.h>//socketͷ�ļ�
#include<cstring>
#include <cstdio>
#include <ctime>
using namespace std;
#pragma comment(lib,"ws2_32.lib")   //socket��
const int BUFFER_SIZE = 1024;//��������С
DWORD WINAPI receiveThread(LPVOID IpParameter);
int main() {
	//˵���汾��Ϣ
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);//MAKEWORD(���汾��, ���汾��)
 
	//�����ͻ���socket
	SOCKET cliSocket = socket(AF_INET, SOCK_STREAM, 0);//������·����ʽ�׽���,���������������Զ�ѡ��Э��
 
	//��װ��ַ
	//�ͻ���
	SOCKADDR_IN cliAddr;
	cliAddr.sin_family = AF_INET;
	cliAddr.sin_addr.s_addr = inet_addr("127.0.0.1");//IP��ַ
	cliAddr.sin_port = htons(10009);//�˿ں�
	//�����
	SOCKADDR_IN servAddr;
	servAddr.sin_family = AF_INET;//sin_family��ʾЭ��أ�AF_INET��ʾTCP/IPЭ�顣
	servAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
	servAddr.sin_port = htons(10009);
 
	if (connect(cliSocket, (SOCKADDR*)&servAddr, sizeof(SOCKADDR)) == SOCKET_ERROR) 
	{
		cout << "���ӳ��ִ��󣬴������:" << WSAGetLastError() << endl;
	}
 
	//����������Ϣ�߳�
	CloseHandle(CreateThread(NULL, NULL, receiveThread, (LPVOID)&cliSocket, 0, 0));
	//���߳���������Ҫ���͵���Ϣ
	cout<<"������'name+�������'��ȷ��ȷ�����:"<<endl;
	while (1) 
	{
		char buf[BUFFER_SIZE] = { 0 };
		cin.getline(buf,sizeof(buf));
		if (strcmp(buf, "886") == 0)    //����ͨ��
		{
			break;
		}
		else if(buf[0]=='n'&&buf[1]=='a'&&buf[2]=='m'&&buf[3]=='e')   //��������
		{
			send(cliSocket, buf, sizeof(buf), 0);
		}
		else  //ͨ����Ϣ������ʱ���ǩ
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

DWORD WINAPI receiveThread(LPVOID IpParameter) //������Ϣ���߳�
{
	SOCKET cliSocket = *(SOCKET*)IpParameter;
	while (1)
	{
		char buffer[BUFFER_SIZE] = { 0 };
		int num = recv(cliSocket, buffer, sizeof(buffer), 0);//num�ǽ��յ����ֽ���
		if (num> 0)//������յ����ַ�������0
		{
			cout << buffer << endl;
		}
		else if (num<0)//������յ����ַ���С��0��˵���Ͽ�����
		{
			cout << "��������Ͽ�����" << endl;
			break;
		}
	}
	return 0;
}
