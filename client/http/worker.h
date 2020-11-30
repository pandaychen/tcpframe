#ifndef _MODULE_H_
#define _MODULE_H_

#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/epoll.h>
#include <sys/sendfile.h> 
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <stdint.h>


enum {
	UNSPECIFY_INDEX =0,
	SPECIFY_INDEX=1,
};


enum 
{
	FDARRAY_INDEX_INIT=-1,
};

enum CLIENT_VARS{
	MAX_RECV_BUF_LEN = 4096,
	MAX_SEND_BUF_LEN = 4096,
	MAX_FAIL_COUNT = 1000,

	CONNECT_TIMEOUT = 3,		//3s��ʱ?
	IP_LEN = 16,

	TIMEOUT_USEC_TIMEVAL = 200000,	//200ms
};

enum STATUS
{
	ENONE = 0,
	READABLE = 1,
	WRITEABLE = 2,
};

typedef enum{
	PROTOCOL_TCP = 1,
	PROTOCOL_UDP = 2,
}PROTOCOL;

//��д�¼��ص�����
typedef int funcProcCallback(void*);

typedef struct   _pthread_or_process_single_struct StatisticsStruct;

//////////////�ṹ�嶨��/////////////////
typedef struct
{
	int stop;
	int epfd;
	struct epoll_event* events;
}EpollLoop;


//���ڿͻ�������.һ��tcp���ӵĶ���
typedef struct
{
	int fd;
	int mask;
	/*����Ҫ����������Ԫ��,FD+MASK*/
	EpollLoop *pep_loop;	//ָ��epoll_loop
	StatisticsStruct *pst_config;

	int64_t latency;
	uint64_t startTime;		//��conn�Ŀ�ʼʱ��
	uint64_t endTime;		//��conn�Ľ���ʱ��
	uint64_t touchTime;
	unsigned recvBufLen;
	unsigned sendedLen;			//�ѷ�������
	unsigned sendBufLen;		//��Ҫ���Ͷ�������(ҪС��MAX_SEND_BUF_LEN)
	char recvBuf[MAX_RECV_BUF_LEN + 1];
	char sendBuf[MAX_SEND_BUF_LEN + 1];
	funcProcCallback* rFuncProc;
	funcProcCallback* wFuncProc;
}ClientFD;

//���ýṹ��(ÿ�����̻����̶߳�ռ�Ľṹ,����ͳ��)
struct _pthread_or_process_single_struct
{
	//�����õ�clients��Ŀ
	unsigned resetClientNum;
	//��ʱ�������client��
	unsigned timeoutClientNum;
	//ʵ����ɵ���������������ʱ�ط�
	unsigned totalRequests;
	//ʵ����Ҫ�ɹ���ɵ���������-n �������룬Ĭ����50
	unsigned needRequestNum;
	//�Ѿ���ɵ���������ÿ�γɹ�����һ��������ݰ�����+1
	unsigned doneRequests;
	//��ʼ����ʱ��
	uint64_t startTime;
	//��������ʱ��
	uint64_t endTime;
	//�Ƿ���ó�����ģʽ��-k ������ʾ���ó�����
	int keepalive;
	//��ǰ����client��(����������)
	unsigned liveClientNum;
	//ʵ�ʴ�����client����
	unsigned createClientNum;
	//��Ҫ�Ĳ��������� -c����
	unsigned needClientNum;
	//�Ƿ������������
	int randomFlag;
	unsigned short port;
	unsigned short protocol; //1 for tcp(default),2 for udp
	char szhost[IP_LEN];
	//Ϊÿ�������ʱ��¼���û����ͳ�Ʋ�ͬ��ʱ�����������
	uint64_t* latency;
	
	//���ڱ����̻����̵߳�epoll_wait--reactor
	EpollLoop* epoll_loop;

	//���ڱ����̻��߱��̵߳�client��Ŀ(�����malloc��ʽ�����,������Ҫ�ö���ָ��,һ��ָ����������ǹ̶�size��)
	ClientFD* clients_ptr;
};


//typedef  _pthread_or_process_single_struct StatisticsStruct;
typedef struct   _pthread_or_process_single_struct StatisticsStruct;


#ifdef __cplusplus
extern "C"{
#endif

	/*
	* encode success return the data len
	* encode fail return -1
	*/
	int EncodeRequest(char* data, int maxLen);

	/*
	* decode success return 0
	* decode fail return -1
	* not a complete package(tcp is stream), need wait more data return 1
	*/
	int DecodeResponse(char* data, int len);


	//socket api
	int SetNonBlock(int fd);
	int SetTcpNoDelay(int fd);

	//time
	uint64_t GetmsecTime();
	uint64_t GetusecTime();
	void TouchTime();
	int CompareLatency(const void *a, const void *b);


	//
	void IgnoreSignal();

	//Ϊ�������̴���һ��epoll�ṹ��
	EpollLoop* CreateSingleEpollLoop(int maxClients);


	void InitConfig(StatisticsStruct *t_pStStruct);
    void ParseOptions(int argc, char **argv,StatisticsStruct *pConfigSt);
	int InitSingleReactor(StatisticsStruct *pConfigSt);

	void ReactorBenchmarkLoop(StatisticsStruct *t_pstConfigSt);
	int ReactorClientInit(StatisticsStruct *t_pStConfig,int iFlag);
	int ReactorAddEvent(ClientFD * pclient, int mask, funcProcCallback * func);
	int ReactorDelEvent(ClientFD* pclient, int t_imask);
	void ReactorCloseConnection(ClientFD * c);
    void ReactorResetClient(ClientFD* pclient);
	void ReactorCloseConnectionNormal(ClientFD* pclient);

    void ClearTimeoutConnection(StatisticsStruct* t_pstConfig, uint64_t curTime);

	//
	int SendToServerCallback(void* pcb_arg);
	int RecvFromServerCallback(void* pcb_arg);

	void ShowThroughPut(StatisticsStruct *pstConfig);
	void ShowReport(StatisticsStruct *pstConfig);

#ifdef __cplusplus
}
#endif


#endif
