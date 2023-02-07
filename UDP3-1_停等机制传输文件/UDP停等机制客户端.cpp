#include <winsock2.h>
#include <iostream>
#include<string>
#include<fstream>
#include<time.h>
using namespace  std;
#pragma comment(lib, "ws2_32.lib")
struct UDP_Head {
	unsigned short type;		// ��Ϣ����,SYN�������ӣ�ACKȷ�ϣ�PSH��Ϣ��FIN�Ͽ����ӣ�END�ļ��������
	unsigned short len;			// ���ݳ��ȣ�16λ
	unsigned short checkSum;    // У��ͣ�16λ
	unsigned short seq;		    // ���кţ�16λ

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
#define maxSize 10240 //�����ļ��������Ϊ10KB
#define maxTime 1000
unsigned short sendSeq = 0; //��ʼ�����к�
int state = 0; //0��ʾ��û�н������ӣ�1��ʾ���ӳɹ����Դ����ļ���2��ʾ�ļ�������϶Ͽ�����
// У��ͣ�ÿ16λ��Ӻ�ȡ��
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
    SOCKET ClientSocket;
    SOCKADDR_IN ServAddress; //Զ�̵�ַ
    int ServAddressLen;

    // socket������ʼ��
    WSADATA  wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cout << "WSA error:" << GetLastError() << endl;
        return false;
    }
    // ����socket����
    ClientSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (ClientSocket == INVALID_SOCKET)
    {
        closesocket(ClientSocket);
        ClientSocket = INVALID_SOCKET;
        return false;
    }
	//�����׽���Ϊ������ģʽ 
	int iMode = 1; //1����������0������ 
	ioctlsocket(ClientSocket, FIONBIO, (u_long FAR*) & iMode);//���������� 

    // ����˵�ַ
    ServAddress.sin_family = AF_INET;
    ServAddress.sin_port = htons(10009);
	ServAddress.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
    ServAddressLen = sizeof(ServAddress);

Label: while(1){
		int sentLen = 0; //ԭʼ�ļ��Ѿ�����ȥ�ĳ���
		char *data = NULL; //Ҫ��������
		int length = 0; //Ҫ�����ݵĴ�С���ֽڣ�
		char *s = new char[20];
		cout<<"������Ҫ������ļ�����";
		cin>>s;
		if(strcmp(s,"q") == 0){
			state = 2;
		}
		else{
			ifstream fin(s, ifstream::in | ios::binary);
			if (!fin) {
				cout << "���ܴ��ļ�" << endl;
				continue;
			}
			fin.seekg(0, fin.end);		// ָ�붨λ���ļ�β
			length = fin.tellg();	// ��ȡ�ļ���С���ֽڣ�
			fin.seekg(0, fin.beg);		// ָ�붨λ���ļ�ͷ
			data = new char[length];
			memset(data, 0, sizeof(data));
			fin.read(data, length);
			fin.close();
		}

		clock_t start;
	
 
        while (1) {
		// ������Ϣͷ
        UDP_Head h;
		h.UDP_init();
        h.seq = sendSeq;
		char *sendBuf;
		if(state == 0){ //��������
			h.type = SYN;  //���ӱ�־
			h.len = 0;
			sendBuf = new char[1024];
			memset(sendBuf, 0, 1024);
			memcpy(sendBuf, &h, sizeof(h));
		}
		else if(state == 2){//׼���Ͽ�����
			h.type = FIN;
			h.len = 0;
			sendBuf = new char[1024];
			memset(sendBuf, 0, 1024);
			memcpy(sendBuf, &h, sizeof(h));
		}
		else{ //state =1 ������Ϣ
			h.type = PSH;
			if(sentLen == 0) cout<<"------------------��ʼ�����ļ�!------------------"<<endl;
			if((length - sentLen) > maxSize){ //�ļ��ⲿ��̫����Ҫ�ֶη���
				h.len = maxSize;
			}
			else if((length - sentLen) <= 0){  //�ļ��������
				h.type = END;
				h.len = strlen(s);
			}
			else
				h.len =length - sentLen;  //�ļ��ⲿ���ڹ涨��С��
			sendBuf = new char[h.len+sizeof(h)];
			memset(sendBuf, 0, h.len+sizeof(h));
			memcpy(sendBuf, &h, sizeof(h));
			// data��Ҫ�������Ϣ��s���ļ�����sentLen���ѷ��͵ĳ��ȣ���Ϊ�����η��͵�ƫ����
			if((length - sentLen) <= 0)
				memcpy(sendBuf + sizeof(h), s, h.len);
			else
				memcpy(sendBuf + sizeof(h), data + sentLen, h.len);
			sentLen += (int)h.len;
		}
		// ����У���
		h.checkSum = checkSumVerify(sendBuf, sizeof(h) + h.len);
		memcpy(sendBuf, &h, sizeof(h));
 
		//��ʼ��ʱ	
        start = clock();
		// ������Ϣ
		int len = sendto(ClientSocket, sendBuf, sizeof(h) + h.len, 0, (SOCKADDR*)&ServAddress, sizeof(SOCKADDR));
		cout<<"�����ļ����ֽ�����"<<h.len<<endl;
		if (len == -1) {
			cout << "������Ϣʧ��" << endl;
			return false;
		}
		else{
			if(state == 0){
				cout << "���ͻ��˷������ӡ�: [SYN] Seq=0" << endl;
			}
			if(state == 2){
				cout << "���ͻ��˷��ͶϿ����ӡ�: [FIN] Seq="<<h.seq << endl;
			}
			Sleep(100);
		}
		// �ȴ�������Ϣ
		char *recvBuf = new char[1024];
	    memset(recvBuf, 0, 1024);
		int sendcount = 0; //�ش��ļ��Ĵ���

		while (1){
			int checklen = recvfrom(ClientSocket, recvBuf, 1024, 0, (SOCKADDR*)&ServAddress, &ServAddressLen);
			if (checklen > 0 ) { //���յ�����Ϣ
				UDP_Head h2;
				h2.UDP_init();
				memcpy(&h2, recvBuf, sizeof(h2));
				// ��֤��Ϣ�����кź�У���
				cout<<"�������к�:"<<h2.seq<<endl;
				int check = checkSumVerify(recvBuf,sizeof(h2));
				if (h2.seq == sendSeq && check == 0) {
					if(state == 0 && h2.type == SYN_ACK){ //���ͼ�飬��������ȷ��
						cout << "��������Ѿ��������ӡ�: [SYN, ACK] Seq=0 Ack=0" << endl;
						cout<<"������ʱ��"<<(clock() - start) * 1000.0 / CLOCKS_PER_SEC << "ms" << endl;
						state = 1;
					}
					else if(state ==2 && h2.type == FIN_ACK){ //���ͼ�飬�Ͽ�����ȷ��
						cout << "��������Ѿ��Ͽ����ӡ�: [FIN, ACK] Ack="<<h2.seq << endl;
						return 0;
					}
					else if(h2.type == END_ACK){
						cout<<"------------------�ļ��������!------------------"<<endl;
						goto Label;
					}
					else{ //������Ϣ
						cout<<"�������Ѿ����յ���Ϣ�����ͽ�����Ϣ��ʱ��"<<(clock() - start)* 1000.0 / CLOCKS_PER_SEC<<"ms"<<endl;
					}
					sendSeq = (sendSeq + 1) % 65536;
					break;
			    }
				else{  //���������кŴ������У��ʹ���ʲôҲ����
					cout<<"�������кŻ���У�������"<<endl;
				}
			}
			double time = (clock() - start)* 1000.0 / CLOCKS_PER_SEC;
		    if(time > maxTime){   // ��ʱ�ش������¼�ʱ
				sendcount++;
				if(sendcount > 5){
					break; //����ش���������5�����������ݰ�
				}
				cout<<"������Ϣ��ʱ����ʱ"<<time<<"ms"<<endl;
			    // ���¼�ʱ
				start = clock();
				// ���·�������
				cout<<"���·�������"<<endl;
				if (sendto(ClientSocket, sendBuf, sizeof(h) + h.len, 0, (SOCKADDR*)&ServAddress,sizeof(SOCKADDR)) == -1) {
			          cout << "������Ϣʧ��" << endl;
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
