/*
* Description: 	socket api 封装
* Author:
* Date:
* Log:
*/

#include "socket_util.h"
#include "socket_api.h"
#include "comm.h"
#include "reactor.h"
#include "bin_proto.h"

/*
	FD超时的回调处理函数(回调参数只有一个)
*/
void ConnectionTimeoutCallback(int t_iPid,void *t_pstArg){
	if (NULL == t_pstArg){
		return;
	}

	ConnFd *pstConnFd = (ConnFd *)t_pstArg;
	LOG_INFO("[pid=%d,%s:%d]fd=%d timeout", t_iPid,pstConnFd->szIpaddr, pstConnFd->iPort, pstConnFd->iFd);
	//删除连接(这里可以做更多事情)
	__DestroyConn(pstConnFd);
	return;
}


/*
	Fun:创建新的连接
	@param:t_pstEventBase
	@param:t_iListenFd
	@param:t_pClientIp
	@param:t_iClientPort
	@return:返回创建的客户端FD结构体指针
*/
static struct __single_conn_fd *__CreateNewConn(EventBase *t_pstEventBase,int t_iClientfd,char *t_pClientIp,int t_iClientPort){
	//
	if (NULL == t_pstEventBase || NULL == t_pClientIp){
		return NULL;
	}

	ConnFd *pstConnFd = (ConnFd *)malloc(sizeof(ConnFd));
	if (NULL == pstConnFd){
		LOG_ERROR("[pid=%d]malloc connfd error:%s", strerror(errno));
		exit(-1);
	}
	memset(pstConnFd, 0x00, sizeof(ConnFd));
	pstConnFd->iFd = t_iClientfd;
	pstConnFd->iPort = t_iClientPort;
	memcpy(pstConnFd->szIpaddr, t_pClientIp, IP_LEN);

	pstConnFd->pstEventBase = t_pstEventBase;	//单个连接如何找到(link to)eventbase?

	//fd buffer init
	pstConnFd->iRecvTotalLen = pstConnFd->iSendTotalLen = MAX_TCP_BUF_SIZE;
	pstConnFd->pszRecvBuf = (char *)malloc(sizeof(char)*pstConnFd->iRecvTotalLen);
	if (!pstConnFd->pszRecvBuf){
		LOG_ERROR("[pid=%d]malloc connfd recvbuff error:%s", strerror(errno));
		exit(-1);
	}
	pstConnFd->pszSendBuf = (char *)malloc(sizeof(char)*pstConnFd->iSendTotalLen);
	if (!pstConnFd->pszSendBuf){
		LOG_ERROR("[pid=%d]malloc connfd sendbuff error:%s", strerror(errno));
		exit(-1);
	}

	//创建最小堆(超时)节点
	pstConnFd->pstTimeoutEvent = (TimeoutEvent *)malloc(sizeof(TimeoutEvent));
	if (!pstConnFd->pstTimeoutEvent){
		LOG_ERROR("[pid=%d]malloc connfd timeevent error:%s", strerror(errno));
		exit(-1);
	}

	//init time event(初始化超时结构node)
	MinheapTimeoutEventInit(pstConnFd->pstTimeoutEvent, NULL, NULL,getpid(),pstConnFd->iFd);

	//初始化fd文件描述符大数组,key/index是fd
	//FdFileEventInit(t_pstEventBase,t_iClientfd);
	FdFileEventInit(t_pstEventBase, pstConnFd->iFd);

	return pstConnFd;
}


/*
@Fun:销毁连接
@fun:主要完成:1资源回收2.统计3.取消事件监听4.close连接5.删除堆对应的节点(删除定时器)5.文件fd数组置位
@param:t_pstConnFd
@return:
*/
void __DestroyConn(ConnFd *t_pstConnFd){
	if (NULL == t_pstConnFd){
		return;
	}

    printf("destory connection...%d\n",t_pstConnFd->iFd);

	//根据connfd的指针寻找到eventbase指针
	EventBase *pstEventBase = t_pstConnFd->pstEventBase;
	if (NULL == pstEventBase){
		return;
	}

	//活动客户端数目-1
	pstEventBase->iClientCount -= 1;

#ifdef __DEBUG
	LOG_DEBUG("[Pid=%d,%s:%d,fd=%d, close socket,curclient count=%d]",getpid(),t_pstConnFd->szIpaddr,t_pstConnFd->iPort,\
		pstEventBase->iClientCount);
#endif
	
	//1.解除epoll监听(para:base---要操作哪个fd----操作的掩码)
	if (RET_FAIL == EventDel(pstEventBase, t_pstConnFd->iFd, EVENT_WRITABLE | EVENT_READABLE)){
		LOG_ERROR("[Pid:%d,%s:%d,fd=%d,error=%s,event del error]", getpid(), t_pstConnFd->szIpaddr, \
			t_pstConnFd->iPort,t_pstConnFd->iFd,strerror(errno));
		exit(-1);
	}
	
	//2.关闭socket
	close(t_pstConnFd->iFd);

	//3.删除定时器
	UnregisterTimerOutEvent(pstEventBase, t_pstConnFd->pstTimeoutEvent);

	//4.初始化fd数组(将t_pstConnFd指向的位置清0)
	FdFileEventInit(pstEventBase, t_pstConnFd->iFd);

	//5.释放堆内存
	free(t_pstConnFd->pszRecvBuf);
	free(t_pstConnFd->pszSendBuf);
	free(t_pstConnFd->pstTimeoutEvent);
	free(t_pstConnFd);
		
	return;
}

/*
	@fun:连接超时回调函数
	@fun:在reactor堆中处理超时的fd调用
	@fun:调用方式为:pst->callbackfunction(pst->callback_arg)----和processaccpet/recv好像是一样的
*/
void ProcessConnTimeoutCallback(void *t_pArg){
	//
	if (NULL == t_pArg){
		return;
	}
	
	ConnFd *pstConnFd = (ConnFd *)t_pArg;
	
#ifdef __DEBUG
	LOG_DEBUG("[Pid=%d,Clientip=%s,Clientport=%d,Fd=%d timeout]",getpid(),pstConnFd->szIpaddr,pstConnFd->iPort,pstConnFd->iFd);
#endif

	__DestroyConn(pstConnFd);
	
	return;
}


/*
	@fun:accpet回调函数
	客户端连接事件回调函数
	为了防止同时并发大量的新连接，可以将系统参数/proc/sys/net/core/somaxconn设置大点
*/
void ProcessAcceptCallback(EventBase *t_pstEventBase, int t_iFd, void *t_pData){
	//
	if (NULL == t_pstEventBase || t_iFd < 0){
		return;
	}

	int iClientPort;
	int iClientFd;
	char szClientIp[IP_LEN] = { 0 };
	struct sockaddr_in stClientAddr;
	socklen_t iaddrLen = sizeof(struct sockaddr_in);

	//接收新的客户端
	if (t_pstEventBase->iEpType == EPOLL_ET_TYPE){
		//ET-model:一次触发可能多个连接
		/*
		while (1){
			iClientFd = accept(t_iFd, );

		}
		*/
		//TODO：pandaychen，完成EPOLL_ET模式下的逻辑
	}
	else if (t_pstEventBase->iEpType == EPOLL_LT_TYPE)
	{
		//normal accpet
		while (1)
		{
            //int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
			iClientFd = accept(t_iFd, (struct sockaddr *)&stClientAddr, (socklen_t *) &iaddrLen);
            
			//这里存在惊群问题
			//printf("accept:%d\n",iClientFd);
			if (iClientFd == -1){
				//may encounter a error
				if (errno == EINTR){
					//慢系统调用,自己重启
					continue;
				}
				else
				{
					//这里是否需要加上EAGAIN的判断?
					break;
				}
			}
			break;
		}
		//end of normal accept

		if (iClientFd < 0){
			//
			if (errno != EAGAIN && errno != EWOULDBLOCK){
				LOG_ERROR("[pid=%d]accept error:%s", getpid(), strerror(errno));
				exit(1);
			}
			//如果是EAGAIN或者EWOULDBLOCK错误的话,在下个触发返回中再尝试下
			return;
		}
		strncpy(szClientIp,inet_ntoa(stClientAddr.sin_addr), IP_LEN);
		iClientPort = ntohs(stClientAddr.sin_port);

		t_pstEventBase->iClientCount++;          


		//设置为非阻塞模式
		if (PynServerSetNonblock(iClientFd) == RET_FAIL){
			LOG_ERROR("[pid=%d]set acceptfd noblock error:%s", getpid(), strerror(errno));
			exit(1);
		}

		//？？？？？？？？？？？？？？？？？？？？？？？？？？？LOG_INFO(Debug("[%d] %s:%d connected, count=%d", getpid(), cip, cport, base->count));
		//创建new-conn的结构体(创建buffer/初始化heap-node)
		ConnFd *pstNewConnFd = __CreateNewConn(t_pstEventBase, iClientFd, szClientIp,iClientPort);
		
        
		//注册连接超时定时器回调函数,回调函数参数是pstNewConnFd
		//TimeoutEventInit(pstNewConnFd->pstTimeoutEvent, ProcessConnTimeoutCallback, pstNewConnFd);
		MinheapTimeoutEventInit(pstNewConnFd->pstTimeoutEvent, ConnectionTimeoutCallback, pstNewConnFd, getpid(), iClientFd);

        //LOG_INFO("CCC");

		//将此fd设置TCP超时,并push到堆(超时时间=eventbase当前缓存时间+超时间隔MAX_CONN_TIMEOUT)
		RegisterTimerOutEvent(t_pstEventBase, pstNewConnFd->pstTimeoutEvent, MAX_CONN_TIMEOUT);

		//注册iClientFd的读事件到reactor主结构(注册新连接读IO事件到事件框架)
		if (RET_FAIL == EventAdd(t_pstEventBase, iClientFd, EVENT_READABLE, ProcessRecvCallback, pstNewConnFd )){
			LOG_ERROR("[pid:%d,%s:%d event add failed,error=%s]",getpid(),szClientIp,iClientPort,strerror(errno));
			exit(-1);
		}
	}
	else{
		LOG_ERROR("[pid=%d,%s:%d error epoll type,epolltype=%d]", getpid(), szClientIp, iClientPort, t_pstEventBase->iEpType);
		exit(-1);
	}
	return;
}


/*
	@FUN:扩大接收缓冲区:在每次recv之后,判断某个FD的接收缓冲区大小是否足够
	@param:
	@param:t_iNeedRecvLen ---  还需要接收的字节数
	@ret:貌似没有用到
*/
char *ResizeRecvBufferPtr(ConnFd *t_pstConnFd, int t_iNeedRecvLen){
	if (NULL == t_pstConnFd || t_iNeedRecvLen <= 0){
		return NULL;
	}

	if ((int)(t_pstConnFd->iRecvTotalLen - t_pstConnFd->iRecvCurLen >= t_iNeedRecvLen)){
		//t_pstConnFd->iRecvTotalLen 默认值为MAX_BUFFER_SIZE
		//t_pstConnFd->iRecvCurLen 已经接收并存储的大小
		//t_iNeedRecvLen 还需要接收的大小

		//不需要扩容
		return (char *)t_pstConnFd->pszRecvBuf + t_pstConnFd->iRecvCurLen;
	}

	//not enough size
	while (t_pstConnFd->iRecvTotalLen -t_pstConnFd->iRecvCurLen < t_iNeedRecvLen)
	{
		//每次扩大2倍,扩大总容量的2倍
		t_pstConnFd->iRecvTotalLen *= 2;
	}

	char *pNewRecvBuffer = (char *)malloc(t_pstConnFd->iRecvTotalLen);
	if (NULL == pNewRecvBuffer){
		return NULL;
	}
	
	//将原来空间的数据move过来
	memmove(pNewRecvBuffer, t_pstConnFd->pszRecvBuf, t_pstConnFd->iRecvCurLen);
	free(t_pstConnFd->pszRecvBuf);
	t_pstConnFd->pszRecvBuf = pNewRecvBuffer;

	return t_pstConnFd->pszRecvBuf + t_pstConnFd->iRecvCurLen;
}

/*
	@FUN:扩大发送缓冲区(有点技巧)
	t_pstConnFd->iSendPacketLen:当前发送缓冲区已存在报文的长度(整个包)
*/
char *ResizeSendBufferPtr(ConnFd *t_pstConnFd, int t_iNeedSendLen){
	if (NULL == t_pstConnFd){
		return NULL;
	}

	/*
		收包和发包不一样,收包是一点点收,以iRecvCurLen积累;而发包,首先应该是把要发送的包copy
		到sendbuf
	*/
	if (t_pstConnFd->iSendTotalLen - t_pstConnFd->iSendPacketLen >= t_iNeedSendLen){
		//剩余空间足够,返回跨越一个包的指针
		return t_pstConnFd->pszSendBuf + t_pstConnFd->iSendPacketLen;
	}

	if (t_pstConnFd->iSendTotalLen - t_pstConnFd->iSendPacketLen + t_pstConnFd->iSendCurLen > t_iNeedSendLen){
		//剩余也足够,只不过需要简单调整下
		//把现有的未发送的buffer移置开头
		//把已发送的部分覆盖掉
		memmove(t_pstConnFd->pszSendBuf,t_pstConnFd->pszSendBuf+t_pstConnFd->iSendCurLen /*未发送部分*/,	\
			t_pstConnFd->iSendPacketLen-t_pstConnFd->iSendCurLen/*未发送部分的长度*/ );

		//覆盖掉已发送的部分,更新发送的总长度
		t_pstConnFd->iSendPacketLen -= t_pstConnFd->iSendCurLen;
		t_pstConnFd->iSendCurLen = 0;

		//返回可以的数据部分长度
		return t_pstConnFd->pszSendBuf + t_pstConnFd->iSendPacketLen;
	}
	
	//需要扩容
	while ( (t_pstConnFd->iSendTotalLen - t_pstConnFd->iSendPacketLen) < t_iNeedSendLen )
	{
		t_pstConnFd->iSendTotalLen *= 2;
	}

	char *pNewSendBuffer = (char *)malloc(sizeof(t_pstConnFd->iSendTotalLen));

	//移动到新内存,抛弃掉已经发送完成的部分
	memmove(pNewSendBuffer, t_pstConnFd->pszSendBuf + t_pstConnFd->iSendCurLen, \
		t_pstConnFd->iSendPacketLen - t_pstConnFd->iSendCurLen);
	free(t_pstConnFd->pszSendBuf);


	t_pstConnFd->pszSendBuf = pNewSendBuffer;	//new部分
	
	t_pstConnFd->iSendPacketLen -= t_pstConnFd->iSendCurLen;
	t_pstConnFd->iSendCurLen = 0;	//抛弃掉了已发送部分,目前已发送为0
	//注意次序问题!!!!!!!

	//返回可用地址
	return t_pstConnFd->pszSendBuf + t_pstConnFd->iSendPacketLen/*注意这个值已经在上面更新了哦*/;
}


/*
	@fun:recv回调函数
	@回调函数的参数是前三个
	@数据接收事件处理函数,为提高性能此函数也协议完全耦合，先接收数据包头，再接收BODY

	@Info:iiRecvCurLen---已经接受的数据长度
		  
*/
void ProcessRecvCallback(EventBase *t_pstEventBase, int t_iFd, void *t_pData){
	if (NULL == t_pstEventBase || t_iFd <0){
		return;
	}
	int iRet = 0;

	ConnFd *pstConnFdRead = (ConnFd *)t_pData;

	if (NULL == pstConnFdRead){
		return;
	}
	//剩余的数据长度
	int iRemainLen = 0;
	int iNeedRecvLen = 0;	//本次触发希望的接收值
	
	//printf("recv pkts,%d,%d\n",pstConnFdRead->iRecvCurLen, PACKET_HEAD_LEN);

	//已接收数据小于HEAD长度时，继续接收HEAD，否则接收BODY
	if (pstConnFdRead->iRecvCurLen < PACKET_HEAD_LEN){
		iRet = PynServerRecvData(pstConnFdRead->iFd, \
			pstConnFdRead->pszRecvBuf + pstConnFdRead->iRecvCurLen, \
			PACKET_HEAD_LEN - pstConnFdRead->iRecvCurLen);

		if (iRet <= 0){
			//error or reset by peer
			__DestroyConn(pstConnFdRead);
			return;
		}

		//更新已接收长度
		pstConnFdRead->iRecvCurLen += iRet;
		if (pstConnFdRead->iRecvCurLen < PACKET_HEAD_LEN){
			//还是没收到包头的长度,先返回,下次再收
			return;
		}
		
		//printf("2recv pkts,%d,%d\n",pstConnFdRead->iRecvCurLen, PACKET_HEAD_LEN);	
	
		//已经接收完包头部分(固定),根据包头部分计算出这个包的长度,由于TCP是流式协议,所以这个包头长度的设置非常重要

		////根据协议计算整个包的长度,能使用接口返回吗?这里不行,因为包不完整
		pstConnFdRead->iPacketLen = *(UN_4BYTE *)(pstConnFdRead->pszRecvBuf + PACKET_LEN_POS);
		//转换成主机序
		pstConnFdRead->iPacketLen = (int)ntohl(pstConnFdRead->iPacketLen);
		
		//已经接收完包头,判断包头中的固定字段是否合法
		if (*pstConnFdRead->pszRecvBuf != PROTO_SOH ||	\
			pstConnFdRead->iPacketLen <= PACKET_BODY_POS){
			LOG_ERROR("[%s:%d] invalid package", pstConnFdRead->szIpaddr, pstConnFdRead->iPort);
			__DestroyConn(pstConnFdRead);
			return;
		}

		if (pstConnFdRead->iPacketLen > PACKET_MAX_LEN){
			LOG_ERROR("[%s:%d] package too long, len=%d", pstConnFdRead->szIpaddr, pstConnFdRead->iPort,\
				pstConnFdRead->iPacketLen);
			__DestroyConn(pstConnFdRead);
			return;
		}
		//printf("3recv pkts,%d,%d\n",pstConnFdRead->iRecvCurLen, PACKET_HEAD_LEN);	

		//移动readbuf指针
		//还需要接收的字节= 报文大小-头部大小
		if (NULL == ResizeRecvBufferPtr(pstConnFdRead, pstConnFdRead->iPacketLen - PACKET_HEAD_LEN)){
			__DestroyConn(pstConnFdRead);
			return;
		}
	}

	//已经接收完报文头部,为防止其它fd饿死,如果包长小于4096,则接收整个包，否则一次只接收4096个字节
	if (pstConnFdRead->iPacketLen <= 0){
		__DestroyConn(pstConnFdRead);
		return;
	}

	iRemainLen = pstConnFdRead->iPacketLen - pstConnFdRead->iRecvCurLen;
	iNeedRecvLen = iRemainLen > 4096 ? 4096 : iRemainLen;
	//printf("**************\n");
	iRet = PynServerRecvData(pstConnFdRead->iFd, \
		pstConnFdRead->pszRecvBuf+pstConnFdRead->iRecvCurLen,\
		iNeedRecvLen);

	//printf("4recv pkts,%d,%d\n",pstConnFdRead->iRecvCurLen, PACKET_HEAD_LEN);

	if (iRet <= 0){
		//这里=0的情况是客户端直接发送完数据就FIN了,好像不符合一般情况
		__DestroyConn(pstConnFdRead);
		return;
	}
	pstConnFdRead->iRecvCurLen += iRet;
	
	//printf("5recv pkts,%d,%d,%d\n",pstConnFdRead->iRecvCurLen, PACKET_HEAD_LEN,pstConnFdRead->iPacketLen);
	//printf("%c,%c\n",pstConnFdRead->pszRecvBuf[13],pstConnFdRead->pszRecvBuf[22]);
	
	//未达到一个包的长度时，继续接收
	if (pstConnFdRead->iRecvCurLen < pstConnFdRead->iPacketLen){
		return;
	}

	//调用处理函数处理一个完整的packet(这里可能需要加上返回值判断,如果报文格式不符合要求,就RESET,
	//当然了,也可借用minheap超时处理,但是这样不是很科学:))
	// process_packet(c, c->rbuf, c->rlen);
	ProcessEntirePacket(pstConnFdRead);

	//重新注册连接超时定时器(再加上一个超时时间)
	RegisterTimerOutEvent(t_pstEventBase,pstConnFdRead->pstTimeoutEvent,MAX_CONN_TIMEOUT);

	//处理完一个数据包后，缓冲区又可以重复使用了(当前接收长度清0,报文长度清0),适合长连接场景
	pstConnFdRead->iRecvCurLen = 0;
	pstConnFdRead->iPacketLen = 0;
	

	return;
}


/*
	@fun:recv回调函数
	@回调函数的参数是前三个
	@数据接收事件处理函数,为提高性能此函数也协议完全耦合，先接收数据包头，再接收BODY
	@Info:iiRecvCurLen---已经接受的数据长度
*/
void ProcessSendCallback(EventBase *t_pstEventBase, int t_iFd, void *t_pData){
	if (NULL == t_pData || NULL == t_pstEventBase || t_iFd < 0){
		return;
	}
	ConnFd *pstConnFdSend = (ConnFd *)t_pData;
	int iRet=0;
	int iRemainSend = 0;

	if (0>= pstConnFdSend->iSendPacketLen){
		//LOG_ERROR("need send packet length zero");
		return; 
	}
	
	//pstConnFdSend->pszSendBuf + pstConnFdSend->iSendCurLen -- 上次发送结束的位置
	//pstConnFdSend->iSendPacketLen-pstConnFdSend->iSendCurLen -- 还需要发送的长度
	iRet = PynServerSendData(pstConnFdSend->iFd,\
		pstConnFdSend->pszSendBuf + pstConnFdSend->iSendCurLen,\
		pstConnFdSend->iSendPacketLen-pstConnFdSend->iSendCurLen);

	if (iRet < 0){
		//write error
		LOG_ERROR("[%s:%d] write error: %s",pstConnFdSend->szIpaddr, \
			pstConnFdSend->iPort, strerror(errno));
		__DestroyConn(pstConnFdSend);
		return;
	}
	else{
		//>=0
		//更新已发送长度
		pstConnFdSend->iSendCurLen += iRet;
	}
	
	//检查数据是否完全发送完成
	if (pstConnFdSend->iSendPacketLen == pstConnFdSend->iSendCurLen){
		//send succ
		if (RET_FAIL == EventDel(t_pstEventBase, pstConnFdSend->iFd, EVENT_WRITABLE)){
			//LT模式下,需要移除写事件监听,否则会不停触发写事件(当缓冲区不满时)
			LOG_ERROR("event_del failed: %s", strerror(errno));
			exit(-1);
		}
		
		//清空
		pstConnFdSend->iSendPacketLen = 0;
		pstConnFdSend->iSendCurLen = 0;
	}
	//数据未全部发送,等待下一次触发写事件处理
	return;
}


/*	
	@FUN:向conn增加需要发送的包,并注册写的IO事件
*/
int AppendPktToBuff(ConnFd *t_pstConnFd, char *t_pszBuff/* =NULL */, int t_iLen){
	if (NULL == t_pstConnFd){
		return RET_FAIL;
	}

	//t_pstConnFd->iSendPacketLen 待发送的报文长度(初始值是0)
	if (t_pszBuff != NULL){
		memcpy(t_pstConnFd->pszSendBuf + t_pstConnFd->iSendPacketLen, t_pszBuff, t_iLen);
		//t_pstConnFd->iSendPacketLen += t_iLen;
	}
	else{
		memcpy(t_pstConnFd->pszSendBuf + t_pstConnFd->iSendPacketLen,t_pstConnFd->pszSendBuf, t_iLen);
	}
    printf("%d,%d\n",  t_pstConnFd->iSendPacketLen,t_iLen);
	t_pstConnFd->iSendPacketLen += t_iLen;

	//注册发送数据的回调函数,回调函数的参数是t_pstConnFd
	if (RET_FAIL == EventAdd(t_pstConnFd->pstEventBase, t_pstConnFd->iFd, \
		EVENT_WRITABLE, ProcessSendCallback, t_pstConnFd)){
		//Err("[%d] %s:%d event_add failed, %s", c->ip, c->port, strerror(errno) );
		exit(-1);
	}

	return RET_SUCC;
}
