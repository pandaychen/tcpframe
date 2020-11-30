/*
	a http/tcp benchmark client,using reactor model(单进程)
	一个worker可以代表一个线程/进程,一个client代表一个connection
	一个worker可以有很多个client

	优化的目标
	1.改成并发模式--多进程/多线程的版本
	2.random-url/random-ua/random-cgi
	3.支持302跳转/200跳转
*/

#include <unistd.h>
#include <getopt.h>
#include "worker.h"

//单进程测试,全局变量

StatisticsStruct config;

struct option longopts[] = {
	{ "concurrency", required_argument, NULL, 'c' },
	{ "requests", required_argument, NULL, 'n' },
	{ "host", required_argument, NULL, 'h' },
	{ "port", required_argument, NULL, 'p' },
	{ "keepalive", no_argument, NULL, 'k' },
	{ "random", no_argument, NULL, 'r' },
	{ "protocol", no_argument, NULL, 'u' },
	//{ "help", no_argument, NULL, 'h' },
	{ 0, 0, 0, 0 },
};

/*
	单进程测试
*/
int main(int argc, char** argv)
{
	//IgnoreSignal();
	
	InitConfig(&config);

    ParseOptions(argc, argv,&config);
	
	//创建本进程需要的reactor资源
	InitSingleReactor(&config);
	
	//事件loop
	ReactorBenchmarkLoop(&config);
	
	FreeResource(&config);

	return 0;
}
