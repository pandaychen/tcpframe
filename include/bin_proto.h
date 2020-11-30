#ifndef __BIN_PROTO_H
#define __BIN_PROTO_H

/*
	二进制协议, 扩展不方便
*/

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <string.h>

#include "reactor.h"

/*
******   协议格式    *********
* unsigned char  SOH;     包头
* unsigned int   len;     包长
* unsigned int   cmd;     命令号
* unsigned int   seq;     序列号
* unsigned char  body[n]; 变长的包体
* unsigned char  EOT;     包尾

1     4    4    4    n      1
SOH  len  cmd  seq  body    EOT
*/

typedef unsigned char UN_BYTE;
typedef unsigned short UN_2BYTE;
typedef unsigned int UN_4BYTE;


enum PROTO_COMMS
{
	// 包头和包尾 ketyword
	PROTO_SOH = 0x28,
	PROTO_EOT = 0x29,

	// 各个字段开始位置
	PACKET_SOH_POS = 0,
	PACKET_LEN_POS = PACKET_SOH_POS + 1,
	PACKET_CMD_POS = PACKET_LEN_POS + 4,
	PACKET_SEQ_POS = PACKET_CMD_POS + 4,
	PACKET_BODY_POS = PACKET_SEQ_POS + 4,

	//PACKET_EOT_POS 的值是和数据长度有关系的, 一般用 tailer 来指向 (在本协议)

	// 包的默认 size
	PACKET_DEFAULT_LEN = 1024,
	// 包的最小长度
	PACKET_MIN_LEN = 18,
	//
	PACKET_MAX_LEN = 1024*1024,	//1Mb
	//SOH+LEN
	PACKET_HEAD_LEN = 5,
};

// 格式化协议包, 便于通用方式的处理 (将字符串转换为二进制数组处理)
struct __packet_pack{
	//
	UN_BYTE *pBuffer;
	int32_t iPos;		// 当前位置
	int32_t iTailer;	// 尾部
	int32_t iHeader;	// 指向头
	int32_t iTotal;		// 长度 (多少字节), 下面的 sample 一共 24 字节
};

/*
	比如一个 helloworld 字符串在 TCP 协议传输中是这样处理的 (PC 端抓包主机序)
	5c 05 28 00 00 00 18 00 00 29 00 00 00 00 00 68
	65 6c 6c 6f 77 6f 72 7c 64 29
*/

typedef struct __packet_pack PacketPack;


// 测试协议
enum TEST_PROTOCOL_VARS
{
	CMD_TEST_VALUE =0x2900,	// 测试协议
	CMD_TEST_ECHO = 0x3000,	//echo
	SEQ_TEST_VALUE =0x0001,
};


#ifdef __cplusplus
extern "C"{
#endif

	void SetPacketLen(PacketPack *t_pstPacketPack, UN_4BYTE t_uPacketLen);
	UN_4BYTE GetPacketLen(PacketPack *t_pstPacketPack);

	void SetPacketCmd(PacketPack *t_pstPacketPack, UN_4BYTE t_uPacketCmd);
	UN_4BYTE GetPacketCmd(PacketPack *t_pstPacketPack);

	void SetPacketSeq(PacketPack *t_pstPacketPack, UN_4BYTE t_uPacketSeq);
	UN_4BYTE GetPacketSeq(PacketPack *t_pstPacketPack);

	int SetDataBody(PacketPack *t_pstPacketPack,const char *t_pData,int t_iDataLen);

	// 将 t_pbuff 指向的位置附加在 packet 的 body 位置
	int32_t AppendData(PacketPack *t_pstPacketPack, const char *t_pBuff, int32_t t_iLen);

	void FreePacketPack(PacketPack *t_pstPacketPack);
	//PacketPack *CreatePacketPack(const char *t_pBuff,int32_t t_iLen);

    //no const
    PacketPack *CreatePacketPack(char *t_pBuff,int32_t t_iLen);

	PacketPack *__CreatePackFromBuffer(const char *t_pBuffer,int32_t t_iLen);
	void __FreePacketPack(PacketPack * t_pstPacketPack);
	void __DestoryPack(PacketPack *t_pstPacketPack);

	//
	const char *PacketPack2String(PacketPack *t_pstPacketPack);
	PacketPack *String2PacketPack(const char *t_pbuff,int32_t t_iLen);

	//
	const char *GetDataPos(PacketPack *t_pstPacketPack);
	int GetDataSize(PacketPack *t_pstPacketPack);

	int ProcessPacket(ConnFd *t_pstConnFd,char *t_pBuff,int t_iLen);
	int ProcessEntirePacket(ConnFd *t_pstConnFd);
	int ProcessPacketTest(ConnFd *t_pstConnFd, PacketPack *t_pstPacketPack);
	int SendPacket2Client(ConnFd *t_pstConnFd, PacketPack *t_pstPacketPack);
	int AddAllData2Packet(PacketPack *t_pstPacketPack,const char *t_pBuffer,int t_uBuffLen);

	/// 测试
	int SendBackEcho(ConnFd *t_pstConnFd);
    int ProcessTestBinProtocol(ConnFd *t_pstConnFd, PacketPack *t_pstPacketPackRecv);


#ifdef __cplusplus
}
#endif


#endif
