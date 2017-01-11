#include "net_api.h"
#include "log_env.h"


#define MAX_RECV_LEN 1024*10;

CNet* CNet::m_cNetIns = NULL;
CNet::CNet()
{
	m_eventBase = NULL;
	pthread_mutex_init(m_mutexLock, NULL);
	m_recvBuffer = NULL;

	m_recvBuffer = new char[MAX_RECV_LEN];
	if( m_recvBuffer == NULL )
	{
		LOG_NET_ERROR(" m_recvBuffer new space failed\n");
		exit(-1);
	}

	memset(m_recvBuffer, '\0', MAX_RECV_LEN);
}

CNet::~CNet()
{
	pthread_mutex_destroy(m_mutexLock);
	if( m_recvBuffer != NULL )
	{
		delete []m_recvBuffer;
	}
}

CNet* CNet::GetInstance()
{
	if( m_cNetIns == NULL )
	{
		m_cNetIns = new CNet;
	}

	return m_cNetIns;
}

void CNet::ReleaseInstance()
{
	if( m_cNetIns != NULL )
	{
		delete m_cNetIns;
		m_cNetIns = NULL;
	}
}

int CNet::startNetServer()
{

  	m_eventBase = event_base_new();

	if( m_eventBase == NULL )
	{
		
		LOG_NET_ERROR("Net Event start error \n");
		
		return -1;
	}
	LOG_NET_INFO(" Net Event start\n");
	event_base_dispatch(m_eventBase);
	return 0;
}

void CNet::stopNetServer()
{
}

struct event_base* CNet::getEventBase()
{
	return m_eventBase;
}

// 读事件  
void onReadEvent(int clifd,short ievent,void *arg)  
{  

    int ilen;  
    char *pBuf = CNet::GetInstance()->getRecvBuffer();  
  
    ilen = recv(clifd, pBuf, MAX_RECV_LEN, 0);  
    if(ilen <= 0)  
    {     
        struct event *pread = (struct event*)arg;  
        event_del(pread);  
        delete pread;  
        close(clifd);  
        return;  
    }       
	CNetBase* pNetBase = CNet::GetInstance()->getHandle(clifd);
	if( pNetBase == NULL )
	{
		LOG_NET_INFO("not found the socketFd NetBaseHanlde sockFd=%d\n", clifd);
		return;
	}

	pNetBase->OnReceive(pBuf, ilen);
}  

// 连接事件  
void onAcceptEvent(int svrfd,short ievent,void *arg)  
{  
    int clifd;  
    struct sockaddr_in cliaddr;  
  
    socklen_t sinsize = sizeof(cliaddr);  
    clifd = accept(svrfd,(struct sockaddr*)&cliaddr,&sinsize);  
	
    struct event *eventHandle = new event;  
    event_set(pread,clifd,EV_READ|EV_PERSIST,onReadEvent,eventHandle);  // 注册读(写)事件  

	struct event_base *base = CNet::GetInstance()->getEventBase();
	event_base_set(base,eventHandle);  
    event_add(eventHandle,NULL);  
}



int CNet::listenTcpServer(char *ip, int port)
{
	 int svrfd;  
    struct sockaddr_in svraddr;  
  
    memset(&svrfd,0,sizeof(svraddr));  
    svraddr.sin_family = AF_INET;  
    svraddr.sin_port = htons(port);  
    svraddr.sin_addr.s_addr = inet_addr(ip);  
  
    svrfd = socket(AF_INET,SOCK_STREAM,0);  
    bind(svrfd,(struct sockaddr*)&svraddr,sizeof(svraddr));  
  
    listen(svrfd,10);  
  
    struct event evlisten;  
    event_set(&evlisten,svrfd,EV_READ|EV_PERSIST,onAcceptEvent,NULL);  
  
    // 设置为base事件  
    event_base_set(m_eventBase,&evlisten);  
    // 添加事件  
    event_add(&evlisten,NULL);  
	return 0;
	
}
int CNet::connectTcpServer(char* ip, int port)
{
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if( sockfd <= 0 )
	{
		LOG_NET_ERROR(" create a socket err:%s\n", stderror(errno));
		return -1;
	}
	struct sockaddr_in servaddr
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &servaddr.sin_addr);
    servaddr.sin_port = htons(port);
    
    if( (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr))) == INVALID_SOCKET)
    {
    	LOG_NET_ERROR("connenct server(%s:%d) error:%s\n", ip, port, stderror(errno));
		return -1;
    }

	struct event *eventHandle = new event;  
    event_set(pread,clifd,EV_READ|EV_PERSIST,onReadEvent,eventHandle);  // 注册读(写)事件  

	struct event_base *base = CNet::GetInstance()->getEventBase();
	event_base_set(base,eventHandle);  
    event_add(eventHandle,NULL); 

	return 0;
}

int CNet::sendTcpData(int socketFd, char* buf, int len)
{
}
int CNet::readTcpData(int socketFd, char* buf, int len)
{
	return 0;
}

char * CNet::getRecvBuffer()
{
	return m_recvBuffer;
}

int CNet::setNetHandle(int Fd,CNetBase* handle)
{
	pthread_mutex_lock(m_mutexLock);
	std::map<int, CNetBase*>::iterator pos;
	for(pos = m_cNetBaseMap.begin(); pos != m_cNetBaseMap.end(); pos++)
	{
		if( pos->first == Fd )
		{
			LOG_NET_INFO("the socket is already exit socketFd=%d\n", Fd);
			pthread_mutex_unlock(m_mutexLock);
			return 0;
		}
	}

	m_cNetBaseMap[Fd] = handle;
	pthread_mutex_unlock(m_mutexLock);
	return 0;
}


int CNet::getSocketFd(CNetBase* handle)
{
	int sockFd = 0;
	pthread_mutex_lock(m_mutexLock);
	std::map<int, CNetBase*>::iterator pos;
	for(pos = m_cNetBaseMap.begin(); pos != m_cNetBaseMap.end(); pos++)
	{
		if( pos->second == handle )
		{
			sockFd = pos->first;
			break;
		}
	}

	pthread_mutex_unlock(m_mutexLock);
	return sockFd;
}


CNetBase* CNet::getHandle(int Fd)
{
	CNetBase* handle = NULL;
	pthread_mutex_lock(m_mutexLock);
	std::map<int, CNetBase*>::iterator pos;
	for(pos = m_cNetBaseMap.begin(); pos != m_cNetBaseMap.end(); pos++)
	{
		if( pos->first == Fd )
		{
			handle = pos->second;
			break;
		}
	}

	pthread_mutex_unlock(m_mutexLock);
	return handle;
}


