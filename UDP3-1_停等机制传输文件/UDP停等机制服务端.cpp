 #include <winsock2.h>
#include <iostream>
#include<string>
#include<fstream>
#include<time.h>
using namespace  std;
#pragma comment(lib, "ws2_32.lib")
struct UDP_Head {
	unsigned short type;		// ��Ϣ����,SYN�������ӣ�ACKȷ�ϣ�PSH��Ϣ
	unsigned short len;			// ���ݳ��ȣ�16λ
	unsigned short checkSum;		// У��ͣ�16λ
	unsigned short seq;		// ������ţ�16λ

	void UDP_init(){
		this->type = 0;
		this->len = 0;
		this->checkSum = 0;
		this ->seq = 0;
	}
};
#define SYN 1  //������Ϣ����
#define SYN_ACK 2
#define ACK 3
#define PSH 4
#define FIN 5
#define FIN_ACK 6
#define END 7
#define END_ACK 8
#define maxSize 10240
unsigned short waitSeq = 0; //��ʼ�����к�
int state = 0; //0��ʾ��û�н������ӣ�1��ʾ���ӿ��Դ����ļ���2��ʾ�ļ�������϶Ͽ�����

// У��ͣ�ÿ16λ��Ӻ�ȡ��,�������ֽ��͵���Ϣ�ͳ��ȣ���λ:�ֽڣ�
u_short checkSumVerify(char* msg, int length) {
	int count = 0;
	char *temp;
	if(length % 2 == 1){ //�ж�һ����ż����
		temp = new char[length+1];
        count = (length + 1) / 2; //ѭ���Ĵ���
	    memset(temp, 0, (length + 1)); //����λ����Ϊ0
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

    // socket������ʼ��
    WSADATA  wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cout << "WSA error:" << GetLastError() << endl;
        return false;
    }
    // ����socket����
    ServSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);  //ʹ��UDPЭ��
    if (ServSocket == INVALID_SOCKET)
    {
        closesocket(ServSocket);
        ServSocket = INVALID_SOCKET;
        return false;
    }
	//�����׽���Ϊ������ģʽ 
	int iMode = 1; //1����������0������ 
	ioctlsocket(ServSocket, FIONBIO, (u_long FAR*) & iMode);//���������� 
 
    // ��IP�Ͷ˿ں�
    ServAddress.sin_family = AF_INET;
    ServAddress.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
    ServAddress.sin_port = htons(10009);
    bind(ServSocket, (sockaddr*)&ServAddress, sizeof(SOCKADDR));
	ClientAddressLen = sizeof(ClientAddress);

	clock_t start;//����˼�¼����һ�������ļ���ʱ��

	// �ȴ�������Ϣ
	while(1){
		int totalLen = 0; //�����ļ����ܳ���
		char *filename; //�����ļ����ļ���

		UDP_Head h1; //���յ���ϢUDPͷ
		h1.UDP_init();

		char *data = new char[20000000]; //�洢һ���ļ����������ݣ�������20MB��С
		char *sendBuf;
		char *recvBuf = new char[maxSize + sizeof(h1)];
		memset(recvBuf, 0, maxSize + sizeof(h1));

		while (1) {
			// �յ���Ϣ��Ҫ��֤У��ͼ����к�
			int checklen = recvfrom(ServSocket, recvBuf, maxSize+sizeof(h1), 0, (SOCKADDR*)&ClientAddress, &ClientAddressLen);
			if ( checklen > 0) {
			if( totalLen == 0){
				start = clock(); //�����ֽ���Ϊ0ʱ��ʼ��ʱ
			}
			memcpy(&h1, recvBuf, sizeof(h1));
			UDP_Head h2;
			h2.UDP_init();
			h2.type = ACK;
			sendBuf = new char[1024];
			memset(sendBuf, 0, 1024);
			cout<<"�������к�:"<<h1.seq<<endl;
			int check = checkSumVerify(recvBuf, h1.len+sizeof(h1));
			//cout<<"������Ϣ��У��ͣ�"<<check<<endl;
			if (h1.seq == waitSeq && check == 0) {
				if(state == 0 && h1.type == SYN){ //ȷ�Ͻ�������
					h2.type = SYN_ACK;
					cout<<"���������յ���������"<<endl;
					state = 1;
				}
				else if(h1.type == FIN){ //�Ͽ�����
					h2.type = FIN_ACK;
					state = 2;
					cout<<"���������յ��Ͽ���������"<<endl;
				}
				else if(h1.type == END){ //һ���ļ��������
					h2.type = END_ACK;
					cout<<"-----------�ļ��������-----------"<<endl;
				}
				h2.seq = waitSeq;
				h2.checkSum = checkSumVerify((char *)&h2, sizeof(h2));
				memcpy(sendBuf, &h2, sizeof(h2));
				sendto(ServSocket, sendBuf, sizeof(h2), 0, (SOCKADDR*)&ClientAddress, sizeof(SOCKADDR));
				if(h2.type == END_ACK){
					filename = new char[h1.len+1];
					memset(filename ,0 ,h1.len+1);
					memcpy(filename, recvBuf + sizeof(h1), h1.len); //�����ļ���
					cout<<"�����յ����ļ�������"<<filename<<endl;
					double time = (clock() - start)* 1.0 / CLOCKS_PER_SEC;
					cout<<"�����ļ�����ʱ��"<<time<<"s"<<endl;
					cout<<"ƽ�������ʣ�"<<(double)(totalLen/1000)/time<<"KB/s"<<endl;
					break;
				}
				if(state == 2)
					goto label;
				memcpy(data + totalLen, recvBuf + sizeof(h1), h1.len);
				cout<<"���յ��ļ��ֽ�����"<<h1.len<<endl;
				totalLen += (int)h1.len;
				waitSeq = (waitSeq + 1) % 65536;
				//Sleep(100);
			}
			else {	// ����ش�
				if(state == 0 && h1.type == SYN){
					h2.type = SYN_ACK;
					cout<<"�������յ��������󣬵�����������Ҫ���ش�"<<endl;
				}
				else{
					cout<<"�������кŴ�������ļ���������"<<endl;
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
	// �Զ����Ʒ�ʽд���ļ�
    ofstream fout(filename, ofstream::out | ios::binary);
	if (!fout) {
		cout << "���ܴ��ļ�" << endl;
		continue;
	}
	fout.write(data, totalLen);
	fout.close();
	}

label:closesocket(ServSocket);
      WSACleanup();
      return 0;
}