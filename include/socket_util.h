#ifndef __EVENT_API_H
#define __EVENT_API_H

/*
	封装 socket API 操作
*/

#include "reactor.h"
#include "min_heap.h"
#include "server_log.h"


#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifdef __cplusplus
extern "C" {
#endif

//socket api
int PynServerAccept(int t_iServerSock, char *t_pszClientIp, int *t_pClientPort);

int PynServerCreate(const char *t_pszServerIp, const int t_iServerPort);
int PynServerPeerToString(char *t_pszIp, int *t_pPort);
int PynServerResolve(const char *t_pszHostname, char *t_pszDestIp);

// 应用层缓冲区操作
int AppendPktToBuff(ConnFd *t_pstConnFd, char *t_pszBuff, int t_iLen);
char *	ResizeSendBuffer();
char *	ResizeRecvBuffer();
char *ResizeRecvBufferPtr(ConnFd *t_pstConnFd, int t_iNeedRecvLen);
char *ResizeSendBufferPtr(ConnFd *t_pstConnFd, int t_iNeedRecvLen);


//Event 回调函数集合
void ProcessAcceptCallback(EventBase *t_pstEventBase, int t_iFD, void *t_pData);
void ProcessRecvCallback(EventBase *t_pstEventBase, int t_iFd, void *t_pData);
void ProcessSendCallback(EventBase *t_pstEventBase, int t_iFd, void *t_pData);

int sendbackecho(ConnFd *t_pstConnFd, char *t_pszBuff/* =NULL */, int t_iLen);


#ifdef __cplusplus
}
#endif


#endif

