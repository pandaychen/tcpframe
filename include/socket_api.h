#ifndef __SOCKET_API_H
#define __SOCKET_API_H

#include "comm.h"
#include "server_log.h"
#include <sys/types.h>
#include <sys/socket.h>

enum NETWORK_VARS
{
	CONNECT_NONE = 0,
	CONNECT_NONBLOCK = 1,

	ERROR_MSG_LEN = 1024,
};

#ifdef __cplusplus
extern "C" {
#endif

int PynResolveDns(const char *t_pHost, char *t_pIp);

int PynServerConnect(const char *t_pszServerIp, const int t_iServerPort);

int PynServerNonBlockConnect(const char *t_pszServerIp, const int t_iServerPort);

void PynServerSetError(char *t_szErrorMsg, const char *fmt, ...);

int PynServerSetNonblock(int t_iFd);

int PynServerSetTcpNoDelay(int t_iFd);

int PynServerSetTcpSendbuffer(int t_iFd, int t_iBufferSize);

//#define SO_REUSEPORT 15
///usr/include/asm-generic/socket.h
int PynServerSetTcpReuseport(int t_iFd);

int PynServerSetTcpKeepalive(int t_iFd);

int PynServerSetTcpReuseaddr(int t_iFd);

//
int PynServerCreateTcpServer(const char *t_szServerIp, int t_iServerPort);


/**
* PynServerSendData 发送数据
* @param t_iFd
* @param t_pszSendBuff  发送数据的 buff 头指针
* @param t_iSendTotalBytes  需要发送的总字节
* @ret 实际成功发送的字节数
*/
int PynServerSendData(int t_iFd, char *t_pszSendBuff, int t_iSendTotalBytes);

/**
* PynServerRecvData 接收数据
* @param t_iFd
* @param t_pszSendBuff  发送数据的 buff 头指针
* @param t_iSendTotalBytes  需要发送的总字节
* @ret 实际成功发送的字节数
*/
int PynServerRecvData(int t_iFd, char *t_pszRecvBuff, int t_iRecvTotalBytes);

#ifdef __cplusplus
}
#endif



#endif
