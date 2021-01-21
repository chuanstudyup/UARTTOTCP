#ifndef UARTTOTCP_H
#define UARTTOTCP_H

#include "common.h"
#include <termios.h>
#include <time.h>
#include <map>

using namespace std;

class UARTTOTCP{
public:
	UARTTOTCP(string uart, int baud, int port);
	void start();
	void Close();
	void setConnectedJudge(bool on, int interval = 2000);
	
private:
	// 串口号
	const char* _uart;
	// 串口波特率
	int _baud;
	// 串口描述符
	int serialFd;
	// 服务器端serverAddr信息
	struct sockaddr_in serverAddr;
	//创建监听的socket
	int listener;
	// epoll_create创建后的返回值
	int epfd;
	// 客户端列表
	list<int> clients_list;
	// 中转缓存区
	char buf[BUF_SIZE];
	// 记录client最近一次发送数据的时间
	map<int,timespec> timeRecord;
	// TCP连接检测
	timespec now;
	int _interval;  //ms
	bool connectedJudgeOn;
	void connectedJudge();
	
	void init();
	
	int setSerialOpt(int fd,int nSpeed, int nBits, char nEvent, int nStop);
};
#endif  //UARTTOTCP_H
