#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <assert.h>

#include "worker.h"


extern struct option longopts[];
/*
在功能文件.c中引用main.c定义的全局变量,必须要用extern来声明,否则在初始化会报错
main.cpp:11:18: error: uninitialized const member in ‘StatisticsStruct {aka struct _pthread_or_process_single_struct}’ [-fpermissive]
*/
extern StatisticsStruct config;

static char buf[4096];
char *ptr = NULL;

static struct timeval _tv = { 0, 0 };

///////////////////////CAL TIME FUNCTION/////////////////////////////////
uint64_t GetmsecTime()
{
	uint64_t msec;

	if (_tv.tv_sec == 0)
		gettimeofday(&_tv, NULL);

	msec = ((long)_tv.tv_sec * 1000LL);
	msec += _tv.tv_usec / 1000;

	return msec;
}

/*
	得到当前static timeval变量中的usec值
*/
uint64_t GetusecTime()
{
	uint64_t usec;

	if (_tv.tv_sec == 0)
		gettimeofday(&_tv, NULL);

	usec = ((uint64_t)_tv.tv_sec) * 1000000LL;
	usec += _tv.tv_usec;

	return usec;
}

/*
	更新当前的时间
*/
void TouchTime()
{
	gettimeofday(&_tv, NULL);
}


//
int CompareLatency(const void *a, const void *b)
{
	return (*(uint64_t*)a) - (*(uint64_t*)b);
}

///////////////////////END OF CAL TIME FUNCTION/////////////////////////////////


int EncodeRequest(char* data, int maxLen)
{
    //只做一次初始化
    if (ptr == NULL)
    {
        sprintf(buf, "%s%s%s%s%s%s%s%s%s%s",
                "GET / HTTP/1.1\r\n",
                "Host: xx.com\r\n",
                "Connection: keep-alive\r\n",
                "User-Agent: Mozilla/5.0 (Windows NT 5.1) AppleWebKit/535.11 (KHTML, like Gecko) Chrome/17.0.963.56 Safari/535.11\r\n",
                "Referer: http://sbdji.com/\r\n",
                "Accept: */*\r\n",
                "Accept-Encoding: gzip,deflate,sdch\r\n",
                "Accept-Language: zh-CN,zh;q=0.8,en;q=0.6\r\n",
                "Accept-Charset: gb18030,utf-8;q=0.7,*;q=0.3\r\n",
                "\r\n");
        ptr = buf;
    }

    int len = strlen(ptr);
       
    if (len > maxLen) 
        return -1;

    memcpy(data, ptr, len);

	return len;
}

int DecodeResponse(char* data, int len)
{
	return 0;
}

//
void IgnoreSignal()
{
	signal(SIGTSTP, SIG_IGN);
	signal(SIGHUP, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGTTOU, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);
	signal(SIGSTOP, SIG_IGN);
	signal(SIGTERM, SIG_IGN);
	signal(SIGINT, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);
}


//socket api
int SetNonBlock(int fd)
{
	int flag;

	if ((flag = fcntl(fd, F_GETFL, 0)) == -1)
		return -1;

	flag |= O_NONBLOCK;

	if (fcntl(fd, F_SETFL, flag) == -1)
		return -1;

	return 0;

}

int SetTcpNoDelay(int fd)
{
	int yes = 1;

	if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes)) == -1)
		return -1;

	return 0;
}



////////////////////////////////CONFIG////////////////////////////////

void InitConfig(StatisticsStruct *t_pStStruct)
{
	assert(t_pStStruct != NULL);

	t_pStStruct->createClientNum = 0;		//实际创建成功(connect成功+addevent成功)的client数
	t_pStStruct->timeoutClientNum = 0;		//超时被清除的client数

	t_pStStruct->totalRequests = 0;			//实际完成的请求数，包括超时重发
	t_pStStruct->needRequestNum = 50;		//实际需要成功完成的请求数，-n 参数传入，默认是50

	t_pStStruct->doneRequests = 0;				//已经完成的请求数，每次成功接受一个完成数据包，会+1

	t_pStStruct->resetClientNum = 0;			//被重置的clients数目

	t_pStStruct->port = 5113;
	strncpy(t_pStStruct->szhost, "127.0.0.1", IP_LEN);
	t_pStStruct->protocol = PROTOCOL_TCP;
	
	//开始测试时间
	t_pStStruct->startTime = 0;

	//结束测试时间
	t_pStStruct->endTime = 0;

	//是否采用长连接模式，-k
	t_pStStruct->keepalive = 0;

	//当前存活的client数(并发连接数)
	t_pStStruct->liveClientNum = 0;

	//需要的并发连接数 -c参数
	t_pStStruct->needClientNum = 5;

	//是否随机发送内容
	t_pStStruct->randomFlag = 0;

	//为每个请求耗时记录，用户最后统计不同耗时区间的请求数
	t_pStStruct->latency = NULL;
	t_pStStruct->epoll_loop = NULL;
	t_pStStruct->clients_ptr = NULL;
}


void PrintHelp(){
	printf("Usage: benchmark [-h <host>] [-p <port>] [-c <clients>] [-n <requests]> [-k] [-r] [-u]");
	printf("\n");
	printf(" -h <host> Server ip (default 127.0.0.1)");
	printf("\n");
	printf(" -p <port> Server port (default 5113)");
	printf("\n");
	printf(" -c <clients> Number of parallel connections (default 5)");
	printf("\n");
	printf(" -n <requests> Total number of requests (default 50)");
	printf("\n");
	printf(" -k keep alive or reconnect (default is reconnect)");
	printf("\n");
	printf(" -r re-encode sendbuf per request (default is no encode)");
	printf("\n");
	printf(" -u benchmark with udp protocol(default is tcp)");
	printf("\n");
	exit(1);
}

/*
	解析参数，结果存入每个进程/线程的配置结构中
*/
void ParseOptions(int argc, char **argv,StatisticsStruct *pConfigSt) {
	assert(argc > 0);
	assert(NULL != argv);
	assert(NULL != pConfigSt);


	int opt = 0;
	while ((opt = getopt_long(argc, argv, ":c:n:h:p:kru", longopts, NULL)) != -1)
	{
		switch (opt) {
		case 'c':
			pConfigSt->needClientNum = atoi(optarg);		//需要客户端的伪并发数(注意不是线程数),其实哪里是并发(不是多线程/多进程).....
			break;
		case 'n':
			pConfigSt->needRequestNum = atoi(optarg);		//需要成功完成请求数
			break;
		case 'h':
			strncpy(pConfigSt->szhost, optarg, IP_LEN);		//
			break;
		case 'p':
			pConfigSt->port = atoi(optarg);
			break;
		case 'k':
			pConfigSt->keepalive = 1;
			break;
		case 'u':
			pConfigSt->protocol = PROTOCOL_UDP;
			break;
		case 'r':
			pConfigSt->randomFlag = 1;
			break;
		default:
			PrintHelp();
		}
	}
    printf("check parameter...,%s,%d\n",pConfigSt->szhost, inet_addr(pConfigSt->szhost));
	//check parameter
	if (INADDR_NONE == inet_addr(pConfigSt->szhost)){
		//这个也可能会有问题
		printf("server host illegal\n");
		PrintHelp();
	}

	if (pConfigSt->port<=0 || pConfigSt->port>65535){
		printf("server port illegal\n");
		PrintHelp();
	}

	//have a connect test
    int ret=TestConnect(pConfigSt);
    printf("ret=%d\n",ret);
	if (TestConnect(pConfigSt) < 0){

		printf("test connect to %s:%d error\n",pConfigSt->szhost,pConfigSt->port);
		exit(1);
	}

	return;
}

//创建一个connect-test-socket
int TestConnect(StatisticsStruct *t_pStConfig){
	assert(NULL != t_pStConfig);
    int fd=-1;
	if (t_pStConfig->protocol == PROTOCOL_UDP){
		fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	}
	else{
		//TCP
		fd = socket(AF_INET, SOCK_STREAM, 0);
	}

	if (fd == -1){
		return -1;
	}

	//set noblock
	if (SetNonBlock(fd) < 0){
		close(fd);
		return -1;
	}

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(t_pStConfig->port);
	addr.sin_addr.s_addr = inet_addr(t_pStConfig->szhost);

	//simple connect
    int ret=connect(fd, (struct sockaddr *)&addr, sizeof(addr));
    //printf("conn=%d,%d\n",ret,errno);
	if (ret == -1)
	{
		//this is ok,need more check status
		if (errno != EINPROGRESS)
		{
			printf("connect failed...\n");
			//这里需要优化下?
			close(fd);
			return -1;
		}
	}
	
	//nginx的非阻塞connect判定方式
	int err;
	socklen_t len;

	err = 0;
	len = sizeof(int);

	/*
	* BSDs and Linux return 0 and set a pending error in err
	* Solaris returns -1 and sets errno
	*/

	if (getsockopt(fd, SOL_SOCKET, SO_ERROR, (void *)&err, &len) == -1)
	{
		err = 1;
	}

	if (err) {
		//printf("finally connect() failed\n");
		return -1;
	}

    return 0;
}

///////////END OF CONFIG //////////////////////////

/////////////////FREE JOB///////////////////////
void FreeResource(StatisticsStruct *pConfigSt){
	assert(NULL != pConfigSt);

	if (pConfigSt->clients_ptr){
		free(pConfigSt->clients_ptr);
	}
	if (pConfigSt->epoll_loop){
		free(pConfigSt->epoll_loop);
	}
	if (pConfigSt->latency){
		free(pConfigSt->latency);
	}

	return;
}

///////////////END OF FREE JOB//////////////////

//////////////////////////////REACTOR EVENTS///////////////////////
/*
	初始化本进程/线程的reactor结构体
*/
int InitSingleReactor(StatisticsStruct *pConfigSt){
	assert(pConfigSt != NULL);

	//1.创建epollwait的资源

	uint32_t uClientNum = pConfigSt->needClientNum;		//default is 5
	uint32_t uRequestNum = pConfigSt->needRequestNum;		//default is 50

	//创建fixed大小的client数组,并发数就是client数组的大小...
	pConfigSt->clients_ptr = (ClientFD *)malloc(sizeof(ClientFD)*uClientNum);
	if (pConfigSt->clients_ptr == NULL){
		return -1;
	}

	memset(pConfigSt->clients_ptr,0,sizeof(ClientFD)*uClientNum);

    int i=0;
    for(;i<uClientNum;i++){
        ClientFD *ptr=pConfigSt->clients_ptr+i;
        ptr->fd=FDARRAY_INDEX_INIT;
    }

	//需要成功完成的请求数,每个成功完成的请求都分配一个计时器
	pConfigSt->latency = (uint64_t*)malloc(sizeof(uint64_t)*uRequestNum);
	if (pConfigSt->latency == NULL){
		return -1;
	}

	memset(pConfigSt->latency, 0, sizeof(uint64_t)*uRequestNum);

	pConfigSt->epoll_loop = CreateSingleEpollLoop(pConfigSt->needClientNum + 1);
	if (pConfigSt->epoll_loop == NULL){
		return -1;
	}

	//2.创建一个worker的所有client(创建足够的client)
	while (pConfigSt->liveClientNum < pConfigSt->needClientNum)
	{
		if (ReactorClientInit(pConfigSt,UNSPECIFY_INDEX) < 0)
		{
			printf("create client faild\n");
			exit(1);
		}
	}

	return 0;
}

/*
	初始化本进程/线程的epoll结构体(EPOLL_WAIT)
*/
EpollLoop* CreateSingleEpollLoop(int t_iMaxClients)
{
	if (t_iMaxClients <= 0){
		return NULL;
	}
	EpollLoop *pLoop=NULL;
	pLoop = (EpollLoop*)malloc(sizeof(EpollLoop));
	if (!pLoop){
		return NULL;
	}

	pLoop->epfd = epoll_create(1024);
	if (pLoop->epfd < 0){
		return NULL;
	}
	pLoop->stop = 1;		//do not stop
	//pLoop->events = new epoll_event[maxClients];
	pLoop->events = (struct epoll_event *)malloc(sizeof(struct epoll_event)*t_iMaxClients);
	if (NULL == pLoop->events){
		return NULL;
	}

	return pLoop;
}

/*
	reactor之客户端初始化
	1.创建一个socket-fd(UDP/TCP)
	2.connect并且成功连接server
	3.将fd放入reactor监听,相关数据存放在clients数组中
	4.返回
*/
int ReactorClientInit(StatisticsStruct *t_pStConfig, int32_t t_specifiedindex)
{
	assert(NULL != t_pStConfig);
	int fd;
	ClientFD *pclient = NULL;

	if (t_pStConfig->protocol == PROTOCOL_UDP){
		fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	}
	else{
		//TCP
		fd = socket(AF_INET, SOCK_STREAM, 0);
	}

	if (fd == -1){
		return -1;
	}

	//set noblock
	if (SetNonBlock(fd) < 0){
		close(fd);
		return -1;
	}

	struct sockaddr_in addr;
	memset(&addr,0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(t_pStConfig->port);
	addr.sin_addr.s_addr = inet_addr(t_pStConfig->szhost);

	//simple connect
	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
	{
		//this is ok
		if (errno != EINPROGRESS)
		{
			printf("connect failed...\n");
			//这里需要优化下?
			close(fd);
			return -1;
		}
	}

	//tcp-setnodelay
	if (t_pStConfig->protocol == PROTOCOL_TCP && SetTcpNoDelay(fd) < 0){
		close(fd);
		return -1;
	}
	t_specifiedindex = UNSPECIFY_INDEX;
	if (t_specifiedindex == UNSPECIFY_INDEX){
		//初始化时,根据fd mod取一个位置
		//寻找一个下标存放此连接成功的fd,并且初始化这个位置
		pclient = t_pStConfig->clients_ptr + fd % t_pStConfig->needClientNum;
	}
	else{
		//复用之前reset掉的位置(有问题,不走这个逻辑了)
		pclient = t_pStConfig->clients_ptr + t_specifiedindex;
	}

	if (pclient->fd != FDARRAY_INDEX_INIT){
		//这里优化下,搞一个循环????
		printf("fd collison..........return \n");
		close(fd);
		return -1;
	}
	else
	{
		//初始化
        //printf("new fd=%d,pos=%d,value=%d\n",fd,fd % t_pStConfig->needClientNum,pclient->fd );
		pclient->fd = fd;
		pclient->mask = ENONE;
		pclient->pep_loop = t_pStConfig->epoll_loop;
		pclient->pst_config = t_pStConfig;
		//将要写的数据放入应用层buffer,并且在epoll的驱动下发送数据
		//不是我们通常的简单做法,直接发送数据-_- 
		pclient->sendBufLen = EncodeRequest(pclient->sendBuf, sizeof(pclient->sendBuf));
		if (pclient->sendBufLen < 0){
			//需要reset?然后再等待么?????????
			printf("CreateClient:encode request buffer fail\n");
			close(fd);
			return -1;
		}

		pclient->sendedLen = 0;		//已发送位置
		pclient->recvBufLen = 0;

		//希望写数据,当然加入写监听了,sendToServer是回调函数
		if (ReactorAddEvent(pclient, WRITEABLE, SendToServerCallback) < 0){	
			//LOG_ERROR()
			close(fd);
			return -1;
		}
		
		t_pStConfig->liveClientNum++;
		t_pStConfig->createClientNum++;
	}
	return 0;
}

/*
	向reactor中添加事件
	t_imask和func是绑定的
*/
int ReactorAddEvent(ClientFD * pclient, int t_imask, funcProcCallback * func)
{
	assert(NULL != pclient);
	assert(NULL != pclient->pep_loop);

	struct epoll_event e;
	int op = pclient->mask == ENONE ? EPOLL_CTL_ADD : EPOLL_CTL_MOD;

	e.events = 0;
	e.data.u64 = 0;
	e.data.fd = pclient->fd;	//fd加入监听
	pclient->mask |= t_imask; // Merge old events 

	//确定要加入epoll监听的事件
	if (pclient->mask & READABLE)
	{
		e.events |= EPOLLIN;

	}
	if (pclient->mask & WRITEABLE)
	{
		e.events |= EPOLLOUT;
	}

	//希望要监听的事件和回调函数
	if (t_imask & WRITEABLE)
	{
		pclient->wFuncProc = func;
	}
	else if (t_imask & READABLE){
		pclient->rFuncProc = func;
	}
	else{
		printf("unknown events\n");
		return -1;
	}

	//修改epoll的监听事件
	return epoll_ctl(pclient->pep_loop->epfd, op, pclient->fd, &e);
}

/*
	向reactor中删除事件
*/
int ReactorDelEvent(ClientFD* pclient, int t_imask)
{
	assert(NULL != pclient);

    //get rid of t_imask
	pclient->mask = pclient->mask & (~t_imask);

	struct epoll_event e;
	e.events = 0;
	e.data.u64 = 0; // avoid valgrind warning 
	e.data.fd = pclient->fd;		//

	if (pclient->mask & READABLE)
	{
		e.events |= EPOLLIN;
	}
	if (pclient->mask & WRITEABLE)
	{
		e.events |= EPOLLOUT;
	}

	int op = pclient->mask == ENONE ? EPOLL_CTL_DEL : EPOLL_CTL_MOD;

	//修改或者删除事件
	return epoll_ctl(pclient->pep_loop->epfd, op, pclient->fd, &e);
}


/*
	关闭client(单纯关闭连接)
	1.移除fd在reator上监听的事件
	2.fd位置初始化
*/
void ReactorCloseConnection(ClientFD* pclient)
{
	assert(pclient != NULL);
	assert(pclient->pep_loop != NULL);

	struct epoll_event e;
	e.events = 0;
	//移除事件监听
	epoll_ctl(pclient->pep_loop->epfd, EPOLL_CTL_DEL, pclient->fd, &e);

	close(pclient->fd);
	pclient->fd = FDARRAY_INDEX_INIT;
	//pclient->fd = 0;	WRONG!!!!

	//已存活的客户端数目-1
	pclient->pst_config->liveClientNum--;
	return;
}


/*
	正常关闭连接
*/
void ReactorCloseConnectionNormal(ClientFD* pclient)
{
	assert(NULL != pclient);
	if (pclient->pst_config->doneRequests >= pclient->pst_config->needRequestNum){
		//已经完整了所有的请求
		//epollloop exit
		//set loop exit sign!
		pclient->pep_loop->stop = 0;
		return;
	}
	
	//there is no need to re-create a udp socket
	if (pclient->pst_config->keepalive || config.protocol == PROTOCOL_UDP)
	{
		//1.长连接的case
		//2.udp的case
		//复用buffer,不close(fd)
		ReactorResetClient(pclient);
	}
	else
	{
		//1.非长连接需要重新建立一个socket
		//重启一个tcpclient
		ReactorCloseConnection(pclient);
		ReactorClientInit(pclient->pst_config, UNSPECIFY_INDEX);
	}
}

/*
	重新开启一个client
	1.长连接的场景,不close-fd,复用fd发包收包
	2.UDP
*/
void ReactorResetClient(ClientFD* pclient)
{
	assert(NULL != pclient);

	ReactorDelEvent(pclient, READABLE);
	ReactorAddEvent(pclient, WRITEABLE, SendToServerCallback);

	pclient->recvBufLen = 0;
	pclient->sendedLen = 0;

	//重新写需要发送的数据
	if (pclient->pst_config->randomFlag == 1)
	{
		pclient->sendBufLen = EncodeRequest(pclient->sendBuf, sizeof(pclient->sendBuf));
		if (pclient->sendBufLen < 0)
		{
			pclient->pst_config->liveClientNum--;
			close(pclient->fd);
			return;
		}
	}
	
	pclient->pst_config->resetClientNum++;
}



/*
	客户端的reactor循环
*/
void ReactorBenchmarkLoop(StatisticsStruct *t_pstConfigSt)
{
	assert(NULL != t_pstConfigSt);

	int epoll_fail = 0;

	t_pstConfigSt->startTime = GetmsecTime();
	uint64_t last_check_ustime = GetusecTime();	//获取初始化时间
    
    //printf("begin loop...\n");
	int epfd = t_pstConfigSt->epoll_loop->epfd;
	struct epoll_event *pEvents = (struct epoll_event *)t_pstConfigSt->epoll_loop->events;

	while (t_pstConfigSt->epoll_loop->stop)
	{
		//请求完成时t_pstConfigSt->epoll_loop->stop会被更改
		int num;
		num = epoll_wait(epfd, pEvents, t_pstConfigSt->needClientNum + 1, 2000);
		// 0<=num <=t_pstConfigSt->needClientNum 其实这里不用+1也可以的

		//update global time per loop
		TouchTime();
        int i;
        for (i = 0; i< num; i++)
		{   
            //printf("num=%d,all=%d\n",num,t_pstConfigSt->needClientNum);
			struct epoll_event *e = pEvents + i;
			/*
				这里有BUG!!!没有很好的解决冲突的问题,比如t_pstConfigSt->needClientNum是50,fd是100和200,就冲突了
				所以我感觉比较好的有两种解决办法:
				1.像conn框架直接用fd做下标,不mod,fd超过max_array_size即报错
				2.直接用指针代替fd,在epoll_ctl中
				3.解决这里的bug,就是在新建的时候,判断新的fd%modsize得到位置的fd状态是不是-1,如果不是-1就返回,直到找到一个fd==-1的位置,当然这样的效率很差
				4.另外,这个t_pstConfigSt->needClientNum最好改成素数比较好
				5.如果按照我的优化方法,最大问题在于e->data.fd % t_pstConfigSt->needClientNum取到的位置,很可能不是触发的fd
				6.conn的模型是没有问题的,一个是直接用fd做下标,二是没有mod,三是array的最大值,fd超过报错返回
			*/
			ClientFD* pclient = t_pstConfigSt->clients_ptr + e->data.fd % t_pstConfigSt->needClientNum;

			if (e->events & (EPOLLERR | EPOLLHUP))
			{
				if (++epoll_fail > MAX_FAIL_COUNT)
				{
					printf("the epoll fail count is more than %d\n" , MAX_FAIL_COUNT);
					printf("the network is so poor,check it and try again.");
					exit(0);
				}
				ReactorCloseConnection(pclient);
				ReactorClientInit(t_pstConfigSt,UNSPECIFY_INDEX);
				continue;
			}
			if (e->events & EPOLLIN)
			{
				//调用触发事件的回调函数
				pclient->rFuncProc(pclient);
			}
			if (e->events & EPOLLOUT)
			{
				pclient->wFuncProc(pclient);
			}
		}

		
		//check timeout connection(check per 200ms => 200000us)
		//检查是否有超时事件,有则处理
		//200ms触发一次超时检测,3s超时时间
		if ((GetusecTime() - last_check_ustime) > TIMEOUT_USEC_TIMEVAL)
		{
			ClearTimeoutConnection(t_pstConfigSt, GetusecTime());
			//补充足够的client
			while (t_pstConfigSt->liveClientNum < t_pstConfigSt->needClientNum)
			{
				if (ReactorClientInit(t_pstConfigSt,UNSPECIFY_INDEX) < 0)
				{
					printf("create client faild\n");
					exit(1);
				}
			}

			//Non-essential, and it also effect performance
			ShowThroughPut(t_pstConfigSt);

			last_check_ustime = GetusecTime();
		}
	}

	t_pstConfigSt->endTime = GetmsecTime();
	ShowReport(t_pstConfigSt);
}

/*
	超时检测函数
*/
void ClearTimeoutConnection(StatisticsStruct* t_pstConfig, uint64_t curTime)
{
	assert(NULL != t_pstConfig);
	unsigned int i;
	uint64_t timeout = CONNECT_TIMEOUT * 1000000;
	//遍历每个client,检查是否超时
	for (i = 0; i< t_pstConfig->needClientNum; i++)
	{
		ClientFD* pclient = t_pstConfig->clients_ptr+i;

		if (pclient->fd > 0 && \
			(pclient->touchTime + timeout) <= curTime	/*为了避免减法的有符号问题,使用加法代替减法*/	\
			)
		{
			t_pstConfig->timeoutClientNum++;
			//重新这个client,这里不close
			ReactorResetClient(pclient);
		}
	}
}

///////////////END OF REACTOR LOOP	////////////////

///////////////////////FUNCTION CALLBACK//////////////////////////////////
/*
	客户端向server发送请求回调函数
*/
int SendToServerCallback(void* pcb_arg)
{
	//学习了,这个pcb_arg的参数不会在任何SendToServerCallback的关键字中出现,只会在调用中传入!!!
	//比如c->wFuncProc(c); 
	//这个c就是SendToServerCallback的函数参数
	assert(NULL != pcb_arg);
    
	ClientFD* pclient = (ClientFD*)pcb_arg;

	//first send
	if (pclient->sendedLen == 0)
	{
		//记录开始时间
		pclient->startTime = GetusecTime();
		pclient->latency = -1;
	}
 
	//update time,不一定是第一次
	pclient->touchTime = GetusecTime();

	int sended = 0;	//记录本次触发发送的字节数
	while (1)
	{
		sended = write(pclient->fd, pclient->sendBuf + pclient->sendedLen,\
			pclient->sendBufLen - pclient->sendedLen/*还剩的发送字节数*/	);
		if (sended <= 0)
		{
			if (errno == EINTR)
			{
				//可恢复打断
				continue;
			}
			else if (errno == EWOULDBLOCK || errno == EAGAIN)
			{
				//可能内核发送缓冲区已经满了,下次再发
				return 1;
			}
			else
			{
				//出现了严重错误,可能要关闭这个连接了
				ReactorCloseConnection(pclient);
				ReactorClientInit(pclient->pst_config, UNSPECIFY_INDEX);
				return -1;
			}
		}
		//>0 这里不一定要退出吧,可以继续发送,但是为了公平起见,比如某个连
		//接需要发送的数据过多,先跳出这次循环,等待下次epoll触发的时候再发送
		break;
	}

	////
	pclient->pst_config->totalRequests++;

	pclient->sendedLen += sended;	//更新已经发送的大小
	if (pclient->sendedLen == pclient->sendBufLen)
	{
		//发送完了一个请求,需要移除写事件的监听,否则epoll会不停通知
		ReactorDelEvent(pclient, WRITEABLE);
		//移除写事件,加入读事件,接收服务器的response
		ReactorAddEvent(pclient, READABLE, RecvFromServerCallback);
	}

	return 0;
}

/*
	客户端向server接收响应回调函数
*/
int RecvFromServerCallback(void* pcb_arg)
{	
	assert(NULL != pcb_arg);

	ClientFD* pclient = (ClientFD*)pcb_arg;

	//update touch time
	pclient->touchTime = GetusecTime();

	// Calculate latency only for the first read event. This means that the
	// server already sent the reply and we need to parse it. Parsing overhead
	//is not part of the latency, so calculate it only once, here. 
	if (pclient->latency < 0){
		pclient->latency = GetusecTime() - pclient->startTime;
	}

	//Calculate from the first response 
	//if (config.startTime == 0)
	//	config.startTime = msTime();
	int received = 0;
	while (1)
	{
		received = read(pclient->fd, pclient->recvBuf + pclient->recvBufLen, sizeof(pclient->recvBuf)/*4096*/ - pclient->recvBufLen);
		if (received < 0)
		{
			if (errno == EINTR)
			{
				continue;
			}
			else if (errno == EWOULDBLOCK || errno == EAGAIN)
			{
				return 1;
			}
			else
			{
				ReactorCloseConnection(pclient);
				ReactorClientInit(pclient->pst_config, UNSPECIFY_INDEX);
				return -1;
			}
		}
		else if (received == 0)
		{
			//peer closed
			ReactorCloseConnection(pclient);
			ReactorClientInit(pclient->pst_config, UNSPECIFY_INDEX);
			return -1;
		}
		else
		{
			pclient->recvBufLen += received;
			break;
		}
	}
	
	//sizeof(c->recvBuf) = MAX_RECV_BUF_LEN + 1
	if (pclient->recvBufLen > MAX_RECV_BUF_LEN) {
		printf("client recv buffer is full, Please make sure that is enough\n");
		exit(-1);
	}

	//encode request package
	//解析响应报文
	int ret = DecodeResponse(pclient->recvBuf, pclient->recvBufLen);
	if (ret < 0)
	{
        ReactorCloseConnection(pclient);
		ReactorClientInit(pclient->pst_config, UNSPECIFY_INDEX);
		return -1;
	}
	else if (ret == 1)
	{
		//it is not a complete packet, wait it
		return 0;
	}

	//已接受一次完整的response,统计数据
	//将这次请求的时延放入时延统计数组
	uint32_t ucurrequests = pclient->pst_config->doneRequests;
	if (pclient->pst_config->doneRequests < pclient->pst_config->needRequestNum)
	{
		pclient->pst_config->latency[ucurrequests++] = pclient->latency;
		pclient->pst_config->doneRequests++;
	}

	//check请求数是否已经完成
	ReactorCloseConnectionNormal(pclient);
	
	return 0;
}



///////////////////////END FUNCTION CALLBACK//////////////////////////////////
void ShowReport(StatisticsStruct *pstConfig)
{
	assert(NULL != pstConfig);
	uint64_t totalTime = pstConfig->endTime - pstConfig->startTime;	//所有请求开始到结束的时间
	float reqpersec = (float)pstConfig->doneRequests / ((float)totalTime / 1000);	//平均每个请求耗费的时间

	if (pstConfig->keepalive == 1){
		printf("KeepAlive is open\n");
	}
	
	printf("parallel clients:%u\n", pstConfig->needClientNum);
	printf("%u requests completed in %lu seconds\n", pstConfig->doneRequests, totalTime / 1000);

	unsigned i, curlat = 0;
	uint64_t total_lat = 0;
	uint64_t avg_lat = 0;
	float perc;

	//排序数组
	qsort(pstConfig->latency, pstConfig->doneRequests, sizeof(uint64_t), CompareLatency);

	for (i = 0; i < config.doneRequests; i++) {

		total_lat += config.latency[i] / 1000LL;

		if (config.latency[i] / 10000 != curlat || i == (config.doneRequests - 1)) {
			curlat = config.latency[i] / 10000;
			perc = ((float)(i + 1) * 100) / config.doneRequests;
			//printf( < perc << "% <= " << config.latency[i] / 1000 << " milliseconds" << endl;
		}
	}
	avg_lat = total_lat / config.doneRequests;

	printf("ms average latency:%lu\n", avg_lat);
	
	printf("requests are needed:%u\n", pstConfig->needRequestNum);

	printf("requests are sended:%u\n",pstConfig->totalRequests);
	
	printf("clients are timeout:%u\n" ,pstConfig->timeoutClientNum);

	printf("clients are reset:%u\n", pstConfig->resetClientNum);

	printf("clients are created:%u\n", pstConfig->createClientNum);

	printf("total milliseconds used:%lu\n", pstConfig->endTime - pstConfig->startTime);

	printf("requests per second:%f\n", reqpersec);
}



void ShowThroughPut(StatisticsStruct *pstConfig)
{
	assert(pstConfig != NULL);
	uint64_t totalTime = GetmsecTime() - pstConfig->startTime;
	//到目前为止使用了多少时间
	float rps = (float)pstConfig->doneRequests / ((float)totalTime / 1000);

	printf("total time:%lu(ms),%u requests done,rps:%f\n ", totalTime, pstConfig->doneRequests, rps);
}
