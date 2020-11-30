/*
* File name:    reactor.c
* @Author:      pandaychen
* @version: 
* Description: 	reactor反应堆模式事件处理框架
* ChangeLog:
*/

#include "reactor.h"
#include "comm.h"
#include "socket_util.h"
#include "min_heap.h"

/*
	@FUN:初始化单进程事件驱动，并将listenfd的读IO事件添加到事件驱动中，设置回调函数为process_accept
	@param:t_iFd
	@param:t_iMaxEventSize ---FD数组的大小,即单个进程处理的连接数
*/
EventBase *EventBaseCreate(int t_iFd, int t_iMaxEventSize){
	if (t_iFd < 0 || t_iMaxEventSize <= 0){
		LOG_ERROR("event base create error");
		return NULL;
	}	

	EventBase *pstEventBase = __CreateNewEventBase(t_iMaxEventSize);
	if (NULL == pstEventBase){
		LOG_ERROR("Can't create file_event loop: %s", strerror(errno));
		return NULL;
	}

	pstEventBase->iListenFd = t_iFd;
	//默认使用LT
	pstEventBase->iEpType = EPOLL_LT_TYPE;

	//注册accpet的事件处理函数
	//ProcessAcceptCallback不需要回调参数,有pstBase指针就可以
	//pstEventBase, iListenFd, EVENT_READABLE 是回调函数process_accept的参数吗?显然这种说法是错误的!
	if (RET_FAIL == EventAdd(pstEventBase, pstEventBase->iListenFd, EVENT_READABLE, ProcessAcceptCallback, NULL)){
		LOG_ERROR("Can't create file file_event: %s", strerror(errno));
		return NULL;
	}
	return pstEventBase;
}



/*
	FdFileEventInit-->新建连接到来时,初始化eventbase中的fd数组的对应位置结构(以fd作为下标)
	param	t_pstEventBase
	param  t_iFd	需要操作的fd
	ret :   CALL_FAIL -1
			CALL_SUCC 0
*/
int FdFileEventInit(EventBase *t_pstEventBase, int t_iFd){
	if (NULL == t_pstEventBase){
		return RET_FAIL;
	}
	
	FdEvent *pstFdEvent = (FdEvent *)&(t_pstEventBase->pstFdEventsArray[t_iFd]);
	if (NULL == pstFdEvent){
		LOG_ERROR("get fdfileevent index null");
		return RET_FAIL;
	}

	pstFdEvent->iMask = 0x0;
	pstFdEvent->pFunEventCallbackRead = pstFdEvent->pFunEventCallbackWrite = NULL;
	pstFdEvent->pstArg = NULL;

	return RET_SUCC;
}

/*
	@fun __CreateNewEventBase-->创建每个单独进程的EventBase结构
	param  t_iEventSize	每个进程最多可以接受连接的数组的大小(需要调整系统配置)
	ret :
*/
EventBase * __CreateNewEventBase(int t_iEventSize){
	if (t_iEventSize <= 0){
		LOG_ERROR("create new eventbase error,event size error");
		return NULL;
	}
	//http://blog.csdn.net/likelikebing/article/details/38533261

	EventBase *pstEventBase = (EventBase *)malloc(sizeof(EventBase));
	if (NULL == pstEventBase){
		LOG_ERROR("create event base,malloc error");
		return NULL;
	}
	memset(pstEventBase, 0x0, sizeof(EventBase));
	pstEventBase->iStopSign = 0;

	pstEventBase->iSize = t_iEventSize;
	pstEventBase->iFdEventArraySize = t_iEventSize;
	
	//创建epoll-fd
	pstEventBase->iEpfd = epoll_create(1024);
	if (pstEventBase->iEpfd < 0){
		free(pstEventBase);
		LOG_ERROR("create event base,create epoll fd error");
		return NULL;
	}
	
	//epoll api need this
	pstEventBase->pstEpollEvents = (struct epoll_event *)malloc(sizeof(struct epoll_event)*EPOLL_SIZE);
	if (NULL == pstEventBase->pstEpollEvents){
		free(pstEventBase);
		LOG_ERROR("create event base,create event base,malloc epoll events array error");
		return NULL;
	}

	//fd events array(海量连接数组)
	pstEventBase->pstFdEventsArray = (FdEvent *)malloc(t_iEventSize*sizeof(FdEvent));
	if (NULL == pstEventBase->pstFdEventsArray){
		free(pstEventBase->pstEpollEvents);
		free(pstEventBase);
		LOG_ERROR("create event base,malloc fd events array error");
		return NULL;
	}

	//time base cache
	pstEventBase->pstTimeCache = (struct timeval *)malloc(sizeof(struct timeval));
	if (NULL == pstEventBase->pstTimeCache){
		free(pstEventBase->pstFdEventsArray);
		free(pstEventBase->pstEpollEvents);
		free(pstEventBase);
		LOG_ERROR("create event base,malloc time cache error");
		return NULL;
	}

	pstEventBase->pstTimeCache->tv_sec = 0;
	pstEventBase->pstTimeCache->tv_usec = 0;
	//缓存了create base的时间
	gettimeofday(pstEventBase->pstTimeCache, NULL);

	//heap header
	pstEventBase->pstHeapHeader = (MinHeapHeader *)malloc(sizeof(MinHeapHeader));
	if (NULL == pstEventBase->pstHeapHeader){
		free(pstEventBase->pstFdEventsArray);
		free(pstEventBase->pstEpollEvents);
		free(pstEventBase->pstTimeCache);
		free(pstEventBase);
		LOG_ERROR("create event base,malloc min heap header error");
		return NULL;
	}

	if (RET_FAIL == MinheapCreate(pstEventBase->pstHeapHeader)){
		//
		free(pstEventBase->pstFdEventsArray);
		free(pstEventBase->pstEpollEvents);
		free(pstEventBase->pstTimeCache);
		free(pstEventBase);
		LOG_ERROR("create event base,min heap header init error");
		return NULL;
	}
	return pstEventBase; 
}

/*
	@fun __DeleteEventBase-->删除进程的EventBase结构
	param  t_pstEventBase
	ret :
*/
void __DeleteEventBase(EventBase *t_pstEventBase){
	if (NULL == t_pstEventBase){
		return;
	}
	if (t_pstEventBase->pstTimeCache){
		free(t_pstEventBase->pstTimeCache);
	}
	if (t_pstEventBase->pstEpollEvents){
		free(t_pstEventBase->pstEpollEvents);
	}

	if (t_pstEventBase->pstFdEventsArray){
		free(t_pstEventBase->pstFdEventsArray);
	}

	if (t_pstEventBase->pstHeapHeader){
		free(t_pstEventBase->pstHeapHeader);
	}

	if (t_pstEventBase){
		free(t_pstEventBase);
	}

	return ;
}


/*
@fun __EventBaseStop-->删除进程的EventBase结构
param  t_pstEventBase
ret :
*/
void __EventBaseStop(EventBase *t_pstEventBase){
	if (t_pstEventBase == NULL){
		return;
	}

	t_pstEventBase->iStopSign = EPOLL_EVENT_BASE_STOP;

	return;
}

/*
@fun EventAdd-->添加期望事件和回调函数到监听epoll结构
@param: t_pstEventBase 
@param: t_iFd	--->accept fd or listenfd(only once)
@param: t_iEventMask -->希望监听的事件掩码(读/写)--待注册到epoll上的事件
@param:	t_pEventCallbackProc	注册的回调函数
@param:	t_pstCallbackArg	注册的回调函数调用参数(没有就是NULL)
ret :
*/
int EventAdd(EventBase *t_pstEventBase, int t_iFd, int t_iEventMask, EventCallbackProc *t_pEventCallbackProc, void *t_pstCallbackArg){
	if (NULL == t_pstEventBase){
		return RET_FAIL;
	}

	struct epoll_event stEV = {0};
	stEV.events = 0;
	
	if (t_iFd > t_pstEventBase->iFdEventArraySize){
		//下标越界
		LOG_ERROR("new fd=%d index invalid", t_iFd);
		return RET_FAIL;
	}

	//get fdevent ptr
	//获取fd下标的指针(为了修改内容)
	FdEvent *pstFdEvent = (FdEvent *)&t_pstEventBase->pstFdEventsArray[t_iFd];
	if (NULL == pstFdEvent){
		LOG_ERROR("new fd=%d ptr invalid", t_iFd);
		return RET_FAIL;
	}
	
	//已经注册,直接返回
	//pstFdEvent->iMask:已注册的事件 t_iEventMask:待注册的事件
	if (pstFdEvent->iMask & t_iEventMask){
		return RET_SUCC;
	}

	//未注册,需要注册
	//如果mask为0x00,初始化,那么就是CTL_ADD,否则非第一次就用CTL_MOD
	int iOper = pstFdEvent->iMask == EVENT_NONE ? EPOLL_CTL_ADD : EPOLL_CTL_MOD;
	pstFdEvent->iMask |= t_iEventMask;

	//根据pstFdEvent->iMask中所包含的事件掩码确定所有的监听类型
	//根据fd中的mask,来注册希望epoll监听在本fd上的事件类型
	if (pstFdEvent->iMask & EVENT_READABLE){
		stEV.events |= EPOLLIN;
	}
	if (pstFdEvent->iMask & EVENT_WRITABLE){
		stEV.events |= EPOLLOUT;
	}

	//设置epoll操作的fd
	stEV.data.fd = t_iFd;


	//设置回调函数(这里不能用pstFdEvent->iMask,想想为什么)和回调函数的参数
	//这里t_pEventCallbackProc 是和t_iEventMask对应的回调函数(要么读要么写)
	if (t_iEventMask & EVENT_READABLE){
		pstFdEvent->pFunEventCallbackRead = t_pEventCallbackProc;
	}
	if (t_iEventMask & EVENT_WRITABLE){
		pstFdEvent->pFunEventCallbackWrite = t_pEventCallbackProc;
	}
	pstFdEvent->pstArg = t_pstCallbackArg;

	if (-1 == epoll_ctl(t_pstEventBase->iEpfd, iOper, t_iFd, &stEV)){
		return RET_FAIL;
	}
	return RET_SUCC;
}



/*
@fun EventAdd-->添加期望事件和回调函数到监听epoll结构
@param: t_pstEventBase
@param: t_iFd	--->accept fd or listenfd(only once)
@param: t_iEventMask -->希望监听的事件掩码(读/写)
@param:
ret :
info:
EPOLL_CTL_ADD：注册新的fd到epfd中；
EPOLL_CTL_MOD：修改已经注册的fd的监听事件；
EPOLL_CTL_DEL：从epfd中删除一个fd；
*/
int EventDel(EventBase *t_pstEventBase, int t_iFd, int t_iEventMask){
	if (NULL == t_pstEventBase){
		return RET_FAIL;
	}


	if (t_pstEventBase->iFdEventArraySize < t_iFd){
		LOG_ERROR("fd[%d] value invalid");
		return RET_FAIL;
	}

	struct epoll_event stEv;
	stEv.events = 0;

	FdEvent *pstFdEvent = (FdEvent *)&t_pstEventBase->pstFdEventsArray[t_iFd];
	if (NULL == pstFdEvent){
		//LOG_ERROR
		return RET_FAIL;
	}

	if (pstFdEvent->iMask == EVENT_NONE){
		return RET_SUCC;
	}
	
	stEv.data.fd = t_iFd;
	//未注册,已经删除,直接返回
	if (!(pstFdEvent->iMask & t_iEventMask)){
		return RET_SUCC;
	}
	
	//保留先前状态,移除需要del的状态
	pstFdEvent->iMask = pstFdEvent->iMask &(~t_iEventMask);
	if (pstFdEvent->iMask & EVENT_READABLE){
		stEv.events |= EPOLLIN;
	}

	if (pstFdEvent->iMask & EVENT_WRITABLE){
		stEv.events |= EPOLLOUT;
	}

	//删除已注册的event事件 EPOLL_CTL_DEL

	//如果这里待监听的事件都没有了,那么可以把这个FD给删除了
	//注意:这里的删除指把t_iFd从iEpfd中删除,并不是从整个fdarray中删除!!!!
	int iOper = pstFdEvent->iMask == EVENT_NONE ? EPOLL_CTL_DEL : EPOLL_CTL_MOD;

	if (0 > epoll_ctl(t_pstEventBase->iEpfd, iOper, t_iFd, &stEv)){
		return RET_FAIL;
	}

	return RET_SUCC;
}

/*
	@fun:对于accept获取到的新的conn,
	1.初始化各种结构,初始化回调参数-函数等,malloc应用层发送/接收缓冲区
	2.加入本进程回调reactor结构
	3.初始化堆节点,设置本conn的超时时间,插入堆(当然触发堆的自动排序)
	@para:
	@para:t_uSec:设置超时时间,单位毫秒
	@ret:
*/
int RegisterTimerOutEvent(EventBase *t_pstEventBase, TimeoutEvent *t_pstTimeoutEvent,int t_uSec){
	if (NULL == t_pstEventBase || NULL == t_pstTimeoutEvent){
		LOG_ERROR("registe timeout event error");
		return RET_FAIL;
	}

	t_pstTimeoutEvent->stInterval.tv_sec = t_uSec/1000;	//seconds
	t_pstTimeoutEvent->stInterval.tv_usec = t_uSec % 1000;	//micro-seconds

	//当前缓存时间(pstTimeCache)+超时常量(stInterval) = 最终超时时间(stTimeout)
	minheap_timeradd(t_pstEventBase->pstTimeCache,&t_pstTimeoutEvent->stInterval,&t_pstTimeoutEvent->stTimeout);
    
	LOG_INFO("register curfd:%d timeout:%d:%d",t_pstTimeoutEvent->iFd, t_pstTimeoutEvent->stTimeout.tv_sec, t_pstTimeoutEvent->stTimeout.tv_usec );

	//为了防止冲突,先把该节点在堆中的对应index删除(第一次调用的时候,由于iHeapIndex的值是-1,所)
	MinheapErase(t_pstEventBase->pstHeapHeader, t_pstTimeoutEvent);

	MinHeapPush(t_pstEventBase->pstHeapHeader, t_pstTimeoutEvent);

	return RET_SUCC;
}

/*
	@FUN:删除已经注册的定时器(删除是根据t_pstTimeoutEvent中的heapindex)
*/
int UnregisterTimerOutEvent(EventBase *t_pstEventBase, TimeoutEvent *t_pstTimeoutEvent){
	if (NULL == t_pstTimeoutEvent || NULL == t_pstEventBase){
		return RET_FAIL;
	}

	if (RET_FAIL == MinheapErase(t_pstEventBase->pstHeapHeader, t_pstTimeoutEvent)){
		LOG_ERROR("unregister timeout event error,fd=%d",t_pstTimeoutEvent->iFd);
		return RET_FAIL;
	}
    LOG_ERROR("unregister timeout event succ,fd=%d",t_pstTimeoutEvent->iFd);

	return RET_SUCC;
}

/*
	@fun:获取下一个超时事件的时间(timeout_next)
	@para
	@para:t_pstEv ---保存超时时间
*/
int GetEPollwaitTimeoutValue(EventBase *t_pstEventBase, struct timeval *t_pstTimeoutEvSave){
	if (NULL == t_pstTimeoutEvSave || NULL == t_pstEventBase || NULL == t_pstEventBase->pstTimeCache){
		return -1;
	}

	int iTimeoutValue = 0;

	//从base的结构中获取cur_time(最近较new的)
	struct timeval *pstCurEv = t_pstEventBase->pstTimeCache;
	
	//从minheap中取top的时间(当然是最小的)
	TimeoutEvent *pstHeapTopTimeout = NULL;

    /*
	CORE
         reactor.c: In function 縂etEPollwaitTimeoutValue?
         > reactor.c:394:2: warning: passing argument 1 of 縈inheapGetTopElement?from incompatible pointer type [enabled by default]
         > min_heap.h:114:16: note: expected 縮truct MinHeapHeader *?but argument is of type 縮truct EventBase *?
    */
	//pstHeapTopTimeout = MinheapGetTopElement(t_pstEventBase);这里core了
    pstHeapTopTimeout = MinheapGetTopElement(t_pstEventBase->pstHeapHeader);
	if (NULL == pstHeapTopTimeout){
		//heap not exist
		//注意这个情况:如果最小堆中没有元素存在,这里取为NULL,epollwait的超时值就是-1,直接返回了(说明此时没有fd处于监听状态)
		iTimeoutValue = -1;
		return iTimeoutValue;
	}

	//if (0 == pstCurEv->tv_sec)
	if (0 == t_pstEventBase->pstTimeCache->tv_sec){
		//没有初始化
		gettimeofday(t_pstEventBase->pstTimeCache, NULL);
	}

	//当前loop中有事件,pstCurEv指向t_pstEventBase->pstTimeCache(即使t_pstEventBase->pstTimeCache刚初始化)
	if (pstHeapTopTimeout !=NULL){
		if (minheap_timercmp(pstCurEv,&pstHeapTopTimeout->stTimeout,<=)){
			//当前时间(pstCurEv)<=HEAPTOP的时间(pstHeapTopTimeout)
			//未超时不做处理
			;
		}
		else{
			//当前时间> HEAPTOP的时间,说明已经超时了,需要处理下超时事件
			//超时时间已经超过当前时间,不需要等待
			t_pstTimeoutEvSave->tv_sec = t_pstTimeoutEvSave->tv_usec = 0;
			return 0;
		}

		//并没有超时(说明当前最小堆顶时间>当前evbase时间)
		//那么下次epoll_wait的超时时间(t_pstTimeoutEvSave)就等于
		//t_pstTimeoutEvSave=当前堆顶时间-当前evbase时间
		minheap_timersub(&pstHeapTopTimeout->stTimeout, pstCurEv, t_pstTimeoutEvSave);
	}
	
	//LOG_INFO("timeout_next: in %d seconds", (int)t_pstTimeoutEvSave->tv_sec);

	return 0;
}

/*
	@FUN:pop最小堆,处理已经超时的事件(处理超时事件)
	@FUN:默认是关闭连接,当然也可以进行其他操作..比如发送回显给客户端
	@para:
*/
int ReactorProcessTimeoutEvent(EventBase *t_pstEventBase){
	if (NULL == t_pstEventBase){
		return RET_FAIL;
	}
	
	//堆顶时间节点
	TimeoutEvent *pstHeapTopNode = NULL;

	//当前时间
	struct timeval *pstCurTimeval = t_pstEventBase->pstTimeCache;
	if (NULL == pstCurTimeval){
		return RET_FAIL;
	}

	while (NULL != (pstHeapTopNode= MinheapGetTopElement(t_pstEventBase->pstHeapHeader)))
	{
		if (minheap_timercmp(&pstHeapTopNode->stTimeout, pstCurTimeval, >)){
			//堆顶时间大于当前时间,说明没有超时事件
			break;
		}

		//LOG_INFO("minheap timeout dealing:%d", pstHeapTopNode->iFd);

		//have timeout events,删除定时器(POP堆顶节点并调整最小堆)
		MinheapPop(t_pstEventBase->pstHeapHeader);
		//调用对应超时事件的处理函数
		if (pstHeapTopNode->pFunTimeoutProcCallback){
			//调用超时回调函数

			//pstHeapTopNode->pFunTimeoutProcCallback(pstHeapTopNode->pstCallbackArg);
			//增加了一个毁掉函数参数
			pstHeapTopNode->pFunTimeoutProcCallback(pstHeapTopNode->iPid, pstHeapTopNode->pstCallbackArg);
		}
	}

	return RET_SUCC;
}

/*
	@FUN:单进程reactor逻辑主循环
*/
void ReactorEventDispatchLoop(EventBase *t_pstEventBase){
	if (NULL == t_pstEventBase){
		return;
	}
	
	int iIndex = 0;	
	struct timeval stTimeSave;
	int nfds;
	int iEpollwaitTimeout = -1;
    
    //printf("%d\n",t_pstEventBase->iStopSign);
	//Begin Loop
	while (!t_pstEventBase->iStopSign)
	{
		//1.根据minheap的top节点的时间确定epoll_wait的超时时间
		//2.epoll_wait的处理逻辑
		//3.遍历最小堆判定有无超时fd,有超时的话处理超时事件
		if (GetEPollwaitTimeoutValue(t_pstEventBase, &stTimeSave) < 0){
			//堆为空
			iEpollwaitTimeout = -1;
		}
		else{
			//堆非空
			//1.堆顶时间小于当前时间---已经有超时的FD,epollwait超时时间就设置为0(直接返回,先处理超时事件)--什么场景?
			//把超时处理函数注释掉,可以有这种效果
			//2.堆顶时间大于当前时间---暂时没有注册超时的FD,epollwait的超时时间就设置为堆顶-当前的时间差
			iEpollwaitTimeout = stTimeSave.tv_sec * 1000 + ((stTimeSave.tv_usec + 999) / 1000);
		}
        
		LOG_INFO("epoll_wait:%d", iEpollwaitTimeout);

		nfds = epoll_wait(t_pstEventBase->iEpfd, t_pstEventBase->pstEpollEvents, EPOLL_SIZE, iEpollwaitTimeout);
		
		//更新当前时间(合适?)
		gettimeofday(t_pstEventBase->pstTimeCache,NULL);
		if (nfds < 0){
			//error
			continue;
		}

		for (iIndex = 0; iIndex < nfds; iIndex++){
			struct epoll_event *pstEV = t_pstEventBase->pstEpollEvents + iIndex;
			if (NULL == pstEV){
				continue;
			}
			int iTriggerFd = pstEV->data.fd;
			int iMask = 0;
			FdEvent *pstFdEventNode = &t_pstEventBase->pstFdEventsArray[iTriggerFd];
			if (NULL == pstFdEventNode){
				continue;
			}
			//check which events happend?
			if (pstEV->events & EPOLLIN){
				iMask |= EVENT_READABLE;
			}
			if ( (pstEV->events & EPOLLOUT) || (pstEV->events & EPOLLHUP) ||(pstEV->events & EPOLLERR)){
				iMask |= EVENT_WRITABLE;
			}

			//触发事件和注册事件比较
			//先前注册的监听事件 & 实际触发事件 & 读/写
			if (pstFdEventNode->iMask & iMask & EVENT_READABLE){
				//调用读回调函数(这里琢磨下)
				/*(t_pstEventBase,iTriggerFd,pstFdEventNode->pstArg)参数要符合回调函数的声明方式
				比如accept的回调函数void ProcessAcceptCallback(EventBase *t_pstEventBase, int t_iFd, void *t_pData)
				t_pstEventBase对应着t_pstEventBase
				t_iFd对应着iTriggerFd
				t_pData对应着pstFdEventNode->pstArg(accept里面是NULL,而recv里面是ConnFd*),为什么呢?因为accept的时候
				fd对应的结构还没创建啊!
				*/
				pstFdEventNode->pFunEventCallbackRead(t_pstEventBase,iTriggerFd,pstFdEventNode->pstArg);
			}
			if (pstFdEventNode->iMask & iMask & EVENT_WRITABLE){
				//调用写回调函数
				pstFdEventNode->pFunEventCallbackWrite(t_pstEventBase, iTriggerFd, pstFdEventNode->pstArg);
			}
		}
	
		//处理超时事件
		ReactorProcessTimeoutEvent(t_pstEventBase);
	}//end of while
	
	return;
}

