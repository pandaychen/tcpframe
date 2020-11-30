#ifndef  __SERVER_LOG
#define  __SERVER_LOG

#include <sys/types.h>
#include <pthread.h>    //for mutex_lock
#include <unistd.h>
#include <stdint.h>
#include "comm.h"


#ifdef __cplusplus
extern "C"
{
#endif

#define DEFAULT_LOG_PATH  "./log/"

/* 定义日志级别 */
enum LOG_LEVEL{
	LOG_ERROR_LEVEL= 0,
	LOG_WARNING_LEVEL = 1,
	LOG_INFO_LEVEL = 2,
	LOG_DEBUG_LEVEL = 3,
	LOG_LARGERNUM_LEVEL = 4,
};

enum COMM_SYSTEM_VARS{
	FD_STDOUT = 1,
	FD_STDIN = 0,
	FD_STDERR = 2,
	FD_DEVNULL = -2,
	FD_FILE = 3,
};

enum LOG_VARS{
	MIN_LOGFILE_SIZE = 1024 * 1024,	/* 默认 log 的最大日志文件为 1M */
	MAX_PATH_LEN = 512,	/* 文件名的最大长度为 512 */
	MAX_LOGMSG_LEN = 8192,	/* 一条信息的最大长度 */
};


/* 封装 Linux 日志 */
struct __server_log{
	pthread_mutex_t stLogMutex;		// 读写互斥
	int iLogFd;		/* 日志文件描述符 */
	int iLogSize;	/* 日志文件大小 */
	int iLogMask;
	int iLogLevel;
	int iLogFileCount;	/* 日志文件循环个数 */
	int iLogStd;	/* 是否为标准输出 */
	char szLogFilename[LOG_PATH_LEN];	/* 日志文件名 */
	time_t iLogDate;	/* 日志时间 */
};

typedef struct __server_log ServerLogHandle;

/* 日志结构指针的全局变量 -- 一个进程一个 */
// 去其他文件寻找变量定义
extern ServerLogHandle *g_pstServerLogHandle;


/**
* 初始化 Linux 进程日志句柄
* @param fileName  日志文件名
* @param logLevel  日志级别
* @param t_iLogSize  日志文件大小
* @param t_iRollCount 日志文件循环的个数
* @return   ptr    日志指针
*/
ServerLogHandle *CreateLogfileHandle(const char *t_pFilename,\
									int t_iLogLevel,\
									int t_iLogSize,\
									int t_iRollCount);

/**
* 销毁文件句柄
*/
void DestroyLogfileHandle(ServerLogHandle *t_pstServerLogHandle);

/*
	*LOG 通用函数 (供宏调用的通用函数)
*/
void __LogMsg(ServerLogHandle *t_pstServerLogHandle, \
	const char *t_pFilename, \
	int t_iLinePos, \
	int t_iLogLevel, \
	const char *t_formatter, ...
	);

/*
	* fun: 向全局 G_pLog 中写日志 (多进程模型下, 每个子进程独享一个全局日志句柄, 不冲突)
	* or 使用一个单独的进程 server 监听 UDP 端口接收日志? 日志接收器
	* @param formatter 日志的格式
	* @param ... 可变参数
	* @return
	*/

/*
	__FILE__ : filename
	__LINE__ : line no

	##__VA_ARGS__ : 可变参数
	*/
#define LOG_INFO(formatter,...)	\
{	\
	__LogMsg(g_pstServerLogHandle, __FILE__, __LINE__, LOG_INFO_LEVEL, formatter, ##__VA_ARGS__);	\
}

#define LOG_DEBUG(formatter,...)	\
{	\
	__LogMsg(g_pstServerLogHandle, __FILE__, __LINE__, LOG_DEBUG_LEVEL, formatter, ##__VA_ARGS__);	\
}

#define LOG_ERROR(formatter,...) \
{	\
	__LogMsg(g_pstServerLogHandle, __FILE__, __LINE__, LOG_ERROR_LEVEL, formatter, ##__VA_ARGS__);	\
}

#define LOG_WARNING(formatter,...)	\
{	\
	__LogMsg(g_pstServerLogHandle, __FILE__, __LINE__, LOG_WARNING_LEVEL, formmater, ##__VA_ARGS__);	\
}



#ifdef __cplusplus
}
#endif


#endif
