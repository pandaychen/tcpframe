/*
* File name:    min_heap.cpp
* @author:      
* @version:     V1.0   2014/09/14
* Description: 	堆操作，根据redis开源版本修改
* Log:
*/

//#include "../include/comm.h"
//#include "../include/min_heap.h"
//#include "../include/server_log.h"

#include "comm.h"
#include "min_heap.h"
#include "server_log.h"


/*
	初始化新malloc的最小堆节点
*/
int MinheapTimeoutEventInit(TimeoutEvent *t_pstTimeoutEvent, TimeoutProcCallback *t_pfTimeoutProcCallback, void *t_pArgCallback, int t_iPid, int t_iFd){
	//
	if (NULL == t_pstTimeoutEvent){
		LOG_ERROR("minheap new node null");
		return RET_FAIL;
	}
    t_pstTimeoutEvent->iHeapIndex=-1;	//新插入的节点为-1(其实是最小堆的最后一个位置)

	//注册超时的回调函数和参数
    t_pstTimeoutEvent->pFunTimeoutProcCallback=t_pfTimeoutProcCallback;
    t_pstTimeoutEvent->pstCallbackArg=t_pArgCallback;
	t_pstTimeoutEvent->iFd = t_iFd;
	t_pstTimeoutEvent->iPid = t_iPid;
}

/**
* [__MinheapNodeCmpGreater 比较堆的两个节点大小]
* @param  t_pstTimeEventFirst [description]
* @param  t_pstTimeEventSecond   [description]
* @return         [description]
*/
int __MinheapNodeCmpGreater(TimeoutEvent *t_pstTimeEventFirst, TimeoutEvent *t_pstTimeEventSecond){
	if (NULL == t_pstTimeEventFirst || NULL == t_pstTimeEventSecond){
		LOG_ERROR("minheap cmp node null");
		exit(1);
	}
	/*
		最小堆
		if first>second,return 1
		elif first<=second,return 0
	*/
	return minheap_timercmp(&t_pstTimeEventFirst->stTimeout, &t_pstTimeEventSecond->stTimeout,>);
}

/*
	初始化最小堆结构
*/
int MinheapCreate(MinHeapHeader *t_pstMinHeapHeader){
	if (NULL == t_pstMinHeapHeader){
		LOG_ERROR("minheap create ptr null");
		return RET_FAIL;
	}

	t_pstMinHeapHeader->iCurPid = getpid();
	t_pstMinHeapHeader->iNodeCapacity = 0;	//
	t_pstMinHeapHeader->iNodeUsed = 0;

	//二维指针的套路,ppstTimeoutEventHeader指向实际的动态最小堆结构
	t_pstMinHeapHeader->ppstTimeoutEventHeader = NULL;

	return RET_SUCC;
}

/*
	注销最小堆
*/
int MinheapDestory(MinHeapHeader *t_pstMinHeapHeader){
	if (NULL == t_pstMinHeapHeader){
		return RET_FAIL;
	}
	if (NULL != t_pstMinHeapHeader->ppstTimeoutEventHeader){
		//for (){
		//需不需要把每个节点都先free?
		//}
		free(t_pstMinHeapHeader->ppstTimeoutEventHeader);
	}
	return RET_SUCC;
}


/*
	
*/
int MinheapIsEmpty(MinHeapHeader *t_pstMinHeapHeader){
	if (t_pstMinHeapHeader == NULL){
		LOG_ERROR("minheap header null");
		return RET_FAIL;
	}
	/*
		if heap is empty,return 1
		else ,return 0
	*/
	return 0u == t_pstMinHeapHeader->iNodeUsed;
}


int MinheapIsTop(TimeoutEvent *t_pstTimeoutEvent){
	if (NULL == t_pstTimeoutEvent){
		LOG_ERROR("minheap node null");
		return RET_FAIL;
	}
	/*
		if node is top,then index == 0,return 1
		else,return 0
	*/
	return 0==t_pstTimeoutEvent->iHeapIndex;
}


int MinheapGetCurSize(MinHeapHeader *t_pstMinHeapHeader){
	if (NULL == t_pstMinHeapHeader){
		LOG_ERROR("minheap header null");
		return RET_FAIL;
	}
	
	return t_pstMinHeapHeader->iNodeUsed;
}

/*
	@fun:返回当前的堆root指针
	@para:
	@return NULL or 堆顶元素(下标为0的数组--也就是指针数组的第一个位置指向的节点)
*/
TimeoutEvent * MinheapGetTopElement(MinHeapHeader *t_pstMinHeapHeader){
	if (NULL == t_pstMinHeapHeader){
		LOG_ERROR("minheap header null");
		return NULL;
	}

    if(NULL == t_pstMinHeapHeader->ppstTimeoutEventHeader){
		LOG_ERROR("minheap node header null");
        return NULL;
    }
    
	//return t_pstMinHeapHeader->ppstTimeoutEventHeader ? *t_pstMinHeapHeader->ppstTimeoutEventHeader : 0;
	
    //core!!!!!!
    //return t_pstMinHeapHeader->ppstTimeoutEventHeader ? *(t_pstMinHeapHeader->ppstTimeoutEventHeader) : NULL;
    return t_pstMinHeapHeader->iNodeUsed ? *t_pstMinHeapHeader->ppstTimeoutEventHeader : 0;
}



/*
@fun:扩容堆的指针数组
@para:t_pstMinHeapHeader	
@para:t_iCurrentSize	当前堆指针数组的占用个数
@return:
*/
int MinheapReserveSize(MinHeapHeader *t_pstMinHeapHeader,int t_iCurrentSize){
	//capacity vs.
	//current nodes
	if (NULL == t_pstMinHeapHeader || t_iCurrentSize <0){
		LOG_ERROR("minheap node null or current size illegal");
		return RET_FAIL;
	}

	//如果当前个数不超过总容量
	if (t_iCurrentSize <= t_pstMinHeapHeader->iNodeCapacity){
		return RET_SUCC;
	}
	else{
		//need resize需要扩容
		TimeoutEvent **pstNewAddr = NULL;
		int iNewSize = t_pstMinHeapHeader->iNodeCapacity ? t_pstMinHeapHeader->iNodeCapacity * 2 : MINHEAP_INIT_SIZE;
		//t_pstMinHeapHeader->iNodeCapacity的初始值是0
		if (iNewSize < t_iCurrentSize){
			iNewSize = t_iCurrentSize;
		}
		if (!(pstNewAddr =(TimeoutEvent **)realloc(t_pstMinHeapHeader->ppstTimeoutEventHeader,iNewSize*sizeof(TimeoutEvent *)))){
			LOG_ERROR("minheap realloc size error");
			return RET_FAIL;
		}
		/*
		if (!(p = (struct time_event**)realloc(s->p, a * sizeof *p)))
		{
			return -1;
		}
		*/
		//renew
		t_pstMinHeapHeader->iNodeCapacity = iNewSize;
		//t_pstMinHeapHeader->iNodeUsed = t_iCurrentSize;
		//iNodeUsed这个值在后面的逻辑会更新,这里不更新了

		t_pstMinHeapHeader->ppstTimeoutEventHeader = pstNewAddr;
		LOG_INFO("cur minheap status:[cur:%d,total:%d]", t_pstMinHeapHeader->iNodeUsed, t_pstMinHeapHeader->iNodeCapacity);
	}
	return RET_SUCC;
}

/*
	@fun:向最小堆插入元素(插入的元素初始在最小堆的最后一个位置,然后调整到堆顶位置)
	@para1:
	@para2:t_pstTimeoutEvent--->指向需要新加入堆的节点
	@return NULL or 堆顶元素(下标为0的数组--也就是指针数组的第一个位置指向的节点)
*/
int MinHeapPush(MinHeapHeader *t_pstMinHeapHeader, TimeoutEvent *t_pstTimeoutEvent){
	if (NULL == t_pstMinHeapHeader || NULL == t_pstTimeoutEvent){
		LOG_ERROR("minheap push node error");
		return RET_FAIL;
	}
	
	int iNewSize = t_pstMinHeapHeader->iNodeUsed + 1;
	//检查堆的指针数组是否需要扩容
	if (RET_FAIL == MinheapReserveSize(t_pstMinHeapHeader, iNewSize)){
		return RET_FAIL;
	}

	//由于下标从0开始,所以待插入位置就是t_pstMinHeapHeader->iNodeUsed,插入成功t_pstMinHeapHeader->iNodeUsed需要+1
	if (RET_FAIL == __MinheapShiftUp(t_pstMinHeapHeader,t_pstMinHeapHeader->iNodeUsed,t_pstTimeoutEvent)){
		LOG_ERROR("minheap insert new node error");
		return RET_FAIL;
	}
	
	//min_heap_shift_up_(s, s->n++, e);

	//更新节点数
	t_pstMinHeapHeader->iNodeUsed += 1;
	return RET_SUCC;
}

/*
@fun:弹出最小堆顶的元素,并返回它
@para1:t_pstTimeoutEvent--
@return NULL or 堆顶元素(下标为0的数组--也就是指针数组的第一个位置指向的节点)
*/
TimeoutEvent *MinheapPop(MinHeapHeader *t_pstMinHeapHeader){
	//
	if (NULL == t_pstMinHeapHeader){
		LOG_ERROR("minheap pop error");
		return NULL;
	}

	//非空堆
	if (t_pstMinHeapHeader->iNodeUsed != 0){
		//返回第0位置的元素
		TimeoutEvent *pstTimeoutEventTop = *(t_pstMinHeapHeader->ppstTimeoutEventHeader);
		//最后一个位置的节点
		//当前的最小堆元素index是从[0，1，2，3，.....,iNodeUsed-1]
		//比如5个节点,下标从0-4
		TimeoutEvent *pstTimeoutEventLast = t_pstMinHeapHeader->ppstTimeoutEventHeader[t_pstMinHeapHeader->iNodeUsed - 1];
		if (pstTimeoutEventLast == NULL){
			LOG_ERROR("minheap get last element error");
			return NULL;
		}
		if (RET_SUCC == __MinheapShiftDown(t_pstMinHeapHeader, 0u, pstTimeoutEventLast)){
			t_pstMinHeapHeader->iNodeUsed -= 1;	//更新当前节点数目
			pstTimeoutEventTop->iHeapIndex = -1;
			return pstTimeoutEventTop;
		}
	}
	//最小堆为空,返回NULL
	return NULL;
}

/*
@fun:MinheapErase---删除指定位置的元素
@para1:t_pstMinHeapHeader
@para2:t_pstTimeoutEventDel	---需要删除的节点(index)
@return NULL or 堆顶元素(下标为0的数组--也就是指针数组的第一个位置指向的节点)
*/
int MinheapErase(MinHeapHeader *t_pstMinHeapHeader, TimeoutEvent *t_pstTimeoutEventDel){
	if (NULL == t_pstMinHeapHeader || NULL == t_pstTimeoutEventDel){
		LOG_ERROR("minheap erase error");
		return RET_FAIL;
	}
	
    LOG_INFO("%d",t_pstTimeoutEventDel->iHeapIndex);
        
	//新申请的节点index值是-1,这里erase会直接返回(在第一个逻辑中)
	if (-1 != t_pstTimeoutEventDel->iHeapIndex){
		//在堆中(是堆中的节点)

		//最后一个节点
		TimeoutEvent *pstTimeoutEventLast = t_pstMinHeapHeader->ppstTimeoutEventHeader[t_pstMinHeapHeader->iNodeUsed - 1];
		if (NULL == pstTimeoutEventLast){
			LOG_ERROR("minheap erase get last element error");
			return RET_FAIL;
		}
		/* 
		we replace e with the last element in the heap.  We might need to
		shift it upward if it is less than its parent, or downward if it is
		greater than one or both its children. Since the children are known
		to be less than the parent, it can't need to shift both up and
		down. 
		*/

		//取待删除节点的parent节点(调整)
		unsigned int uParentIndex = (t_pstTimeoutEventDel->iHeapIndex - 1) / 2;
		if (t_pstTimeoutEventDel->iHeapIndex > 0/*不是根节点*/ && \
			__MinheapNodeCmpGreater(t_pstMinHeapHeader->ppstTimeoutEventHeader[uParentIndex], pstTimeoutEventLast)){
			//上移,这里子树的根还是原来的root,新插入位置是t_pstTimeoutEventDel->iHeapIndex,最后一个节点是pstTimeoutEventLast
			if (RET_SUCC == __MinheapShiftUp(t_pstMinHeapHeader, t_pstTimeoutEventDel->iHeapIndex, pstTimeoutEventLast)){
				t_pstMinHeapHeader->iNodeUsed -= 1;
				pstTimeoutEventLast->iHeapIndex = -1;
				return RET_SUCC;
			}
		}
		else{
			//想想这里e->index > 0一定是成立的,为什么?
			//下移,这里子树的根是t_pstTimeoutEventDel->iHeapIndex,最后一个节点是pstTimeoutEventLast,按照Down方法调整大小
			if (RET_SUCC == __MinheapShiftDown(t_pstMinHeapHeader, t_pstTimeoutEventDel->iHeapIndex, pstTimeoutEventLast)){
				t_pstMinHeapHeader->iNodeUsed -= 1;
				t_pstTimeoutEventDel->iHeapIndex = -1;
				return RET_SUCC;
			}
		}
	}

	return RET_FAIL;
}

/*
@fun:从最底层调整至堆,适合于添加元素的情况
@para:t_uHoleIndex	
@return NULL or 堆顶元素(下标为0的数组--也就是指针数组的第一个位置指向的节点)
*/

/*
int __MinheapShiftUp(MinHeapHeader* t_pstMinHeapHeader, unsigned int t_uHoleIndex, TimeoutEvent* t_pstTimeoutEvent){
	if (NULL == t_pstMinHeapHeader || NULL == t_pstTimeoutEvent){
		return RET_FAIL;
	}
	
	unsigned int uParentIndex = (t_uHoleIndex - 1) / 2;
	while (t_uHoleIndex && __MinheapNodeCmpGreater() ){
		//调整到root并且
		
	}


	return RET_SUCC;
}

*/

/*
	@__MinheapShiftUp: 新插入节点总是在最后一个位置,自下而上进行调整
	@para1:t_uHoleIndex--> 新插入节点的位置(比如第7个元素插入第6号位置)
	@para2:t_pstTimeoutEvent -->
	@info: p:index child:2p+1,2p+2
	@return:
*/
int __MinheapShiftUp(MinHeapHeader* t_pstMinHeapHeader, unsigned int t_uHoleIndex, TimeoutEvent* t_pstTimeoutEvent){
	if (NULL == t_pstMinHeapHeader || NULL == t_pstTimeoutEvent){
		return RET_FAIL;
	}
	

	/*

	|----|----|----|--------|			0
	|  0	1	 2	 3(NEW)	|	=>	  /   \
	|----|----|----|--------|		 1		2
	/\		/\
	3  4	   5  6
	*/

	unsigned int uParentIndex = (t_uHoleIndex - 1) / 2;
	
	//一直调整到根或者新插入节点比父亲节点要大退出循环
	//t_pstTimeoutEvent这个指针始终指向最后一个新插入的节点(但是其index未定-初始化是最后一个位置,这个函数
	//的目的就是要为这个节点找到合适的index)
	while (t_uHoleIndex !=0 && __MinheapNodeCmpGreater(t_pstMinHeapHeader->ppstTimeoutEventHeader[uParentIndex],t_pstTimeoutEvent))
	{
		//需要交换父子节点
		//让最后的位置指向其父亲节点的位置
		t_pstMinHeapHeader->ppstTimeoutEventHeader[t_uHoleIndex] = t_pstMinHeapHeader->ppstTimeoutEventHeader[uParentIndex];
		//此时t_pstMinHeapHeader->ppstTimeoutEventHeader[t_uHoleIndex]已经指向它的孩子节点了
		t_pstMinHeapHeader->ppstTimeoutEventHeader[t_uHoleIndex]->iHeapIndex = t_uHoleIndex;

		/*WRONG!
		t_pstTimeoutEvent[uParentIndex] = t_pstTimeoutEvent;
		t_pstTimeoutEvent[uParentIndex]->iHeapIndex = t_uHoleIndex;
		*/
		//继续向上判断
		t_uHoleIndex = uParentIndex;
		uParentIndex = (t_uHoleIndex - 1) / 2;
	}

	//已经为t_pstTimeoutEvent找到位置(或)到达了root位置
	//将新节点t_pstTimeoutEvent放入最终的位置1.数组指针 2.index
	t_pstMinHeapHeader->ppstTimeoutEventHeader[t_uHoleIndex] = t_pstTimeoutEvent;
	t_pstMinHeapHeader->ppstTimeoutEventHeader[t_uHoleIndex]->iHeapIndex = t_uHoleIndex;
	return RET_SUCC;
}



/*
	@fun:删除的逻辑是把当前堆的最后一个元素(可能是左孩子/或者右孩子),替换到删除位置(在这里有两种case,一是POP堆顶/
	二是待删除位置),然后看最后一个元素和当前删除位置左右孩子比较,是否满足堆关系,如果满足就直接替换,如果不满足就回溯移动,
	直到为最后一个元素找到合适的插入位置
	t_uHoleIndex-->指向待删除节点的index(在堆算法:1.堆顶 2.待删除节点)
	t_pstTimeoutEvent -->指向替换待删除节点位置的节点(可能需要调整),一般为最后一个节点
	
*/
int __MinheapShiftDown(MinHeapHeader* t_pstMinHeapHeader, unsigned int t_uHoleIndex, TimeoutEvent* t_pstTimeoutEvent){

	if (NULL == t_pstMinHeapHeader || NULL == t_pstTimeoutEvent){
		return RET_FAIL;
	}

	//先取待删除节点的右孩子(再和左孩子比较大小)
	unsigned int uMinChildIndex = 2 * (t_uHoleIndex + 1);

	//如果右孩子节点存在
	
	//从待删除的节点一直调整直到最后一个节点为止
	while (uMinChildIndex <= t_pstMinHeapHeader->iNodeUsed){
		/*
		==的优先级最高,其次是||.最后是-=
		min_child -= min_child == s->n || min_heap_elem_greater(s->p[min_child], s->p[min_child - 1]);
		*/
		if (uMinChildIndex == t_pstMinHeapHeader->iNodeUsed){
			//说明待删除节点的右孩子就是最后一个节点,只需要比较他的前一个节点(即待删除节点的左孩子)
			uMinChildIndex -= 1;
		}
		else{
			//不是最后一个节点
			//最后一个节点不是右孩子,其实就是右孩子不存在(那就取左孩子)
			if (__MinheapNodeCmpGreater(t_pstMinHeapHeader->ppstTimeoutEventHeader[uMinChildIndex], t_pstMinHeapHeader->ppstTimeoutEventHeader[uMinChildIndex - 1])){
				uMinChildIndex -= 1;
			}
		}

		//如果最后一个节点小于
		if (!__MinheapNodeCmpGreater(t_pstTimeoutEvent /*取代的parent节点*/, t_pstMinHeapHeader->ppstTimeoutEventHeader[uMinChildIndex]/*孩子节点*/)){
			//如果待删除节点比最小的孩子节点还小,那么就不用下溯了
			break;
		}

		if (__MinheapNodeCmpGreater(t_pstMinHeapHeader->ppstTimeoutEventHeader[uMinChildIndex], t_pstTimeoutEvent)){
			//因为删除算法要求t_pstTimeoutEvent(最后一个节点)取代删除节点的位置
			//所以当这个节点的值小于原来待删除节点的左右孩子中的最小值时,下溯成功,直接退出循环
			break;
		}

		//最后一个节点比左右孩子中的最小节点要大,还是要继续下溯
		t_pstMinHeapHeader->ppstTimeoutEventHeader[t_uHoleIndex] = t_pstMinHeapHeader->ppstTimeoutEventHeader[uMinChildIndex];
		//相对较小的孩子节点上移

		//注意,这一步t_pstMinHeapHeader->ppstTimeoutEventHeader[t_uHoleIndex]的值已经是新的了,不是旧的
		t_pstMinHeapHeader->ppstTimeoutEventHeader[t_uHoleIndex]->iHeapIndex = t_uHoleIndex;
		//修改index

		//上面这两步是交换t_uHoleIndex和它的最小孩子节点(让大的节点上移,t_uHoleIndex下移)

		//
		t_uHoleIndex = uMinChildIndex;
		uMinChildIndex = 2 * (t_uHoleIndex + 1);
	}
	
	//为最后一个元素找到了合适的插入位置
	t_pstMinHeapHeader->ppstTimeoutEventHeader[t_uHoleIndex] = t_pstTimeoutEvent;
	
	//t_pstTimeoutEvent->iHeapIndex = t_uHoleIndex;
	t_pstMinHeapHeader->ppstTimeoutEventHeader[t_uHoleIndex]->iHeapIndex = t_uHoleIndex;
	return RET_SUCC;
}


/*
int main(void){

	struct timeval timea;
	struct timeval timeb;

	gettimeofday(&timea,NULL);

	sleep(3);

	gettimeofday(&timeb);

    printf("a:%u,%d;\tb:%u,%d\n",timea.tv_sec,timea.tv_usec,timeb.tv_sec,timeb.tv_usec);
	printf("a,b < compare:%d\n", minheap_timercmp(&timea, &timeb, <));
    printf("a,b > compare:%d\n",minheap_timercmp(&timea, &timeb, >));

    //return higher
    printf("a,b = compare:%d\n",minheap_timercmp(&timea, &timeb, =));



	return 0;
}
*/
