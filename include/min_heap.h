#ifndef __MIN_HEAP_H
#define __MIN_HEAP_H

#include <time.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>

#ifdef __cplusplus
extern "C" {
#endif

	//typedef void TimeoutProcCallback(void *t_pstArg);

	// 增加一个回调函数参数
	typedef void TimeoutProcCallback(int t_iPid,void *t_pstArg);

	// 是一个数组构建的堆
	struct __timeout_event{
		//
		int iPid;
		int iFd;				// 这个超时节点属于哪个 fd
		int iHeapIndex;			// 该超时结构体在 heap 中的下标 (key)
		struct timeval stTimeout;		//save timeout+timestart value
		struct timeval stInterval;		//save timeout value
		TimeoutProcCallback *pFunTimeoutProcCallback;	// 超时回调函数
		void *pstCallbackArg;		// 回调参数
	};

	// 每一个堆的元素都是一个 TimeoutEvent, 每一个堆数组元素都是指针, 指向一个 TimeoutEvent 节点
	typedef struct __timeout_event TimeoutEvent;


	struct __min_heap_managenode{
		// 超时堆管理结点
		TimeoutEvent **ppstTimeoutEventHeader;	// 指向堆的结构
		int iNodeCapacity;
		int iNodeUsed;
		int	iCurPid;		// 创建 Heap 的进程
	};

	typedef struct __min_heap_managenode MinHeapHeader;


	/*
		操作时间比较的宏
	*/

	/*
	* quote from libevent
	Return true iff the tvp is related to uvp according to the relational
		* operator cmp.  Recognized values for cmp are ==, <=, <,>=, and >. */
#define	minheap_timercmp(a_vp, b_vp, cmp_symbol)				\
	(((a_vp)->tv_sec == (b_vp)->tv_sec) ? \
	((a_vp)->tv_usec cmp_symbol (b_vp)->tv_usec) : \
	((a_vp)->tv_sec cmp_symbol (b_vp)->tv_sec))

//c=a+b
#define minheap_timeradd(a_vp, b_vp, c_vp)					\
	do { \
	(c_vp)->tv_sec = (a_vp)->tv_sec + (b_vp)->tv_sec;		\
	(c_vp)->tv_usec = (a_vp)->tv_usec + (b_vp)->tv_usec;       \
	if ((c_vp)->tv_usec >= 1000000) {	\
		(c_vp)->tv_sec++;				\
		(c_vp)->tv_usec -= 1000000;			\
	}							\
	} while (0)

//c=a-b
#define	minheap_timersub(a_vp, b_vp, c_vp)					\
	do { \
	(c_vp)->tv_sec = (a_vp)->tv_sec - (b_vp)->tv_sec;		\
	(c_vp)->tv_usec = (a_vp)->tv_usec - (b_vp)->tv_usec;	\
		if ((c_vp)->tv_usec < 0) { \
				(c_vp)->tv_sec--;				\
				(c_vp)->tv_usec += 1000000;			\
		}							\
	} while (0)


	/*
		操作堆的函数
		堆的元素是一个 TimeoutEvent
	*/

	/*
	@
	@
	@para3: 回调函数参数
	*/
	int MinheapTimeoutEventInit(TimeoutEvent *t_pstTimeoutEvent, TimeoutProcCallback *t_pfTimeoutProcCallback, void *t_pArgCallback,int t_iPid,int t_iFd);

	int MinheapCreate(MinHeapHeader *t_pstMinHeapHeader);

	int MinheapDestory(MinHeapHeader *t_pstMinHeapHeader);

	//PUSH FROM THE LAST CHILD
	int MinHeapPush(MinHeapHeader *t_pstMinHeapHeader, TimeoutEvent *t_pstTimeoutEvent);
	//POP FROM THE ROOT,and save it
	TimeoutEvent * MinheapPop(MinHeapHeader *t_pstMinHeapHeader);
	// 删除指定位置的元素
	int MinheapErase(MinHeapHeader *t_pstMinHeapHeader, TimeoutEvent *t_pstTimeoutEvent);
	//

	// 堆的平衡函数 (IMP)
	int __MinheapShiftUp(MinHeapHeader* t_pstMinHeapHeader, unsigned t_uHoleIndex, TimeoutEvent* t_pstTimeoutEvent);
	int __MinheapShiftDown(MinHeapHeader* t_pstMinHeapHeader, unsigned t_uHoleIndex, TimeoutEvent* t_pstTimeoutEvent);

	// 比较两个节点的超时时间大小, 小的在上, 大的在下
	int __MinheapNodeCmpGreater(TimeoutEvent *t_pstTimeEventFirst, TimeoutEvent *t_pstTimeEventSecond);

	// 判断某个给定的 Node 是不是堆顶
	int MinheapIsTop(TimeoutEvent *t_pstTimeoutEvent);
	int MinheapIsEmpty(MinHeapHeader *t_pstMinHeapHeader);
	int MinheapGetCurSize(MinHeapHeader *t_pstMinHeapHeader);

	// 得到当前堆顶的元素, 但不删除
	TimeoutEvent *MinheapGetTopElement(MinHeapHeader *t_pstMinHeapHeader);


	//minheap 的指针数组部分扩容
	int MinheapReserveSize(MinHeapHeader *t_pstMinHeapHeader, int t_iCurrentSize);



#ifdef __cplusplus
}
#endif
#endif
