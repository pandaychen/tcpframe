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

	CONNECT_TIMEOUT = 3,		//3s超时?
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

//读写事件回调函数
typedef int funcProcCallback(void*);

typedef struct   _pthread_or_process_single_struct StatisticsStruct;

//////////////结构体定义/////////////////
typedef struct
{
	int stop;
	int epfd;
	struct epoll_event* events;
}EpollLoop;


//对于客户端来言.一个tcp连接的定义
typedef struct
{
	int fd;
	int mask;
	/*必须要包裹的两个元素,FD+MASK*/
	EpollLoop *pep_loop;	//指向本epoll_loop
	StatisticsStruct *pst_config;

	int64_t latency;
	uint64_t startTime;		//本conn的开始时间
	uint64_t endTime;		//本conn的结束时间
	uint64_t touchTime;
	unsigned recvBufLen;
	unsigned sendedLen;			//已发送数据
	unsigned sendBufLen;		//需要发送多少数据(要小于MAX_SEND_BUF_LEN)
	char recvBuf[MAX_RECV_BUF_LEN + 1];
	char sendBuf[MAX_SEND_BUF_LEN + 1];
	funcProcCallback* rFuncProc;
	funcProcCallback* wFuncProc;
}ClientFD;

//配置结构体(每个进程或者线程独占的结构,用于统计)
struct _pthread_or_process_single_struct
{
	//被重置的clients数目
	unsigned resetClientNum;
	//超时被清除的client数
	unsigned timeoutClientNum;
	//实际完成的请求数，包括超时重发
	unsigned totalRequests;
	//实际需要成功完成的请求数，-n 参数传入，默认是50
	unsigned needRequestNum;
	//已经完成的请求数，每次成功接受一个完成数据包，会+1
	unsigned doneRequests;
	//开始测试时间
	uint64_t startTime;
	//结束测试时间
	uint64_t endTime;
	//是否采用长连接模式，-k 参数表示采用长连接
	int keepalive;
	//当前存活的client数(并发连接数)
	unsigned liveClientNum;
	//实际创建的client次数
	unsigned createClientNum;
	//需要的并发连接数 -c参数
	unsigned needClientNum;
	//是否随机发送内容
	int randomFlag;
	unsigned short port;
	unsigned short protocol; //1 for tcp(default),2 for udp
	char szhost[IP_LEN];
	//为每个请求耗时记录，用户最后统计不同耗时区间的请求数
	uint64_t* latency;
	
	//属于本进程或者线程的epoll_wait--reactor
	EpollLoop* epoll_loop;

	//属于本进程或者本线程的client数目(如果是malloc形式加入的,这里需要用二级指针,一级指针表明这里是固定size的)
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

	//为单个进程创建一个epoll结构体
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
