#include <winsock2.h>
#include <iostream>
#include<string>
#include<fstream>
#include<time.h>
#include<queue>
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
struct buf{  //buf�ṹ�����ڽ����������ݴ浽������
	bool isACK;
	char *sendbuf;
	int len;
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
#define maxTime  1000 //���ó�ʱ�ش���ʱ��Ϊ1s
#define maxwindow 20 //����ͨ����������ƴ������Ϊ20

unsigned short nextseqnum = 1; //���ͻ��������أ�ÿ�γɹ�����һ����Ϣ��+1
unsigned short base = 1; //���ͻ�����ǰ��,ÿ�γɹ����յ�ACK��+1
int state = 0; //�ͻ��˵�״̬��0��ʾ��û�н������ӣ�1��ʾ���ӳɹ����Դ����ļ���2��ʾ�ļ�������϶Ͽ�����,3��ʾһ���ļ���������
//��״̬����3��Ҫ����Ϊ��������Ƶ���������ļ�����Ϊ�˱�֤�ļ�������֮���ڵȴ�����ACK��Ϣ��ʱ�򲻻��ظ������ļ���������һ�η����ļ�ʱ������1��
queue<buf> sndpkt; //���ö��б�ʾ������

float cwnd = 1.0; //TCPӵ�����ƴ���
int ssthresh = 16; //��ֵ
int dupACKcount = 0; //�ظ�ACK�ĸ���
int status = 1; //״̬��1��ʾ�������׶Σ�2��ʾӵ������׶Σ�3��ʾ���ٻָ��׶�

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

    // ·������ַ
    ServAddress.sin_family = AF_INET;
    ServAddress.sin_port = htons(10009);
	ServAddress.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
    ServAddressLen = sizeof(ServAddress);

	clock_t start; //��ʱ����ֻ�ǿ�ʼʱ��

 Label:while(1){
		if(state == 3)  //״̬3��ʾ�շ�����һ���ļ���Ҫ������һ���ļ��ˣ�״̬��1
			state = 1;
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
		char *recvBuf = new char[1024]; //���ջ�����
	    memset(recvBuf, 0, 1024);
		int sendcount = 0; //�ش���Ϣ�Ĵ���������ܳ���5
		char *sendBuf; //���ͻ��������ڷ���ʱ����

        while (1) {
			//������Ϣ
			if(nextseqnum < base + cwnd && state!=3){ //״̬Ϊ3ʱ���ܷ�����Ϣ�������Խ�����Ϣ
				UDP_Head h; // ������Ϣ��ͷ
				h.UDP_init();
				h.seq = nextseqnum;
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
						state = 3;
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
					if(base == nextseqnum) //�������ڴ�С��ʱΪ0�������Ժ�ʼ��ʱ	
						start = clock();
					Sleep(100);
					nextseqnum = (nextseqnum + 1) % 65536; //���Ͷ˿ں���+1
					buf b1;
					b1.isACK = false; //��ʾ��û�н��չ�ȷ����Ϣ
					b1.sendbuf = sendBuf;
					b1.len = h.len + sizeof(h);
					sndpkt.push(b1); //�����͵���Ϣ������У��Ա�֮��ʱ�ش�
					cout<<"���ʹ��ڴ�С��"<<nextseqnum - base<<endl;
				}
			}


			// ������Ϣ
			int checklen = recvfrom(ClientSocket, recvBuf, 1024, 0, (SOCKADDR*)&ServAddress, &ServAddressLen);
			if (checklen > 0 ) { //���յ�����Ϣ
				UDP_Head h2;
				h2.UDP_init();
				memcpy(&h2, recvBuf, sizeof(h2));
				// ��֤��Ϣ�����кź�У���
				cout<<"�������к�:"<<h2.seq<<endl;
				int check = checkSumVerify(recvBuf,sizeof(h2));

				//���յ���У��ͺ����к�����,ACK���к��ڻ���������
				if ( h2.seq >= base && h2.seq < nextseqnum && check == 0 ) { 

					/*
					ӵ�����ƣ�����״̬�ֱ�����
					*/
					if(status == 1){ //������״̬��ÿ�յ�һ��ACK��cwnd��1
						cwnd = cwnd + 1;
						dupACKcount = 0;
						if(cwnd >=ssthresh){
							status = 2;
						}
					}
					else if(status == 2){ //ӵ������״̬��ÿ��RTT��cwnd��1
						cwnd = cwnd + 1.0 / cwnd ;
						dupACKcount = 0;
					}
					else{
						//���ٻָ�״̬:new reno�㷨����ʵ�ʴ��ڴ�СΪ0ʱ�����з�������Ϣ����ȷ���ˣ���
				        //�ŵ�ӵ������״̬������ά�ֿ��ٻָ�״̬
						dupACKcount = 0;
						if(base == nextseqnum){
							cwnd = ssthresh;
							status = 2;
						}else{
							cwnd = cwnd + 1;
						}
					}
					if(cwnd > maxwindow){ //ӵ�������������ͨ����������ڣ���ôȡ��Сֵ
						cwnd = maxwindow;
					}

					/*
					ACK��ȷʱ�������������1�����ڴ����ڵ����кŲ���base 2�����к�Ϊbase
					*/
					if(h2.seq != base){ //���ڴ����ڵ����кŲ���baseʱ��ѭ�������б���һ�飬���Ǹ��յ�����Ϣ���
						for(unsigned short i = base; i < nextseqnum; i++){
							if(i == h2.seq){
								sndpkt.front().isACK = true; //����Ѿ�����ACK��Ϣ��

							}
						    sndpkt.push(sndpkt.front());
							sndpkt.pop();
						}
					}
					else{
						unsigned short temp = base;
						for(unsigned short i = temp; i < nextseqnum; i++){ //�ڶ�����ѭ���������������Ѿ������յĽ�����Ϣ����
							if(i != temp && sndpkt.front().isACK == false) break;  //������һ��û�б����յ���Ϣ���˳�ѭ��
							sendcount = 0; //�ش���������
							if(state == 0 && h2.type == SYN_ACK){ //���ͼ�飬��������ȷ��
								cout << "��������Ѿ��������ӡ�: [SYN, ACK] Seq=0 Ack=0" << endl;
								state = 1;
							}
							else if(state ==2 && h2.type == FIN_ACK){ //���ͼ�飬�Ͽ�����ȷ��
								cout << "��������Ѿ��Ͽ����ӡ�: [FIN, ACK] Ack="<<h2.seq << endl;
								return 0;
							}
							else if(h2.type == END_ACK){
								cout<<"------------------�ļ��������!------------------"<<endl;
								sndpkt.pop();
								base = (base + 1) % 65536;  //���Ͷ˿�ǰ��+1
								if(base != nextseqnum) //���¼�ʱ��
									start = clock();	
								cout<<"���ʹ��ڴ�С��"<<nextseqnum - base<<endl;
								goto Label;
							}
							else{ //������Ϣ
								cout<<"�������Ѿ����յ���Ϣ���ӷ��͵�������Ϣ��ʱ��"<<(clock() - start)* 1000.0 / CLOCKS_PER_SEC<<"ms"<<endl;
							}
							sndpkt.pop();
							base = (base + 1) % 65536;  //���Ͷ˿�ǰ��+1
							if(base != nextseqnum) //���¼�ʱ��
								start = clock();	
							cout<<"���ʹ��ڴ�С��"<<nextseqnum - base<<"    cwnd��"<<cwnd<<"    ssthresh��"<<ssthresh<<endl;
						}
					}
			    }

				else{  //���������кŴ������У��ʹ���ʲôҲ����
					cout<<"�������кŻ���У�������"<<endl;
					if(check == 0){

					/*
					checkΪ0˵��У�������ֻ���յ��ظ�ACK��Ҫ����ӵ������
					*/
						if(status == 1 || status == 2){ //������״̬��ӵ������״̬
							dupACKcount++;
							if(dupACKcount == 3){
								ssthresh = cwnd / 2;
								cwnd = ssthresh + 3;
								status = 3;
							}
						}
						else{ //���ٻָ�״̬
							cwnd++;
						}
						if(cwnd > maxwindow){ //ӵ�������������ͨ����������ڣ���ôȡ��Сֵ
							cwnd = maxwindow;
					    }
						cout<<"���ʹ��ڴ�С��"<<nextseqnum - base<<"    cwnd��"<<cwnd<<"    ssthresh��"<<ssthresh<<endl;
					}
				}
			}

			// ��ʱ�ش������¼�ʱ
			double time = (clock() - start)* 1000.0 / CLOCKS_PER_SEC;
		    if(time > maxTime){  
				int queueSize = sndpkt.size(); //Ҫ�ط�����Ϣ������ֻ�л���������������0ʱ�����ط���Ϣ�����¼�ʱ
				cout<<"�ط���Ϣ��������"<<queueSize<<endl;
				if(queueSize > 0){
					sendcount++;
					if(sendcount > 5){
						break; //����ش���������5�����������ݰ�
					}
					cout<<"������Ϣ��ʱ����ʱ"<<time<<"ms"<<endl;
					// ���¼�ʱ
					start = clock();
					// ���·�������
					cout<<"���·�������"<<endl;
					while(queueSize > 0){
					    buf b2 = sndpkt.front();
						int len = b2.len;  //Ҫ�ط���Ϣ�ĳ���
						char *temp = b2.sendbuf;
						UDP_Head h;
						h.UDP_init();
						memcpy(&h, temp, sizeof(h));
						cout<<"�ط���Ϣ�����кţ�"<<h.seq<<endl;
						if (sendto(ClientSocket, temp, len, 0, (SOCKADDR*)&ServAddress,sizeof(SOCKADDR)) == -1) {
							cout << "������Ϣʧ��" << endl;
							return false;
						}
						sndpkt.push(sndpkt.front());
						sndpkt.pop();
						queueSize --;
						//Sleep(100);
					}

					/*
					����ӵ������
					*/
					ssthresh = cwnd / 2;
					cwnd = 1.0;
					dupACKcount = 0;
					status = 1; //�����������׶�
				}
			}
		}
	}
    closesocket(ClientSocket);
    WSACleanup();
    return 0;
}
