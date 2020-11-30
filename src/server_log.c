#include "server_log.h"
#include "comm.h"
#include <pthread.h>

#define DEBUG_SERVER_LOG

static char szLogOut[][8] = \
{ "ERROR", "WARN", "INFO", "DEBUG" };

extern ServerLogHandle* g_pstServerLogHandle;


/**
* static 函数作用域仅限本文件
* Is_File_OverSize  日志文件是否超过了最大限额
* @param pLog       日志结构的指针
* @return:
*/
static inline  long int CheckFileSize(ServerLogHandle *t_pstServerLogHandle){
	//
	struct stat temp_st;
	if (t_pstServerLogHandle != NULL){
		return 0;
	}
	
	if (stat(t_pstServerLogHandle->szLogFilename, &temp_st) == -1){
		return 0;
	}
	else{
		return (temp_st.st_size >= t_pstServerLogHandle->iLogSize);
	}
}

/**
* Fun:当日志大小超过指定大小时,将旧文件改名,并创建新文件的fd
* RollOldFile         循环日志文件, pLog->log_name永远是当前日志文件,
* pLog->log_name.1, pLog->log_name.1, ...pLog->log_name.pLog->log_filecount依次按新旧的排列
* @param pLog       日志结构的指针
*/
static void __roll_oldfile2new(ServerLogHandle *t_pstServerLogHandle,int t_iSign){
	if (NULL == t_pstServerLogHandle){
		return;
	}

	if (t_pstServerLogHandle->iLogFd >= 0){
		//关闭当前的文件
		close(t_pstServerLogHandle->iLogFd);
		t_pstServerLogHandle->iLogFd = -1;
	}
	
	struct stat temp_stat;
	char szOldFile[MAX_PATH_LEN] = { 0 };
	char szNewFile[MAX_PATH_LEN] = { 0};

	snprintf(szOldFile, MAX_PATH_LEN - 1, "%s.%d", t_pstServerLogHandle->szLogFilename, t_pstServerLogHandle->iLogFileCount);
	if (stat(szOldFile, &temp_stat) == 0){
		//删除
		remove(szOldFile);
	}

	/* 循环的将pLog->log_name.pLog->log_filecount - 1  -> pLog->log_name.pLog->log_filecount
	...
	pLog->log_name.2 -> pLog->log_name.3
	pLog->log_name.1 -> pLog->log_name.2
	*/

	int i = t_pstServerLogHandle->iLogFileCount-1;
	for (; i >0 ; i--){
		//
		memset(szOldFile, 0, sizeof(szOldFile));
		memset(szNewFile, 0, sizeof(szNewFile));
		snprintf(szOldFile, MAX_PATH_LEN - 1, "%s.%d", t_pstServerLogHandle->szLogFilename, i);
		snprintf(szNewFile, MAX_PATH_LEN - 1, "%s.%d", t_pstServerLogHandle->szLogFilename, i + 1);
		if (stat(szOldFile, &temp_stat) == 0){
			rename(szOldFile, szNewFile);
		}
	}

	/* 将pLog->log_name ->　pLog->log_name.1 */
	memset(szOldFile, 0, sizeof(szOldFile));
	memset(szNewFile, 0, sizeof(szNewFile));
	snprintf(szNewFile, MAX_PATH_LEN - 1, "%s.1", t_pstServerLogHandle->szLogFilename);
	if (stat(t_pstServerLogHandle->szLogFilename, &temp_stat) == 0){
		rename(t_pstServerLogHandle->szLogFilename, szNewFile);
	}
	
	//create a new file
	t_pstServerLogHandle->iLogFd = open(t_pstServerLogHandle->szLogFilename, O_CREAT | O_RDWR | O_APPEND, 0644);
	if (t_pstServerLogHandle->iLogFd == -1){
		//open wrong
		free(t_pstServerLogHandle);
		g_pstServerLogHandle = NULL;

	}
	return;
}


/**
* 内部函数
* @param pLog       日志结构的指针
* @param loglevel  日志级别
* @param msg       日志消息
*/
static void __print_log(ServerLogHandle *t_pstServerLogHandle,int t_iLogLevel,const char * t_pszMsg){
	//
	if (t_pstServerLogHandle == NULL){
		return;
	}

	char szBuffer[64] = { 0 };
	char szMsg[MAX_LOGMSG_LEN+2] = { 0 };	//add a extra 2 with \n and \0
	time_t now = time(NULL);
	struct tm ptm;

	//get tm struct info
	localtime_r(&now, &ptm);
    int *p=(int *)malloc(sizeof(int));
    *p=2;
	//get date str
	strftime(szBuffer, 64 - 1, "%Y-%m-%d %H:%M:%S", &ptm);
	snprintf(szMsg, MAX_LOGMSG_LEN, "[%s]%s:%s\n", szLogOut[t_iLogLevel],szBuffer,t_pszMsg);
	szMsg[MAX_LOGMSG_LEN - 1] = '\0';
	//szMsg[MAX_LOGMSG_LEN - 1] = 0;

	if (t_pstServerLogHandle->iLogFd > 0 && t_pstServerLogHandle->iLogStd == FD_FILE){
		pthread_mutex_lock(&t_pstServerLogHandle->stLogMutex);
		if (CheckFileSize(t_pstServerLogHandle)>0){
			__roll_oldfile2new(t_pstServerLogHandle, 1);
		}
		write(t_pstServerLogHandle->iLogFd,szMsg,strlen(szMsg));
		//unlock
		pthread_mutex_unlock(&t_pstServerLogHandle->stLogMutex);
	}
	else if(t_pstServerLogHandle->iLogStd == FD_STDOUT ||t_pstServerLogHandle->iLogStd == FD_STDERR){
		//print
		printf("%s\n", szMsg);
	}
}


/*
*LOG通用函数(供宏调用的通用函数)
* LogRecord 写错误日志
* @param pLog 日志结构的指针
* @param fmt  日志的格式
* @param ...  可变参数
*/
void __LogMsg(ServerLogHandle *t_pstServerLogHandle, const char *t_pFilename, int t_iLinePos, int t_iLogLevel, const char *t_formatter, ...){
	//
	if (NULL == t_pstServerLogHandle|| NULL ==t_pFilename){
		return;
	}
	
	if (t_pstServerLogHandle->iLogLevel < t_iLogLevel){
		return;
	}

	char szMsg[MAX_LOGMSG_LEN] = { 0 };
	char szMsg1[MAX_LOGMSG_LEN] = { 0 };
	va_list ap;
	va_start(ap, t_formatter);
	vsnprintf(szMsg, MAX_LOGMSG_LEN, t_formatter, ap);
	va_end(ap);

	snprintf(szMsg1, MAX_PATH_LEN - 1, "<%s:%d>:%s", t_pFilename, t_iLinePos, szMsg);
	__print_log(t_pstServerLogHandle, t_iLogLevel, szMsg1);
	return;
}



/*
* 初始化Linux进程日志句柄
* @param fileName  日志文件名
* @param logLevel  日志级别
* @param maxSpace  日志文件大小
* @param rollCount 日志文件循环的个数
* @return   ptr    日志指针
*/
ServerLogHandle *CreateLogfileHandle(const char *t_pFilename, int t_iLogLevel, int t_iLogSize,int t_iRollCount){
	//创建并初始化日志结构
	if (NULL == t_pFilename){
		return NULL;
	}
	//elegent?
	g_pstServerLogHandle = (ServerLogHandle *)malloc(sizeof(ServerLogHandle));
	if (NULL == g_pstServerLogHandle){
		return NULL;
	}
	memset(g_pstServerLogHandle, 0,sizeof(ServerLogHandle));

	//init
	g_pstServerLogHandle->iLogDate = time(NULL);
	g_pstServerLogHandle->iLogLevel = t_iLogLevel < 0 ? 0 : t_iLogLevel;
	g_pstServerLogHandle->iLogSize = t_iLogSize < MIN_LOGFILE_SIZE ? MIN_LOGFILE_SIZE : t_iLogSize;
	g_pstServerLogHandle->iLogFileCount = t_iRollCount < 3 ? 3 : t_iRollCount;
	g_pstServerLogHandle->iLogStd = FD_FILE;
    
    int *p=(int *)malloc(sizeof(int));
    *p=2;
	//strncpy(g_pstServerLogHandle->szLogFilename, t_pFilename, MAX_PATH_LEN-1);
    strncpy(g_pstServerLogHandle->szLogFilename,t_pFilename,strlen(t_pFilename));

	if (0 != pthread_mutex_init(&g_pstServerLogHandle->stLogMutex,NULL)){
		return NULL;
	}
    /*
    *p=(int *)malloc(sizeof(int));
       *p=2;
        exit(1);
    */
	//根据传入的文件名来确定向哪里写入文件
	if (strncmp(g_pstServerLogHandle->szLogFilename,"/dev/null",strlen("/dev/null")) == 0){
		//
		g_pstServerLogHandle->iLogStd = FD_DEVNULL;
	}
	else if (strncmp(g_pstServerLogHandle->szLogFilename, "stdout", strlen("stdout")) == 0){
		g_pstServerLogHandle->iLogStd = FD_STDOUT;
	}
	else{
		//normal file
		if (access(g_pstServerLogHandle->szLogFilename, F_OK | R_OK | W_OK) == 0){
			//文件存在
			g_pstServerLogHandle->iLogFd = open(g_pstServerLogHandle->szLogFilename, O_RDWR | O_APPEND, 0644);
		}
		else{
			g_pstServerLogHandle->iLogFd = open(g_pstServerLogHandle->szLogFilename, O_CREAT | O_RDWR | O_APPEND, 0644);
		}
		if (g_pstServerLogHandle->iLogFd == -1){
			//open error
			pthread_mutex_destroy(&g_pstServerLogHandle->stLogMutex);
			free(g_pstServerLogHandle);
			g_pstServerLogHandle = NULL;
			return NULL;
		}
		g_pstServerLogHandle->iLogStd = FD_FILE;
	}

        //int *p=(int *)malloc(sizeof(int));
          //  *p=2;
	return g_pstServerLogHandle;
}


/**
* 销毁文件句柄
*/
void DestroyLogfileHandle(ServerLogHandle *t_pstServerLogHandle){
	if (NULL != t_pstServerLogHandle){
		pthread_mutex_destroy(&t_pstServerLogHandle->stLogMutex);
		if (t_pstServerLogHandle->iLogFd >= 0){
			close(t_pstServerLogHandle->iLogFd);
		}

		free(g_pstServerLogHandle);
		g_pstServerLogHandle = NULL;
	}
}


/*
*LOG通用函数(供宏调用的通用函数)
*/
void __LogMsg(ServerLogHandle *t_pstServerLogHandle, \
	const char *t_pFilename, \
	int t_iLinePos, \
	int t_iLogLevel, \
	const char *t_formatter, ...
	);


/*
int main(void) {
	//test for serverlog
	//ServerLogHandle* g_pstServerLogHandle = NULL;	//放在主进程main中即可
    g_pstServerLogHandle=CreateLogfileHandle("./test.log",5, 1024 * 1024 * 10, 5);
    printf("%p\n",g_pstServerLogHandle);
    LOG_ERROR("create child process pid=%d", getpid());
	return 0;
}
*/
