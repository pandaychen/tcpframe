#ifndef __REACOR_H
#define __REACOR_H

#include "comm.h"
#include "min_heap.h"

#ifdef __cplusplus
extern "C" {
#endif

	struct __event_base;
	typedef struct __event_base EventBase;

	/*
		事件处理的回调函数声明
		*t_pstEventBase: 事件集合的管理结点
		*t_iFd: 哪个 FD 有事件发生
		*t_pstArg: 回调参数
	*/
	typedef void EventCallbackProc(EventBase *t_pstEventBase, int t_iFd,void *t_pstArg);

	// 和每个 fd 关联的数据结构 (读写)
	struct __fd_rw_event{
		int iMask;		// 当前 fd 期望发生 (监控的) 事件掩码	/* one of AE_(READABLE|WRITABLE) */
		EventCallbackProc *pFunEventCallbackWrite; // 读事件回调函数
		EventCallbackProc *pFunEventCallbackRead;	// 写事件回调函数

		void *pstArg;	// 一个 fd 不可能同时操作读和写, 所以这里参数只设置一个
		//void *pstArgWrite;
	};

	typedef struct __fd_rw_event FdEvent;


	/* State of an file_event based program */
	/* REACTOR 结构, 每个进程一个, 管理头结点 */
	struct __event_base{
		int iSize;
		int iFdEventArraySize;
		int iStopSign;

		int iEpType;
		int iEpfd;
		int iListenFd;	/* 监听的 fd*/
		int iClientCount;	/* 客户端数量 */

		struct epoll_event *pstEpollEvents;	/*EPOLL 事件 */	// 这个是 epoll 调用相关

		struct timeval *pstTimeCache;	/* 缓存当前时间，防止每次调用太多系统函数 */
		/* 作为定时器的判断时间?*/

		/*
		pstHeapHeader 和 pstFdEventsArray 是两个大数据结构的管理指针节点
		前者指向所有的连接的 fd
		后者指向所有的连接 fd 的超时事件集合
		*/

		MinHeapHeader *pstHeapHeader;	/* 超时事件小根堆 */

		//malloc 大小为支持的最大客户端连接数
		FdEvent *pstFdEventsArray;		/*fd 处理事件数组, 每个 fd 对应一个, 以 fd 作为 key*/
	};


	/*
	这个结构体可以和 file_event 合并的
	对应着 libevent 的 event
	意义是描述了每一条新建 TCP 连接
	*/
	struct __single_conn_fd{
		EventBase *pstEventBase;	/* 指向 base 的位置 */

		// 每个连接都需要一个超时事件关联 (指向 heap 中的某个节点)
		TimeoutEvent *pstTimeoutEvent;	/* 对应超时事件 */

		//fd 也作为一个指针 --> 在 base 的 fd 数组下标中
		int iFd;
		int iPacketLen;		/* 包长度 */	// 每次的包长度, 接收完一个包之后清 0
		char *pszRecvBuf;	/* 存储接收数据 */
		char *pszSendBuf;	/* 存储发送数据 */

		int iRecvTotalLen;	/* 接收 Buffer 总长度 */
		int iRecvCurLen;	/* 已接收数据长度 */
		int iRecvPos;

		int iSendTotalLen;	/* 发送 Buffer 总长度 */
		int iSendCurLen;	/* 发送数据长度 */
		int iSendPos;		/* 已发送数据位置 */
		int iSendPacketLen;

		/*
			这里的 iSendTotalLen 和 iRecvTotalLen 和 iPacketLen/iSendPacketLen 有什么区别呢?
			由于是动态数组 (非静态), 所以必须用 iSendTotalLen/iRecvTotalLen 来标识动态数组的长度 (用来判断是否需要扩容)
			而 TCP 是流式协议, 需要指明一个报文的长度, 即用 iSendPacketLen/iPacketLen 来标识
			实际上长连接的场景下, iSendTotalLen 是可以包含 N 多个 iSendPacketLen,iRecvTotalLen 是可以包含 N 个 iPacketLen 的
			这里也要看用法了.
		*/

		char szIpaddr[IP_LEN];
		int iPort;
	};

	typedef struct __single_conn_fd ConnFd;


	/*
	EVENTBASE
	*/
	EventBase * EventBaseCreate(int t_iFd, int t_iMaxEventSize);
	EventBase * EventBaseNew(int t_iFd, int t_iMaxEventSize);
	void EventBaseDelete(EventBase *t_pstEventBase);

    //
    EventBase * __CreateNewEventBase(int t_iEventSize);

	/**
	* EventAdd
	* @param t_pstEventBase
	* @param t_iFd  操作的 fd
	* @param t_iEventMask  注册到监听该 fd 的触发事件 epoll 掩码
	* @param t_pEventCallbackProc 注册事件发生的回调函数
	* @param t_pstCallbackArg 回调函数参数
	*/
	int EventAdd(EventBase *t_pstEventBase, int t_iFd, int t_iEventMask, EventCallbackProc *t_pEventCallbackProc, void *t_pstCallbackArg);
	int EventDel(EventBase *t_pstEventBase, int t_iFd, int t_iEventMask);
	//void EventDispatchLoop(EventBase *t_pstEventBase);
	void ReactorEventDispatchLoop(EventBase *t_pstEventBase);

	/*
	对于每个新建的连接, 都向 EVENTBASE 指向的 heap 中插入一个节点 (当然要插到合适的位置上)
	@param t_pstEventBase Event 结构
	@param t_pstTimeoutEvent 超时处理参数 (1. 超时时间计时, 2. 超时处理回调函数 3. 回调函数参数)
	*/
	int RegisterTimerOutEvent(EventBase *t_pstEventBase, TimeoutEvent *t_pstTimeoutEvent, int t_uSec);
	int UnregisterTimerOutEvent(EventBase *t_pstEventBase, TimeoutEvent *t_pstTimeoutEvent);

	/*
	FdFileEventInit 初始化 eventbase 中的 fd 数组 (以 fd 作为下标)
	param	t_pstEventBase
	param  t_iFd	需要操作的 fd
	*/
	int FdFileEventInit(EventBase *t_pstEventBase, int t_iFd);



#ifdef __cplusplus
}
#endif

#endif
