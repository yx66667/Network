#include<iostream>
#include<Winsock2.h>//socketͷ�ļ�
#include<cstring>
using namespace std;
#pragma comment(lib,"ws2_32.lib")  //��̬����lib�ļ���socket�⣬�����������API  
//ȫ�ֱ���
const int BUFFER_SIZE = 1024;   //��������С
const int MAX_LINK_NUM = 6;     //��������������Ϊ5
SOCKET Sock[MAX_LINK_NUM];      //�洢�׽��� 0��Ϊ�����
SOCKADDR_IN Addr[MAX_LINK_NUM]; //�洢��ַ
char ConName[MAX_LINK_NUM][BUFFER_SIZE];  //���ӿͻ��˵�����
int total = 0;                  //��ǰ�Ѿ����ӵĿͷ��˷�����
//�����̺߳�������
DWORD WINAPI handlerRequestThread(LPVOID IpParameter);//�������˴����߳� 
int main() 
{
	//˵���汾��Ϣ
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);//MAKEWORD(���汾��, ���汾��)
 
	//����������socket
	SOCKET servSocket = socket(AF_INET, SOCK_STREAM, 0); 
 
	//��������ַ��װ��һ���ṹ������
	SOCKADDR_IN servAddr;          //�׽��ֵ�ַ
	servAddr.sin_family = AF_INET;
	servAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");//����˵�ַ����Ϊ���ػػ���ַ
	servAddr.sin_port = htons(10009);//�˿�

	//��socket�ͷ�������ַ����sokect��һ���¼������������տͻ������ӵ��¼�
	bind(servSocket, (SOCKADDR*)&servAddr, sizeof(servAddr));
	WSAEVENT servEvent = WSACreateEvent();
	WSAEventSelect(servSocket, servEvent, FD_ALL_EVENTS); //��socket���¼�
	Sock[0] = servSocket; //0��λ����Ƿ�������socket
 
	//��������
	listen(servSocket, 5);//δ��ɶ��еĴ�СΪ5����˼�Ǽ������������Ϊ5
    cout << "�����ҷ���������" << endl;

	while(1)   //���ѭ������Ϊ���ܲ������û����ӽ���
	{
				WSANETWORKEVENTS networkEvents;
				WSAEnumNetworkEvents(servSocket, servEvent, &networkEvents);//�鿴��ʲô�¼�
				if (networkEvents.lNetworkEvents & FD_ACCEPT)//accept�¼�
				{
					if (networkEvents.iErrorCode[FD_ACCEPT_BIT] != 0) //�ж��Ƿ����ӳɹ�
					{
						cout <<"�������Ӵ��󣺴������" << networkEvents.iErrorCode[FD_ACCEPT_BIT] << endl;
						continue;
					}
					//���ӳɹ�
					if (total + 1 < MAX_LINK_NUM)     //�ж��Ƿ��ܼ�������
					{
						int nextIndex = total + 1;    //���û��˵������±�
						int addrLen = sizeof(SOCKADDR);
						SOCKET connectSocket = accept(servSocket, (SOCKADDR*)&Addr[nextIndex], &addrLen);
						if (connectSocket != INVALID_SOCKET)  //�ж��Ƿ�����Чsocket
						{
							Sock[nextIndex] = connectSocket;
							total++;                 //�ͻ�������������
							CloseHandle(CreateThread(NULL, NULL, handlerRequestThread, (LPVOID)&connectSocket, 0, 0)); //Ϊ�����ӵĿͻ�����һ���߳�
						}
					}
					
				}
	}
	//�ر�socket�����β����
	WSACleanup();
	return 0;
}
 
DWORD WINAPI handlerRequestThread(LPVOID IpParameter)
{
	//���̸߳��������˺͸����ͻ��˷������¼�
	SOCKET connectSocket = *(SOCKET*)IpParameter;//����ָ������ת��SOCKET����
	WSAEVENT connectEvent = WSACreateEvent();    //���¼�
	WSAEventSelect(connectSocket, connectEvent, FD_ALL_EVENTS);
	int index=total;
	char name[BUFFER_SIZE] = { 0 }; //��¼����

	while (1)  //���ѭ���Ƕ�һ���û�����ͣ���н�����Ϣ���߶Ͽ����ӽ����߳�
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
	    WSAEnumNetworkEvents(connectSocket, connectEvent, &networkEvents); //�����鿴��ʲô�¼�
	    if (networkEvents.lNetworkEvents & FD_CLOSE)                       //�ͻ��˱��رգ����Ͽ�����
		{
			 cout << "#" << index << "�û�[" << name << "]�˳���������,��ǰ��������"<<total-1 << endl;
			 closesocket(connectSocket);
			 WSACloseEvent(connectEvent);

			//�������
			for (int j = index; j < total; j++)
			{
				Sock[j] = Sock[j + 1];
				Addr[j] = Addr[j + 1];
				strcpy(ConName[j],ConName[j+1]);
			}
			total--;
			//�����пͻ��˷����˳������ҵ���Ϣ
			char buf[BUFFER_SIZE] = "[";
			strcat(buf, name);
			strcat(buf, "]�˳�������");
			for (int j = 1; j <=total; j++)
			{
				send(Sock[j], buf, sizeof(buf), 0);
			}	
			return 0;
		}
		else if (networkEvents.lNetworkEvents & FD_READ)//���յ���Ϣ
		{
			char buffer[BUFFER_SIZE] = { 0 };         //�ַ�������
			char sendbuffer[BUFFER_SIZE] = { 0 };
			int num = recv(connectSocket, buffer, sizeof(buffer), 0);//num�ǽ��յ����ֽ���
			if (num > 0)                             //������յ����ַ�������0
			{	
				if(buffer[0]=='n'&&buffer[1]=='a'&&buffer[2]=='m'&&buffer[3]=='e')
				{ //���Ƚ�������
					strcpy(ConName[index],buffer+4);
					strcpy(name,ConName[index]);
					cout <<"#" << index<< "�û�[" << name << "]���������ң���ǰ��������" << total << endl;
					//�����пͻ��˷��ͻ�ӭ��Ϣ
					char buf[BUFFER_SIZE] = "��ӭ[";
					strcat(buf, name);
					strcat(buf, "]����������");
					for (int j = 1; j <=total; j++) //������������
					{	
						send(Sock[j], buf, sizeof(buf),0);							
					}

				}
				else
				{
					sprintf(sendbuffer,"[%s]%s",ConName[index],buffer);
					//�ڷ������ʾ
					cout <<sendbuffer << endl;
					//�㲥�������ͻ���
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