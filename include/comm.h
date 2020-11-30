#ifndef __COMM_H
#define __COMM_H

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <sys/resource.h>

enum COMM_RET
{
	CALL_FAIL = -1,
	CALL_SUCC = 0,
	RET_FAIL = -1,
	RET_SUCC = 0,
	SOCKET_FAIL = -1,
	SOCKET_SUCC = 0,
};


enum COMM_MASK {
	EVENT_NONE = 0x0,
	EVENT_WRITABLE = 0x01,
	EVENT_READABLE = 0x02,
	EVENT_OTHER = 0x04,
};

enum COMM_SERVER {
	EPOLL_SIZE = 2048,
	MAX_CLIENT_COUNT = 20000,        // 每个进程最多处理客户数量 (需要修改 fd 上限)
	MAX_TCP_BUF_SIZE = 4096,
	MAX_CONN_TIMEOUT = 60000,		// 最大超时时间
	LOG_PATH_LEN = 80,
	MAX_IP_LEN = 64,
	IP_LEN = 16,
	MINHEAP_INIT_SIZE=16,	// 最小堆指针数组的初始值

	EPOLL_EVENT_BASE_STOP = 0,
	EPOLL_EVENT_BASE_START = 1,
};

enum EPOLL_VARS {
	EPOLL_ET_TYPE = 1,
	EPOLL_LT_TYPE = 2,
};

enum LINUX_KERNEL_CONFIG
{
	SYSTEM_FILE_MAX=1000000,	//200000

};

#endif
