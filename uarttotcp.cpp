#include "uarttotcp.h"
using namespace std;

UARTTOTCP::UARTTOTCP(string uart, int baud, int port)
{
    _uart = uart.c_str();
    _baud = baud;
	
    // 初始化服务器地址和端口
    serverAddr.sin_family = PF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
 
    // 初始化socket
    listener = 0;
    
    // epool fd
    epfd = 0;
    
    // 清空buf
    memset(buf,0,BUF_SIZE);
    
    connectedJudgeOn = false;
    _interval = 2000; //ms
}

void UARTTOTCP::init()
{
    cout << "Init Serial..." << endl;

    if((serialFd = open(_uart, O_RDWR|O_NOCTTY|O_NDELAY|O_NONBLOCK))<0)
	perror("serialopen failded");
    else   
	setSerialOpt(serialFd, 57600, 8, 'N', 1);
    
    /*wiringPiSetup();
	serialFd = serialOpen(_uart,_baud);
    if(serialFd < 0)
    {
        perror("serialOpen");
    }*/
    
    cout << "Init Server..." << endl;
     //创建监听socket
    listener = socket(PF_INET, SOCK_STREAM, 0);
    if(listener < 0) {
        perror("listener");
        exit(-1);
    }
    
    // 设置套接字选项避免地址使用错误  
    int on=1;  
    if((setsockopt(listener,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on)))<0)  
    {  
        perror("setsockopt failed");  
        exit(EXIT_FAILURE);  
    }
    
    //绑定地址
    if(bind(listener, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("bind error");
        exit(-1);
    }
 
    //监听
    int ret = listen(listener, 4);
    if(ret < 0) {
        perror("listen error"); 
        exit(-1);
    }
 
    //在内核中创建事件表 epfd是一个句柄 
    epfd = epoll_create (EPOLL_SIZE);
    if(epfd < 0) {
        perror("epfd error");
        exit(-1);
    }
    
    cout << "Start to linten: "
         << inet_ntoa(serverAddr.sin_addr) << ":"
         << ntohs(serverAddr.sin_port) << endl;
    
    //往事件表里添加监听事件
    addfd(epfd, listener, true);
}

int UARTTOTCP::setSerialOpt(int fd,int nSpeed, int nBits, char nEvent, int nStop)
{
    struct termios newtio,oldtio;
    //读出原来的配置信息，按理说之所以读出来，是有部分数据不需要改，但感觉这里是这里对新的结构体配置，也没用到读出来的值
    if  ( tcgetattr( fd,&oldtio)  !=  0) { 	
	    perror("SetupSerial 1");
	    return -1;
    }
    bzero( &newtio, sizeof( newtio ) );		//新结构体清0
    newtio.c_cflag  |=  CLOCAL | CREAD;
    newtio.c_cflag &= ~CSIZE;

    switch( nBits )
    {
    case 7:
	    newtio.c_cflag |= CS7;
	    break;
    case 8:
	    newtio.c_cflag |= CS8;
	    break;
    }
    /***		奇偶校验		***/
    switch( nEvent )
    {
    case 'O':
	    newtio.c_cflag |= PARENB;
	    newtio.c_cflag |= PARODD;
	    newtio.c_iflag |= (INPCK | ISTRIP);
	    break;
    case 'E': 
	    newtio.c_iflag |= (INPCK | ISTRIP);
	    newtio.c_cflag |= PARENB;
	    newtio.c_cflag &= ~PARODD;
	    break;
    case 'N':  
	    newtio.c_cflag &= ~PARENB;
	    break;
    }
    /***		设置波特率		***/
    switch( nSpeed )
    {
	case 2400:
		cfsetispeed(&newtio, B2400);
		cfsetospeed(&newtio, B2400);
		break;
	case 4800:
		cfsetispeed(&newtio, B4800);
		cfsetospeed(&newtio, B4800);
		break;
	case 9600:
		cfsetispeed(&newtio, B9600);
		cfsetospeed(&newtio, B9600);
		break;
	case 57600:
		cfsetispeed(&newtio, B57600);
		cfsetospeed(&newtio, B57600);
		break;
	case 115200:
		cfsetispeed(&newtio, B115200);
		cfsetospeed(&newtio, B115200);
		break;
	case 460800:
		cfsetispeed(&newtio, B460800);
		cfsetospeed(&newtio, B460800);
		break;
	default:
		cfsetispeed(&newtio, B9600);
		cfsetospeed(&newtio, B9600);
		break;
    }
    /***		设置停止位		***/
    if( nStop == 1 )
	newtio.c_cflag &=  ~CSTOPB;
    else if ( nStop == 2 )
	newtio.c_cflag |=  CSTOPB;
    /*设置等待时间和最小接收字符*/
	newtio.c_cc[VTIME]  = 0;
	newtio.c_cc[VMIN] = 50;			//设置阻塞的最小字节数，阻塞条件下有效
    /*处理未接收字符*/ 
	tcflush(fd,TCIFLUSH);
    /*激活新配置*/
    if((tcsetattr(fd,TCSANOW,&newtio))!=0)
    {
	perror("com set error");
	return -1;
    }
    
    //	printf("set done!\n");
    return 0;
}

void UARTTOTCP::start()
{
    // epoll 事件队列
    static struct epoll_event events[EPOLL_SIZE]; 
 
    // 初始化
    init();
    
    //循环
    while(1)
    {
        //epoll_events_count表示就绪事件的数目
        int epoll_events_count = epoll_wait(epfd, events, EPOLL_SIZE, 0);
 
        if(epoll_events_count < 0) {
            perror("epoll_wait failure");
            break;
        }
 
        //处理这epoll_events_count个就绪事件
        for(int i = 0; i < epoll_events_count; ++i)
        {
            int sockfd = events[i].data.fd;
            //新TCP Client连接
            if(sockfd == listener)
            {
                struct sockaddr_in client_address;
                socklen_t client_addrLength = sizeof(struct sockaddr_in);
                int clientfd = accept(listener, ( struct sockaddr* )&client_address, &client_addrLength );
 
                cout << "client connection from: "
                     << inet_ntoa(client_address.sin_addr) << ":"
                     << ntohs(client_address.sin_port) << ", clientfd = "
                     << clientfd << endl;

                addfd(epfd, clientfd, true);
 
                // 服务端用list保存用户连接
                clients_list.push_back(clientfd);
		timespec thisTime;
		clock_gettime(CLOCK_MONOTONIC,&thisTime);
		timeRecord.insert(pair<int, timespec>(clientfd,thisTime));
                cout << "Add new clientfd = " << clientfd << " to epoll. "
					 << "Now there are " << clients_list.size() << " tcp clients" << endl;
		cout <<"timeRecord.size: "<<timeRecord.size()<<endl;
		
            }
            //处理Client发来的消息，并转发给serialFd
            else {
		memset(buf,0,BUF_SIZE);
                int recvLen = recv(sockfd, buf, BUF_SIZE, 0);
                
                if(recvLen == 0) 
                {
                    close(sockfd);
                    // 在客户端列表中删除该客户端
                    clients_list.remove(sockfd);
		    timeRecord.erase(sockfd);
                    cout << "ClientID = " << sockfd 
                         << " closed. now there are " 
                         << clients_list.size()
                         << " clients"
                         << endl;
                }else if(recvLen > 0){
		    cout << "Recv "<< recvLen << "bytes" << "from client"
			     << sockfd << ": " << buf <<endl;
		    if(write(serialFd,buf,recvLen)<0)
		    {
			perror("write to serial");
			exit(-1);
		    }
		    map<int,timespec>::iterator it = timeRecord.find(sockfd);
		    if(it != timeRecord.end())
			clock_gettime(CLOCK_MONOTONIC,&(it->second));
		}else{
		    close(sockfd);
                    // 在客户端列表中删除该客户端
                    clients_list.remove(sockfd);
		    timeRecord.erase(sockfd);
		    cout << "This sockfd " << sockfd << " disconnects abnomally"; 
		}
            }
        }
        
        int recvLen = read(serialFd,buf,BUF_SIZE);
	if(recvLen > 0)
	{
            buf[recvLen] = '\0';
	    cout << "Recv " << recvLen << "bytes" << "from serial " << _uart
		     << ": " << buf << endl;
	    if(clients_list.size() > 0)
	    {
		list<int>::iterator it;
		
		for(it=clients_list.begin(); it!=clients_list.end(); ++it)
		{
		    if(send(*it,buf,recvLen,0)<=0)
		    {
			perror("send to client");
			close(*it);
			// 在客户端列表中删除该客户端
			clients_list.remove(*it);
			timeRecord.erase(*it);
		    }
		}
	    }else
		cout << "There is no clients" << endl;
	}
	
	if(connectedJudgeOn)
	    connectedJudge();
    }
    // 关闭服务
    Close();
}

void UARTTOTCP::Close()
{
    //关闭socket
    close(listener);
    
    //关闭epoll监听
    close(epfd);
    
    clients_list.clear();
    timeRecord.clear();
}

void UARTTOTCP::setConnectedJudge(bool on, int interval)
{
    connectedJudgeOn = on;
    _interval = interval;
}

void UARTTOTCP::connectedJudge()
{
    static timespec lastTime;
    if(timeRecord.size() > 0)
    {
	clock_gettime(CLOCK_MONOTONIC,&now);
	int interval = (now.tv_sec-lastTime.tv_sec)*1000+(now.tv_nsec-lastTime.tv_nsec)/1000000;
	if(interval > _interval)
	{
	    map<int,timespec>::iterator it;
	    for(it=timeRecord.begin(); it!=timeRecord.end(); ++it)
	    {
		interval = (now.tv_sec-it->second.tv_sec)*1000+(now.tv_nsec-it->second.tv_nsec)/1000000;
		//cout << "interval = " << interval << endl;
		if(interval > _interval)
		{
		    //cout << "There was " << timeRecord.size() <<" "<<clients_list.size() <<endl;
		    close(it->first);
		    clients_list.remove(it->first);
		    timeRecord.erase(it);
		    //cout << "There is " << timeRecord.size() <<" "<<clients_list.size() <<endl;
		    break;
		}
	    }
	    lastTime = now;
	}
    }
}
