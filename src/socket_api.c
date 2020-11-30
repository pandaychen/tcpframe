#include "socket_api.h"
#include "socket_util.h"
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>

/**
* [PynCreateTcpSocket 创建一个socket]
* @param  t_pHost [description]
* @param  t_pIp   [description]
* @return         [description]
*/
int PynCreateTcpSocket(){
	int iSock;
	int iFlag = 1;

	if (iSock = socket(AF_INET, SOCK_STREAM, 0) == -1){
		LOG_ERROR("create tcp socket error");
		return RET_FAIL;
	}

	/* Make sure connection-intensive things like the redis benckmark
	* will be able to close/open sockets a zillion of times */
	if (setsockopt(iSock, SOL_SOCKET, SO_REUSEADDR, &iFlag, sizeof(iFlag)) == -1)
	{
		LOG_ERROR("setsockopt SO_REUSEADDR: %s", strerror(errno));
		return RET_FAIL;
	}

	if (PynServerSetNonblock(iSock) == RET_FAIL){
		LOG_ERROR("set socket nonblock error");
		return RET_FAIL;
	}
	
	return iSock;
}


/**
 * [PynResolveDns 简单的DNS域名,低效]
 * @param  t_pHost [description]
 * @param  t_pIp   [description]
 * @return         [description]
 */
/*
int PynResolveDns(const char *t_pHost, char *t_pIpSave) {
	if (NULL == t_pHost || NULL == t_pIpSave){
		return RET_FAIL;
	}
	struct sockaddr_in stSa;

	stSa.sin_family = AF_INET;
	//inet_aton() returns nonzero if the address is valid, zero if not. 
	if (0 == inet_aton(t_pHost, &stSa.sin_addr)){
		//is domain
		struct hostent *pHostent = NULL;
		pHostent = gethostbyname(t_pHost);
		if (NULL == pHostent){
			LOG_ERROR("resolve host %s error",t_pHost);
			return RET_FAIL;
		}
		memcpy(&stSa.sin_addr, pHostent->h_addr, sizeof(struct in_addr));
	}
	strncpy(t_pIpSave, inet_ntoa(stSa.sin_addr), IP_LEN);
	return RET_SUCC;
}
*/


/**
* [PynResolveDns 简单的DNS域名,低效]
* @param  t_pHost [description]
* @param  t_pIp   [description]
* @return         [description]
*/
int PynResolveDnsNonBlock(const char *t_pHost, char *t_pIpSave){
	//

	return RET_SUCC;
}

/**
* [PynServerConnect normal connect -- 低效]
* @param  t_pszServerIp [description]
* @param  t_iServerPort [description]
* @return               [description]
*/
int PynServerConnectNonBlock(const char *t_pServerIp, const int t_iServerPort){
	if (NULL == t_pServerIp){
		return RET_FAIL;
	}

	int iSock;
	struct sockaddr_in stSa;

	return  iSock;
}


/**
 * [PynServerConnect normal connect -- 低效]
 * @param  t_pszServerIp [description]
 * @param  t_iServerPort [description]
 * @return               [description]
 */
/*
int PynServerConnect(const char *t_pServerIp, const int t_iServerPort) {
	if (NULL == t_pServerIp){
		return RET_FAIL;
	}

	int iSock;
	struct sockaddr_in stSa;
		
	if (iSock = PynCreateTcpSocket() ==RET_FAIL ){
		return RET_FAIL;
	}

	//初始化服务器sockaddr_in结构
	stSa.sin_family = AF_INET;
	stSa.sin_port = htons(t_iServerPort);
	//确定是否是域名还是IP
	if (inet_aton(t_pServerIp,&stSa.sin_addr)== 0){
		struct hostent *pHe =NULL;
		pHe = gethostbyname(t_pServerIp);
		if (NULL == pHe){
			LOG_ERROR("can't resolve: %s",t_pServerIp);
			close(iSock);
			return RET_FAIL;
		}
		
		memcpy(&stSa.sin_addr, pHe->h_addr, sizeof(struct in_addr));
	}
	//get server ip succ
	if (0 > connect(iSock, (struct sockaddr *)&stSa, sizeof(stSa))){
		if (errno == EINPROGRESS){
			return iSock;
		}
		LOG_ERROR("connect to server %s:%d[%s] error", t_pServerIp, t_iServerPort, strerror(errno));
		close(iSock);
		return RET_FAIL;
	}

	return iSock;
}

*/

int PynServerSetNonblock(int t_iFd) {
	if (t_iFd < 0) {
		return RET_FAIL;
	}

	/* Set the socket nonblocking.
	* Note that fcntl(2) for F_GETFL and F_SETFL can't be
	* interrupted by a signal. */
	int iFlag = 0;
	if ((iFlag = fcntl(t_iFd, F_GETFL)) == -1) {
		return RET_FAIL;
	}
	if (fcntl(t_iFd, F_SETFL, iFlag | O_NONBLOCK) == -1) {
		return RET_FAIL;
	}

	return RET_SUCC;
}

/*
int PynServerSetTcpNoDelay(int t_iFd){
	if (t_iFd < 0){
		return RET_FAIL;
	}

	int iOp = 1;
	if (setsockopt(t_iFd, IPPROTO_TCP, TCP_NODELAY, &iOp, sizeof(int)) == -1){
		LOG_ERROR("setsockopt TCP_NODELAY error: %s",strerror(errno));
		return RET_FAIL;
	}
	return RET_SUCC;
}
*/

int PynServerSetTcpSendbuffer(int t_iFd, int t_iBufferSize) {
	if (t_iFd < 0 || t_iBufferSize <= 0) {
		return RET_FAIL;
	}

	if (setsockopt(t_iFd, SOL_SOCKET, SO_SNDBUF, &t_iBufferSize, sizeof(int)) == -1) {
		LOG_ERROR("setsockopt SO_SNDBUF error: %s", strerror(errno));
		return RET_FAIL;
	}

	return RET_SUCC;
}


int PynServerSetTcpKeepalive(int t_iFd) {
	if (t_iFd < 0) {
		return RET_FAIL;
	}

	int iOper = 1;

	if (setsockopt(t_iFd, SOL_SOCKET, SO_KEEPALIVE, &iOper, sizeof(int)) == -1)
	{
		LOG_ERROR("setsockopt SO_KEEPALIVE error: %s", strerror(errno));
		return RET_FAIL;
	}

	return RET_SUCC;
}

int PynServerSetTcpReuseaddr(int t_iFd) {
	if (t_iFd < 0) {
		return RET_FAIL;
	}

	int iOper = 1;

	if (setsockopt(t_iFd, SOL_SOCKET, SO_REUSEADDR, &iOper, sizeof(iOper)) == -1)
	{
		LOG_ERROR("setsockopt SO_REUSEADDR error: %s", strerror(errno));
		return RET_FAIL;
	}

	return RET_SUCC;
}


void PynServerSetError(char *t_szErrorMsg, const char *fmt, ...) {
	if (NULL == t_szErrorMsg || NULL == fmt) {
		return;
	}

	va_list ap;

	va_start(ap, fmt);
	vsnprintf(t_szErrorMsg, ERROR_MSG_LEN, fmt, ap);
	va_end(ap);
	return;
}

/*
	@fun:创建TCPserver并监听端口
	@ret:fd
*/
int PynServerCreateTcpServer(const char *t_szServerIp, int t_iServerPort) {
	if (NULL == t_szServerIp) {
		return RET_FAIL;
	}

	if (t_iServerPort < 1024 || t_iServerPort > 65535) {
		return RET_FAIL;
	}

	int iListenFd = 0;
	int iOper = 1;
	struct sockaddr_in stSinAddr;
	memset(&stSinAddr, 0, sizeof(struct sockaddr_in));

	if ((iListenFd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		LOG_ERROR("creating socket error: %s", strerror(errno));
		return RET_FAIL;
	}

	/* Make sure connection-intensive things like the redis benckmark
	* will be able to close/open sockets a zillion of times */
	if (RET_FAIL == PynServerSetTcpReuseaddr(iListenFd)) {
		return RET_FAIL;
	}

	if (RET_FAIL == PynServerSetNonblock(iListenFd)) {
		LOG_ERROR("set socket nonblock error: %s", strerror(errno));
		return RET_FAIL;
	}

	stSinAddr.sin_family = AF_INET;
	stSinAddr.sin_port = htons(t_iServerPort);
	stSinAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	if (inet_aton(t_szServerIp, &stSinAddr.sin_addr) == 0) {
		//check t_szServerIp is valid or not
		LOG_ERROR("invalid bind address");
		close(iListenFd);
		return RET_FAIL;
	}


	//bind server address
	if (-1 == bind(iListenFd, (struct sockaddr *)&stSinAddr, (socklen_t)sizeof(struct sockaddr) )) {
		LOG_ERROR("bind error: %s", strerror(errno));
		close(iListenFd);
		return RET_FAIL;
	}

	//listen
	/* the magic 511 constant is from nginx */
	if (-1 == listen(iListenFd, SOMAXCONN)) {
		LOG_ERROR("listen error: %s", strerror(errno));
		close(iListenFd);
		return RET_FAIL;
	}

	return iListenFd;
}



/**
* PynServerRecvData 接收数据
* @param t_iFd
* @param t_pszSendBuff  发送数据的buff头指针
* @param t_iSendTotalBytes  需要发送的总字节
* @ret 实际成功发送的字节数
*/
int PynServerRecvData(int t_iFd, char *t_pszRecvBuff, int t_iNeedRecvTotalBytes) {
	//
	if (NULL == t_pszRecvBuff || t_iFd < 0 ) {
		return 0;
	}

	int iRecvCount = 0;		//在这个函数中已读取的总数(看做在一次触发中)
	int iSingleReadRet = 0;	//每次调用recv/read返回的值

	//我们的模型是非阻塞的,所以要按照非阻塞的读取方式
	while (iRecvCount < t_iNeedRecvTotalBytes) {

		iSingleReadRet = read(t_iFd, t_pszRecvBuff + iRecvCount, t_iNeedRecvTotalBytes - iRecvCount);
		if (0 == iSingleReadRet) {
			//对端已经关闭连接(还没读完的情况下)
			return 0;
		}
		else if (iSingleReadRet < 0) {
			//可能发生了错误
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				//这次收不了了,下次再试
				break;
			}
			else if (errno == EINTR)
			{
				//重试下
				continue;
			}
			//严重错误
			return -1;
		}
		iRecvCount += iSingleReadRet;
	}
	return iRecvCount;
}


/**
* PynServerSendData 发送数据
* @param t_iFd
* @param t_pszSendBuff  发送数据的buff头指针
* @param t_iSendTotalBytes  需要发送的总字节
* @ret 实际成功发送的字节数
*/
int PynServerSendData(int t_iFd, char *t_pszSendBuff, int t_iSendTotalBytes) {
	if (NULL == t_pszSendBuff || t_iFd <= 0 || t_iSendTotalBytes < 0) {
		return -1;
	}

	int iSendCount = 0;		//总数
	int iSingleSendRet = 0;	//每次

	while (iSendCount < t_iSendTotalBytes)
	{
		iSingleSendRet = write(t_iFd, t_pszSendBuff + iSendCount, t_iSendTotalBytes - iSendCount);
		// On success, the number of bytes written is returned (zero indicates nothing was written)
		if (iSingleSendRet < 0) {
			//occur a erro
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				//下次再试
				break;
			}
			else if (errno == EINTR) {
				continue;
			}
			return -1;
		}
		iSendCount += iSingleSendRet;
	}

	return iSendCount;
}
