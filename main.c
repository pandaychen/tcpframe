/*
* ProjectInfo:多进程框架，数据接收与发送无任何拷贝与上下文切换
* File name:  main.c
* @author:    pandaychen
* @version:     
* Description: 	socket多进程实现,监控子进程-Master-Worker模型
* Log:
*/

#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/resource.h>


#include "reactor.h"
#include "socket_api.h"
#include "socket_util.h"
#include "server_log.h"
#include "comm.h"

ServerLogHandle *g_pstServerLogHandle;

void SetTestLoop()
{
    while(1){
        printf("sleep 1s cur pid=%d\n",getpid());
        sleep(1);
    }
}

/*
	先将主进程daemon化
*/
void InitDaemonProcess()
{
	//设置文件打开限制和Core文件生成
	struct rlimit rlim, rlim_new;
	//rlim --save old

	memset(&rlim_new, 0, sizeof(struct rlimit));
	memset(&rlim, 0, sizeof(struct rlimit));

	//RLIMIT_NOFILE 
	//指定比进程可打开的最大文件描述词大一的值，超出此值，将会产生EMFILE错误

	//RLIMIT_CORE 
	//内核转存文件的最大长度


	if (getrlimit(RLIMIT_NOFILE, &rlim) == 0)
	{
		/*
		stlimitNew.rlim_cur = 200000;	//soft limit
		stlimitNew.rlim_max = 200000;	//hard limit
		*/
		rlim_new.rlim_cur = rlim_new.rlim_max = SYSTEM_FILE_MAX;

		if (setrlimit(RLIMIT_NOFILE, &rlim_new) != 0)
		{
			//恢复为默认值
			LOG_ERROR("Setrlimit file Fail, use old rlimit,use old.");
			rlim_new.rlim_cur = rlim_new.rlim_max = rlim.rlim_max;
			setrlimit(RLIMIT_NOFILE, &rlim_new);
		}
	}

	memset(&rlim_new, 0, sizeof(struct rlimit));
	memset(&rlim, 0, sizeof(struct rlimit));
	//设置CORE文件大小
	if (getrlimit(RLIMIT_CORE, &rlim) == 0)
	{
		rlim_new.rlim_cur = rlim_new.rlim_max = RLIM_INFINITY;
		//设置core文件为unlimited
		if (setrlimit(RLIMIT_CORE, &rlim_new) != 0)
		{
			LOG_ERROR("Setrlimit core Fail, use old.");
			rlim_new.rlim_cur = rlim_new.rlim_max = rlim.rlim_max;
			setrlimit(RLIMIT_CORE, &rlim_new);
		}
	}


	signal(SIGPIPE, SIG_IGN);
	pid_t pid;

	if ((pid = fork()) != 0)
	{
		//父进程退出一次
		exit(0);
	}

	setsid();
	signal(SIGINT, SIG_IGN);
	signal(SIGHUP, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGTTOU, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);
	signal(SIGCHLD, SIG_IGN);
	signal(SIGTERM, SIG_IGN);

	//为何要再屏蔽一次SIGHUP信号
	struct sigaction sig;
	sig.sa_handler = SIG_IGN;
	sig.sa_flags = 0;
	sigemptyset(&sig.sa_mask);
	sigaction(SIGHUP, &sig, NULL);

	if ((pid = fork()) != 0)
	{
		//父进程再退出一次
		exit(0);
	}

	umask(0);
	setpgrp();

	return;
}

void ParentSigHandler(int sig)
{
	LOG_ERROR("Main Process PID=%d recv SIGALRM signal, notify child process exit", getpid());
	//向本进程组的所有子进程发送SIGTERM命令
	kill(0, SIGTERM);

	//主进程退出
	exit(0);
}

void ChildSigHandler(int sig)
{
	LOG_ERROR("Child Process PID=%d recv SIGALRM signal, exit", getpid());
	//子进程收到SIGTERM信号后退出
	exit(0);
}

void printUsage()
{
	printf("./connsvr listenip listenport process_num maxclients_num daemon_sign\n");
	return;
}

int main(int argc, char* argv[])
{
	if (argc < 5){
		printUsage();
		return -1;
	}

	//改成配置文件
	const char* szServerIP = argv[1];
	int iServerPort = atoi(argv[2]);
	int iWorkerCount = atoi(argv[3]);
	int iMaxClientsCount = atoi(argv[4]);
	int iDaemon = atoi(argv[5]);

	if (iWorkerCount <= 0){
		iWorkerCount = 1;
	}

	if (1 == iDaemon){
		InitDaemonProcess();
	}

	/*
	////默认处理方式，是接收子进程的返回值(由于在damon中已经默认忽略了SIGCHLD,所以这里要显式恢复,不然会导致waitpid异常)
	//如果在父进程已经signal(SIGCHLD,SIG_IGN)；那么子进程结束时，子进程的返回值不能被waitpid接收。
	//这个是必须关注的问题。
	*/
	signal(SIGCHLD, SIG_DFL);
	signal(SIGTERM, ParentSigHandler);


	//主进程中创建LOG句柄
	g_pstServerLogHandle = CreateLogfileHandle("./log/connsvr.log", 5, 1024 * 10240, 10);

	if (NULL == g_pstServerLogHandle){
		fprintf(stderr, "Can not create log file\n");
		exit(-1);
	}

	int iServerFd = 0;

	//创建server
    iServerFd = PynServerCreateTcpServer(szServerIP, iServerPort);
	if (0 > iServerFd){
		LOG_ERROR("create server %s:%d failed", szServerIP, iServerPort);
		return -1;
	}

	int iChildOrParent = 0;
	int iPid = 0;

	//master-worker-loop
	while (!iChildOrParent){
		if (iWorkerCount > 0)
		{
			iPid = fork();
			switch (iPid){
			case -1:
				LOG_ERROR("fork error,%s", strerror(errno));
				return -1;

			case 0:
				//child work here
				iChildOrParent = 1;
				break;

			default:
				//parent work here
				LOG_INFO("create child process pid=%d ok", iPid);
				iWorkerCount--;
				break;
			}
		}
		else{
			//parent goes here
			int iStatus;
			//改进:用waitpid替换
			int iWaitPid = wait(&iStatus);

			if (iWaitPid != -1){
				//succ collect a child exit
				iWorkerCount++;
				LOG_ERROR("moinitor process pid =%d exit", iWaitPid);
			}
			else{
				LOG_ERROR("wait return error, %s", strerror(errno));
				break;
			}
		}
	}


	if (!iChildOrParent){
		//parent goes here
		LOG_ERROR("Main Process exit");
		//向进程组的所有子进程发送信号
		kill(0, SIGTERM);
		return 0;
	}

	//在子进程中重新创建日志句柄,防止多进程同时写同一日志与滚动(首先销毁fork继承的文件handler)
	DestroyLogfileHandle(g_pstServerLogHandle);

	char szLogNameNew[1024] = { 0 };
	snprintf(szLogNameNew, sizeof(szLogNameNew)-1, "./log/connsvr-worker-%d", getpid());
	g_pstServerLogHandle = CreateLogfileHandle(szLogNameNew, 5, 1024 * 1024 * 10, 5);

	if (NULL == g_pstServerLogHandle){
		fprintf(stderr, "Can't create log file\n");
		return -1;
	}

	//在子进程中注册SIGTERM信号的回调函数,以便接收父进程退出时的信号
	signal(SIGTERM, ChildSigHandler);

	//Info("create child process pid=%d", getpid());
	//int iServerFd = PynServerCreateTcpServer(szServerIP, iServerPort);

	//创建本子进程的reactor核心结构
	EventBase *pstEventBase = (EventBase *)EventBaseCreate(iServerFd,iMaxClientsCount);
	if (NULL == pstEventBase)
	{
		LOG_ERROR("Can't create reactor loop: %s", strerror(errno));
		return -1;
	}
    
	//开启每个子进程的事件循环
	ReactorEventDispatchLoop(pstEventBase);

	return 0;
}

