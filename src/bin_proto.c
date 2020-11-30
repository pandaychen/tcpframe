/*
* File name:	bin_proto.c
* @author:  pandaychen
* @version:
* Description:	协议操作函数
* Log:
*/

#include "bin_proto.h"
#include "socket_util.h"

/*
	@fun: 针对流式的 TCP 协议, 如何便捷化处理?
	@fun: 从应用层的 buffer 中创建二进制协议操作的管理节点
	struct __packet_pack{
	//
	UN_BYTE *pBuffer;	// 指向应用层 buffer 头部
	int32_t iPos;
	int32_t iTailer;	// 包尾部
	int32_t iHeader;	// 包头
	int32_t iTotal;		// 操作包长度 (正常应该小于等于 buffer 的长度, 要先判断)
	};
	调用场景: 将一个完整的 TCP 包转换成二进制 pack(t_pBuffer 是一个字符串数组指针?)

	IMPORTANT: 只是 malloc 一个 pack 结构体, 所有的操作都在 sendbuf 或者 recvbuf 上面进行!!!!!!
*/
PacketPack *__CreatePackFromBuffer(const char *t_pBuffer, int32_t t_iLen){
	if (NULL == t_pBuffer){
		return NULL;
	}
	int iLenNworder = 0;
	PacketPack *pstPacketPack = (PacketPack *)malloc(sizeof(PacketPack));
	if (NULL == pstPacketPack){
		return NULL;
	}
	memset(pstPacketPack, 0, sizeof(PacketPack));

	//8bytes
	/*
	1     4    4    4    n      1
	SOH  len  cmd  seq  body    EOT

	*/

	//pstPacketPack->pBuffer = (UN_BYTE)t_pBuffer;	//may core
	pstPacketPack->pBuffer = (UN_BYTE *)t_pBuffer;
	pstPacketPack->iHeader = 0;
	pstPacketPack->iPos = PACKET_BODY_POS;
	pstPacketPack->iTailer = PACKET_BODY_POS;	// 有疑问
	pstPacketPack->iTotal = t_iLen;

	//iTotal 和 iTailer 的区别是:
	//iTotal 是一个限制 (上限),iTailer 是一个动态增长的 index, 初始化指向 PACKET_BODY_POS
	// 当有数据复制时, iTailer 需要加上数据的长度, 但是不能超过 iTotal
	// 所以第 49 行和第 59 行就是解决了这个问题

	// 初始化 SOH 和 EOT
	pstPacketPack->pBuffer[PACKET_SOH_POS] = PROTO_SOH;
	pstPacketPack->pBuffer[pstPacketPack->iTailer] = PROTO_EOT;	// 有异议

	iLenNworder = htonl(PACKET_MIN_LEN);
    // warning: call to __builtin___memcpy_chk will always overflow destination buffer
	//memcpy(&pstPacketPack[PACKET_LEN_POS], (void *)&iLenNworder, sizeof(unsigned int));
    memcpy(&pstPacketPack->pBuffer[PACKET_LEN_POS], (void *)&iLenNworder, sizeof(unsigned int));

	return pstPacketPack;
}

/*
	@FUN: 释放 pack 空间
	@para:
*/

void __DestoryPack(PacketPack *t_pstPacketPack){
	if (NULL == t_pstPacketPack){
		return;
	}

	free(t_pstPacketPack);
	t_pstPacketPack = NULL;
	return;
}

/*
	@FUN: 使用场景 (对一个 pack, 设置 packet 的长度)
*/
const char *PacketPack2String(PacketPack *t_pstPacketPack){
	// 给定某个 packetpack, 返回应用层 buff 指针
	if (NULL ==t_pstPacketPack){
		return NULL;
	}

	UN_4BYTE uLen = 0;

	// 更新 SOH 和 EOT
	t_pstPacketPack->pBuffer[PACKET_SOH_POS] = PROTO_SOH;
	t_pstPacketPack->pBuffer[t_pstPacketPack->iTailer] = PROTO_EOT;

	uLen = htonl(t_pstPacketPack->iTailer + 1);	//iTailer=21,len=22
	memcpy(&t_pstPacketPack->pBuffer[PACKET_LEN_POS],&uLen,sizeof(UN_4BYTE));
	return (char *)t_pstPacketPack->pBuffer;
}

/*
	@FUN:
*/
PacketPack *String2PacketPack(const char *t_pbuff, int32_t t_iLen){
	if (NULL == t_pbuff || t_iLen<0){
		return NULL;
	}

	PacketPack *pstPacketPack = (PacketPack *)malloc(sizeof(PacketPack));
	if (NULL == pstPacketPack){
		return NULL;
	}
	//pstPacketPack->pBuffer = (UN_BYTE)t_pbuff;	// 这种同长度有无符号指针乱指是没问题的
	pstPacketPack->pBuffer = (char *)t_pbuff;
	pstPacketPack->iHeader = 0;

	//t_iLen 是数据包长度, iTailer 当然要减 1(数组最后一个位置)
	pstPacketPack->iTailer = t_iLen - 1;
	pstPacketPack->iPos = PACKET_BODY_POS;	// 指向数据部分开始
	pstPacketPack->iTotal = t_iLen;
	//printf("$$%0x,%0x\n",pstPacketPack->pBuffer[PACKET_SOH_POS],pstPacketPack->pBuffer[pstPacketPack->iTailer]);	//coredump
	//printf("$$d,%d,%d,%d\n",pstPacketPack->iHeader,pstPacketPack->iTailer,pstPacketPack->iPos,pstPacketPack->iTotal);
	//printf("$$%c,%c\n",pstPacketPack->pBuffer[13],pstPacketPack->pBuffer[22]);

	//printf("%d,%d,%d,%d\n", pstPacketPack->iHeader,pstPacketPack->iTailer,pstPacketPack->iPos ,pstPacketPack->iTotal);
	//printf("%c,%c\n",pstPacketPack->pBuffer[PACKET_SOH_POS],pstPacketPack->pBuffer[pstPacketPack->iTailer]);

	// 检查 t_pBuff 所指向的内容是不是正确协议
	if (pstPacketPack->pBuffer[PACKET_SOH_POS] != PROTO_SOH || \
		pstPacketPack->pBuffer[pstPacketPack->iTailer] != PROTO_EOT){
		LOG_ERROR("[%d]invalid packet",getpid());
		free(pstPacketPack);
		return NULL;
	}

	return pstPacketPack;
}


/*
	@FUN: 设置网络序的包长度 (设置到 buff 中)
	@ret: 是否判断错误 (感觉要的, 这里暂时不判断了)
*/
void SetPacketLen(PacketPack *t_pstPacketPack, UN_4BYTE t_uPacketLen){
	if (NULL == t_pstPacketPack){
		return;
	}
	t_uPacketLen = htonl(t_uPacketLen);
	memcpy(&t_pstPacketPack[PACKET_LEN_POS], &t_uPacketLen, sizeof(UN_4BYTE));
	return;
}

UN_4BYTE GetPktLenFromHead(void){
	return 0;
}

/*
	@FUN: 得到主机序的包长度
*/
UN_4BYTE  GetPacketLen(PacketPack *t_pstPacketPack){
	if (NULL == t_pstPacketPack){
		return 0;
	}

	// 数组长度 152, 最后一个下标 t_pstPacketPack->iTailer==151
	return t_pstPacketPack->iTailer + 1;
	//return t_pstPacketPack->iTotal;
}

/*
	@FUN: 设置包中的命令字
*/
void SetPacketCmd(PacketPack *t_pstPacketPack, UN_4BYTE t_uPacketCmd){
	if (NULL == t_pstPacketPack){
		return;
	}
	// 先转成网络序
	t_uPacketCmd = htonl(t_uPacketCmd);
	memcpy(&t_pstPacketPack[PACKET_CMD_POS], &t_uPacketCmd, sizeof(UN_4BYTE));
	return;
}

/*
	@FUN: 获取包中的命令字
*/
UN_4BYTE GetPacketCmd(PacketPack *t_pstPacketPack){
	if (NULL == t_pstPacketPack){
		return 0;
	}

	UN_4BYTE uCmd=0;
	memcpy(&uCmd,&t_pstPacketPack->pBuffer[PACKET_CMD_POS],sizeof(UN_4BYTE));

	// 将网络序还原为主机序
	return ntohl(uCmd);
}

/*
	@FUN: 设置包中的 seq
*/
void SetPacketSeq(PacketPack *t_pstPacketPack, UN_4BYTE t_uPacketSeq){
	if (NULL == t_pstPacketPack){
		return;
	}

	t_uPacketSeq = htonl(t_uPacketSeq);
	memcpy(&t_pstPacketPack->pBuffer[PACKET_SEQ_POS], &t_uPacketSeq, sizeof(UN_4BYTE));

	return;
}

/*
	@FUN: 返回包中的 seq
*/
UN_4BYTE GetPacketSeq(PacketPack *t_pstPacketPack){
	if (NULL == t_pstPacketPack){
		return 0;
	}

	UN_4BYTE uSeq = 0;
	memcpy(&uSeq, &t_pstPacketPack->pBuffer[PACKET_SEQ_POS], sizeof(UN_4BYTE));
	return ntohl(uSeq);
}

/*
	@fun: 得到数据的开始位置指针
*/
const char *GetDataPos(PacketPack *t_pstPacketPack){
	if (NULL == t_pstPacketPack){
		return NULL;
	}
	//return (char *)(t_pstPacketPack->pBuffer + t_pstPacketPack->iPos);
    //printf("%c\n",t_pstPacketPack->pBuffer[PACKET_BODY_POS]);
    //printf("%s\n",t_pstPacketPack->pBuffer+PACKET_BODY_POS);

	//return  (const char *)t_pstPacketPack->pBuffer[PACKET_BODY_POS]; 这里会 core, 因为后面是个字符! 不是指针
	return (const char *)t_pstPacketPack->pBuffer+PACKET_BODY_POS;
}

/*
	@fun: 得到 body-n 的长度
*/
int GetDataSize(PacketPack *t_pstPacketPack){
	//
	if (NULL == t_pstPacketPack){
		return -1;
	}
	//TAIL-POS 就是数据的长度
	return t_pstPacketPack->iTailer - t_pstPacketPack->iPos;
}



/*
	@fun: 设置数据部分
	@有疑问！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！
*/
int SetDataBody(PacketPack *t_pstPacketPack, const char *t_pData, int t_iDataLen){
	if (NULL == t_pstPacketPack || NULL == t_pData || t_iDataLen<=0){
		return -1;
	}

	//
	if (t_pstPacketPack->iTailer + t_iDataLen > t_pstPacketPack->iTotal - 1){
		return -1;
	}

	memcpy(&t_pstPacketPack->pBuffer[t_pstPacketPack->iTailer], t_pData, t_iDataLen);
	t_pstPacketPack->iTailer += t_iDataLen;
	t_pstPacketPack->pBuffer[t_pstPacketPack->iTailer] = PROTO_EOT;

	return 0;
}

////////////////////////////////////////////////////////////


/*
	@FUN: 处理一个完整的 TCP-packet(从 recv 调用而来)
	@FUN: 处理每种命令类型的数据包
*/
int ProcessEntirePacket(ConnFd *t_pstConnFd){
	if (NULL == t_pstConnFd){
		return RET_FAIL;
	}

	// 获得接收 buff 和长度
	char *pBuffer = (char *)t_pstConnFd->pszRecvBuf;
	//int iPacketLen = t_pstConnFd->iPacketLen;
	int iRecvLen = t_pstConnFd->iRecvCurLen;
	//iPacketLen 这里应该是等于 iRecvLen 的
	//printf("%s,%d,%d,%c,%c\n",pBuffer,iPacketLen,iRecvLen,pBuffer[13],pBuffer[22]);

	int iRet = 0;
	/*
		StrToPack 这个函数做的事情是把缓冲区 buf 转换成我们可以处理的字符串数组?
		注意缓冲区 buf 中的数据是网络序的
	*/
	//PacketPack *pstPacketPack = String2PacketPack(pBuffer, iPacketLen);
	PacketPack *pstPacketPack = String2PacketPack(pBuffer, iRecvLen);
	if (NULL == pstPacketPack){
		LOG_ERROR("[%s:%d] String2PacketPack return NULL", t_pstConnFd->szIpaddr, t_pstConnFd->iPort);
		return RET_FAIL;
	}

	// 从报文中获取到主机序命令字
	unsigned int uPacketCmdValue = GetPacketCmd(pstPacketPack);
	switch (uPacketCmdValue)
	{
	case CMD_TEST_VALUE:
		iRet = RET_SUCC;
		ProcessTestBinProtocol(t_pstConnFd, pstPacketPack);
        break;
	case CMD_TEST_ECHO:
		iRet = RET_SUCC;
		SendBackEcho(t_pstConnFd);
        break;
	default:
		iRet = RET_FAIL;
		LOG_ERROR("[%s:%d]unknown protocol [%u]", t_pstConnFd->szIpaddr, t_pstConnFd->iPort, uPacketCmdValue);
		break;
	}

	// 销毁 pack(不销毁会导致内存泄露)
	__DestoryPack(pstPacketPack);
	return iRet;
}

//////////////////////////////// 协议测试 ////////////////////////////////////////

/*
	发送给 client 要求两点:
	t_pstConnFd->pszSendBuf: 二进制数组中存放的的待发送数据
	t_pstConnFd->iSendPacketLen: 待发送的长度 (pszSendBuf 的数据长度)
	以上二者缺一不可
*/
int SendBackEcho(ConnFd *t_pstConnFd){
	if (NULL == t_pstConnFd){
		return RET_FAIL;
	}

	// 注意: t_pstConnFd->iSendPacketLen 不能为 0

	// 直接把 recvbuf 的内容复制到 sendbuf 中
	memcpy(t_pstConnFd->pszSendBuf, t_pstConnFd->pszRecvBuf, t_pstConnFd->iPacketLen);
	// 只复制了一个报文的长度 (可能需要微调整下, 比如改成已经接收的长度?)
	t_pstConnFd->iSendPacketLen = t_pstConnFd->iPacketLen;

	if (RET_FAIL == EventAdd(t_pstConnFd->pstEventBase, t_pstConnFd->iFd, \
		EVENT_WRITABLE, ProcessSendCallback, t_pstConnFd)){
		LOG_ERROR("[%d] %s:%d event_add failed, %s", t_pstConnFd->szIpaddr, t_pstConnFd->iPort, strerror(errno) );
		exit(-1);
	}
	return RET_SUCC;
}

/*
	@fun: 处理测试协议的报文
	@ret:
*/
int ProcessTestBinProtocol(ConnFd *t_pstConnFd, PacketPack *t_pstPacketPackRecv){
	if (NULL == t_pstConnFd || NULL == t_pstPacketPackRecv){
		return RET_FAIL;
	}
	// 获取数据部分 (字符串 helloworld) 的头指针 (接收部分)
	const char *pDataBufferRecv = GetDataPos(t_pstPacketPackRecv);
    //LOG_INFO("recv data:%s,%c",pDataBufferRecv,*pDataBufferRecv);  //core??????

	// 获取数据部分长度 t_pstPacketPack->iTailer - t_pstPacketPack->iPos;
	int iDataLen = GetDataSize(t_pstPacketPackRecv);

	// 处理数据逻辑

	// 从发送缓存分配内存
	//2018-05-16: 注意这个长度, 是服务器 send 时候, 预先申请好的长度, 借用__CreatePackFromBuffer 函数将它变成一个可用 packet 结构!!!!!
	int iLength = iDataLen + 128;

	// 直接获取 conn 发送缓冲的指针，将发送数据直接填写到这个缓冲，减少数据拷贝
	// 当然也可以直接用写 buffer

	//L: 获取写缓冲区的头指针 (顺便 check 一下需不需要扩容)
	//2018-05-16: 检查 fd 的写缓冲区 (堆上分配的内存) 是否需要扩容, 如果需要扩容就直接 alloc
	char* pSendBuffer = ResizeSendBufferPtr(t_pstConnFd, iLength);

	// 将写缓冲区变成一个好用的 packet
	PacketPack *pstPacketPackSend = __CreatePackFromBuffer(pSendBuffer,iLength);

	// 直接把 recvbuffer 复制过来
	SetPacketCmd(pstPacketPackSend,GetPacketCmd(t_pstPacketPackRecv));
	SetPacketSeq(pstPacketPackSend, GetPacketSeq(t_pstPacketPackRecv));

	// 添加数据 (这里借用了读缓冲区的内容 pDataBufferRecv)
	if (RET_FAIL == AddAllData2Packet(pstPacketPackSend, pDataBufferRecv, iLength)){
		return RET_FAIL;
	}

	//SendPacket2Client(t_pstConnFd,t_pstPacketPackRecv);
	SendPacket2Client(t_pstConnFd, pstPacketPackSend);
	// 销毁 pack
	__DestoryPack(pstPacketPackSend);

	return RET_SUCC;
}

/*
@FUN: 向缓冲区添加内容
@param:
@param:t_uBuffLen --- 数据长度
*/
int AddAllData2Packet(PacketPack *t_pstPacketPack, const char *t_pBuffer, int t_uBuffLen){
	if (NULL == t_pBuffer || NULL == t_pstPacketPack || t_uBuffLen <= 0){
            printf("asasasas\n");
		return  RET_FAIL;
	}
	// 检查长度是否非法
	//t_pstPacketPack->iTailer 初始是指向 DATA 开始位置

    printf("%d,%d,%d\n",t_pstPacketPack->iTailer,t_uBuffLen,t_pstPacketPack->iTotal);
	if (t_pstPacketPack->iTailer + t_uBuffLen > t_pstPacketPack->iTotal - 1){
		// 这里好像有问题, 不能打开注释
		//return RET_FAIL;
	}

	//PACKET_BODY_POS == iTailer[init]


    int iOriLen=strlen("ffllosb");
    //t_pBuffer[iOriLen-1]='b';
    printf("%d,%d,%d,%s",t_pstPacketPack->iTailer,t_uBuffLen,strlen(t_pBuffer),t_pBuffer);

	//memcpy(&t_pstPacketPack->pBuffer[t_pstPacketPack->iTailer], t_pBuffer, t_uBuffLen);
    memcpy(&t_pstPacketPack->pBuffer[t_pstPacketPack->iTailer], "ffllosb", iOriLen);
	// 注意 "ffllosb" 最后有一个'\0'
    memcpy(&t_pstPacketPack->pBuffer[t_pstPacketPack->iTailer+iOriLen-1],"aaaaaaaaaaaaaaaaaaaaa",t_uBuffLen-iOriLen);

    printf("rilegou,%c\n",t_pstPacketPack->pBuffer[t_pstPacketPack->iTailer+1]);
    // 更新 packet 长度
	t_pstPacketPack->iTailer += t_uBuffLen;
    //printf("%d,%d,%d,%s,%c\n",t_pstPacketPack->iTailer,t_uBuffLen,strlen(t_pBuffer),t_pBuffer,t_pstPacketPack->pBuffer[t_pstPacketPack->iTailer]);
	t_pstPacketPack->pBuffer[t_pstPacketPack->iTailer] = PROTO_EOT;

	return RET_SUCC;
}

/*
	@FUN: 将包添加到 conn 中
	@FUN: 在组装好报文头部 / 数据和尾部后, 利用 pack 将报文的长度填充
*/
int SendPacket2Client(ConnFd *t_pstConnFd, PacketPack *t_pstPacketPack){
	if (NULL == t_pstPacketPack || NULL == t_pstConnFd){
		return RET_FAIL;
	}

	// 设定 packet 的 len
	if (NULL == PacketPack2String(t_pstPacketPack)){
		return RET_FAIL;
	}

	// 将报文复制到 sendbuf 的尾部, 并开启 EPOLLOUT 驱动
	if (RET_FAIL == AppendPktToBuff(t_pstConnFd, NULL, GetPacketLen(t_pstPacketPack))){
		return RET_FAIL;
	}

	return RET_SUCC;
}
