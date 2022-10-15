#include <stdio.h>
#include <string.h>
#include <Windows.h>
#include <process.h>
#include <Winsock.h>
#include <stdlib.h>
#include <conio.h>
#include <time.h>
#pragma comment(lib, "ws2_32.lib")
#define UNSUPPORT_METHOD_TIME_SLEEP 1000
#define MAX_CACHE_NUM 1000
#define MAX_TCP_CONNECT 25
#define CACHE_DELIM "||||"
#define MAXSIZE 150000//65507 //发送数据报文的最大长度
#define HTTP_PORT 80  // http 服务器端口
#define B_QUIT 'Q'
#define FILE_DELIM "||||||||"
int proexit(int err);
struct CACHE
{
	char url[1024];
	char cookie[10 * 1024];
	char date[50];
	char path[10];
}cache[MAX_CACHE_NUM] = { 0 };
FILE* fcache;
FILE* fcachelist[MAX_CACHE_NUM] = { 0 };
int cachenum = 0;

void writeShare();
int forbidIpNum = 0;
char forbidIp[100][20] = { 0 };
typedef struct ForbidWeb {
	char url[200];
	int len;
} FORBIDWEB;
FORBIDWEB forbidWeb[100] = { 0 };
FORBIDWEB tipFishWeb[100] = { 0 };
int forbidWebNum = 0;
int tipFishWebNum = 0;

const char* getWSAErrorText() {
	switch (WSAGetLastError())
	{
	case 0: return "Directly send error";
	case 10004: return "Interrupted function";//call 操作被终止
	case 10013: return "Permission denied";//c访问被拒绝
	case 10014: return "Bad address"; //c地址错误
	case 10022: return "Invalid argument"; //参数错误
	case 10024: return "Too many open files";// 打开太多的sockets
	case 10035: return "Resource temporarily unavailable"; // 没有可以获取的资料
	case 10036: return "Operation now in progress"; // 一个阻塞操作正在进行中
	case 10037: return "Operation already in progress";// 操作正在进行中
	case 10038: return "Socket operation on non-socket";//非法的socket对象在操作
	case 10039: return "Destination address required"; //目标地址错误
	case 10040: return "Message too long";//数据太长
	case 10041: return "Protocol wrong type for socket"; //协议类型错误
	case 10042: return "Bad protocol option";// 错误的协议选项
	case 10043: return "Protocol not supported"; //协议不被支持
	case 10044: return "Socket type not supported"; //socket类型不支持
	case 10045: return "Operation not supported"; //不支持该操作
	case 10046: return "Protocol family not supported";//协议族不支持
	case 10047: return "Address family not supported by protocol family";//使用的地址族不在支持之列
	case 10048: return "Address already in use"; //地址已经被使用
	case 10049: return "Cannot assign requested address";//地址设置失败
	case 10050: return "Network is down";//网络关闭
	case 10051: return "Network is unreachable"; //网络不可达
	case 10052: return "Network dropped connection on reset";//网络被重置
	case 10053: return "Software caused connection abort";//软件导致连接退出
	case 10054: return "connection reset by peer"; //连接被重置
	case 10055: return "No buffer space available"; //缓冲区不足
	case 10056: return "Socket is already connected";// socket已经连接
	case 10057: return "Socket is not connected";//socket没有连接
	case 10058: return "Cannot send after socket shutdown";//socket已经关闭
	case 10060: return "Connection timed out"; //超时
	case 10061: return "Connection refused"; //连接被拒绝
	case 10064: return "Host is down";//主机已关闭
	case 10065: return "No route to host";// 没有可达的路由
	case 10067: return "Too many processes";//进程太多
	case 10091: return "Network subsystem is unavailable";//网络子系统不可用
	case 10092: return "WINSOCK.DLL version out of range"; //winsock.dll版本超出范围
	case 10093: return "Successful WSAStartup not yet performed"; //没有成功执行WSAStartup
	case 10094: return "Graceful shutdown in progress";//
	case 11001: return "Host not found"; //主机没有找到
	case 11002: return "Non-authoritative host not found"; // 非授权的主机没有找到
	case 11003: return "This is a non-recoverable error";//这是个无法恢复的错误
	case 11004: return "Valid name, no data record of requested type";//请求的类型的名字或数据错误

	default:
		return "未知错误";
	}
}

struct ProxyParam
{
	SOCKET clientSocket;
	SOCKET serverSocket;
};
struct Job {
	char host[64];
	char srcIpText[16];
	ProxyParam socket;	
	unsigned long srcIp;
	unsigned long dstIp;
	unsigned long srcPort;
	unsigned long dstPort;
	int jobid;
	int status;
	int jobindex;
};
int jobClose[MAX_TCP_CONNECT];
int jobcacheidlist[MAX_TCP_CONNECT];

struct  SharedArea
{
	Job jobs[MAX_TCP_CONNECT];
	bool usingjob[MAX_TCP_CONNECT] = { false };
}SHAREDATA;

struct ControlArea {
	bool stop = false;
	bool hidehttps = false;
}CONTROLDATA;

//代理相关参数
SOCKET ProxyServer;
sockaddr_in ProxyServerAddr;
const int ProxyPort = 10240;

// Http 重要头部数据
struct HttpHeader
{
	char method[8];         // POST 或者 GET，注意有些为 CONNECT，本实验暂不考虑
	char url[1024];         // 请求的 url
	char host[1024];        // 目标主机
	char cookie[1024 * 10]; // cookie
	char connection[15];
	HttpHeader()
	{
		ZeroMemory((char*)this, sizeof(HttpHeader));
	}
};

// Http 重要头部数据
struct ResponseHeader
{
	int status;
	int keep;
	int contentlen;
	char date[50];
	ResponseHeader()
	{
		ZeroMemory((char*)this, sizeof(ResponseHeader));
	}
};
//由于新的连接都使用新线程进行处理，对线程的频繁的创建和销毁特别浪费资源
//可以使用线程池技术提高服务器效率
// const int ProxyThreadMaxNum = 20;
// HANDLE ProxyThreadHandle[ProxyThreadMaxNum] = {0};
// DWORD ProxyThreadDW[ProxyThreadMaxNum] = {0};


/*
SOCKET GetRemoteSocket(HttpHeader* httpHeader, sockaddr_in *RemoteSocketAddr) {
	SOCKET RemoteSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (INVALID_SOCKET == RemoteSocket)
	{
		printf("创建远程套接字失败，错误代码为：%d\n", WSAGetLastError());
		return FALSE;
	}
	RemoteSocketAddr->sin_family = AF_INET;
	RemoteSocketAddr->sin_port = 80;

	hostent *host = gethostbyname(httpHeader->host);
	printf("解析到服务器为[%d] %s", host->h_length, host->h_name);
	if (host->h_length <= 0) {
		return 0;
	}
	char* host_addr = host->h_addr_list[0];
	printf("解析到服务器IP地址为[%d] %s", 0, host_addr);
	unsigned long host_ip = atoi(host_addr);

	RemoteSocketAddr->sin_addr.S_un.S_addr = host_ip;
	/*
	if (bind(RemoteSocket, (SOCKADDR*)RemoteSocketAddr, sizeof(SOCKADDR)) == SOCKET_ERROR)
	{
		printf("绑定套接字失败\n");
		return FALSE;
	}
	return RemoteSocket;
}
*/
BOOL InitSocket();
BOOL ConnectToServer(SOCKET* serverSocket, char* host, Job* j);
void CloseSocket(LPVOID lpParameter, int id);
unsigned int __stdcall ProxyThread(LPVOID lpParameter);
//************************************
// Method: InitSocket
// FullName: InitSocket
// Access: public
// Returns: BOOL
// Qualifier: 初始化套接字
//************************************
BOOL InitSocket()
{
	//加载套接字库（必须）
	WORD wVersionRequested;
	WSADATA wsaData;
	//套接字加载时错误提示
	int err;
	//版本 2.2
	wVersionRequested = MAKEWORD(2, 2);
	//加载 dll 文件 Scoket 库
	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0)
	{
		//找不到 winsock.dll
		printf("加载 winsock 失败，错误代码为: %d\n", WSAGetLastError());
		return FALSE;
	}
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		printf("不能找到正确的 winsock 版本\n");
		WSACleanup();
		return FALSE;
	}
	ProxyServer = socket(AF_INET, SOCK_STREAM, 0);
	if (INVALID_SOCKET == ProxyServer)
	{
		printf("创建套接字失败，错误代码为：%d\n", WSAGetLastError());
		return FALSE;
	}
	ProxyServerAddr.sin_family = AF_INET;
	ProxyServerAddr.sin_port = htons(ProxyPort);
	ProxyServerAddr.sin_addr.S_un.S_addr = INADDR_ANY;
	if (bind(ProxyServer, (SOCKADDR*)&ProxyServerAddr, sizeof(SOCKADDR)) == SOCKET_ERROR)
	{
		printf("绑定套接字失败\n");
		return FALSE;
	}
	if (listen(ProxyServer, SOMAXCONN) == SOCKET_ERROR)
	{
		printf("监听端口%d 失败", ProxyPort);
		return FALSE;
	}
	return TRUE;
}


void cacheClose(int index) {
	if (fcachelist[index] != NULL) {
		fclose(fcachelist[index]);
		fcachelist[index] = NULL;
	}
}
//续写
int cacheWrite(int index, char* buffer, int length, int close) {
	if (index >= MAX_CACHE_NUM) return -1;
	if (index < 0) return -1;
	if (fcachelist[index] == NULL) return -1;
	//fopen_s(&fcachelist[index], cache[index].path,"w");
	//if (fcachelist[index] == NULL) return -1;
	FILE* fptr;
	char path[50] = "../Cache/";
	//char tar[50] = {0};
	//char pink[50] = { 0 };
	//sprintf_s(pink, 50, "%d_%s", index, tar);
	//_itoa_s((int)rand(), tar, 50, 10);
	//sprintf_s(pink, 50, "%d_%s", index,tar);
	sprintf_s(path, 50, "../Cache/%d_%d_%d", index, rand()+rand(),length);
	fopen_s(&fptr, path, "w");
	if (fptr == NULL) {
		printf("创建目标缓存文件失败 [%s]\n", path);
		return 0;
	}
	printf("目标缓存文件 [%s]\n", path);
	int ret = fwrite(buffer, sizeof(char), length, fptr);
	fclose(fptr);
	fwrite(path, sizeof(char), strlen(path), fcachelist[index]);
	fwrite("\n", sizeof(char), 1, fcachelist[index]);
	fflush(fcachelist[index]);
	//int c = fclose(fcachelist[index]);
	//printf("缓存关闭状态：%d\n", c);
	return ret;
}
int cacheCheck(char* url, char* cookie) {
	printf("正在检查缓存数据\n%s\n", url);
	for (int i = 0; i < cachenum; i++) {
		if (strcmp(cache[i].url, url) == 0) {
			printf("存在缓存数据[%d]：[%s]\n", i, cache[i].path);
			return i;
		}
	}
	return -1;
}

int cacheCreate(char* url, ResponseHeader rh, char* cookie) {
	int index = cacheCheck(url, cookie);
	if (index == -1) {
		index = cachenum++;
	}
	else {
		printf("## 修改缓存文件\n");
	};
	char path[100] = { 0 };
	_itoa_s(index, path, 10);
	char fpath[200] = "../Cache/";
	strcat_s(fpath, path);
	fopen_s(&fcachelist[index], fpath, "w");
	printf("创建缓存文件[%s]\n", fpath);
	if (NULL == fcachelist[index]) {
		printf("失败\n");
		return -1;
	}
	//fclose(fcachelist[index]);
	
	char ibuf[20] = { 0 };
	_itoa_s(index, ibuf, 10);
	char* tbuf = new char[20000];
	
	/*fwrite(url, sizeof(char), strlen(url), fcache);
	fwrite(CACHE_DELIM, sizeof(char), strlen(CACHE_DELIM), fcache);
	fwrite(path, sizeof(char), strlen(path), fcache);
	fwrite(CACHE_DELIM, sizeof(char), strlen(CACHE_DELIM), fcache);
	fwrite(rh.date, sizeof(char), strlen(rh.date), fcache);
	fwrite(CACHE_DELIM, sizeof(char), strlen(CACHE_DELIM), fcache);
	fwrite(cookie, sizeof(char), strlen(cookie), fcache);
	fwrite(CACHE_DELIM, sizeof(char), strlen(CACHE_DELIM), fcache);
	
	fwrite(ibuf, sizeof(char), strlen(cookie), fcache);
	fwrite(CACHE_DELIM, sizeof(char), strlen(CACHE_DELIM), fcache);
	fwrite("\n", sizeof(char), 1, fcache);*/
	
	sprintf_s(tbuf, 20000, "%s%s%s%s%s%s%s%s%s%s\n", url, CACHE_DELIM, path, CACHE_DELIM, rh.date, CACHE_DELIM, cookie, CACHE_DELIM, ibuf, CACHE_DELIM);
	fwrite(tbuf, sizeof(char), strlen(tbuf), fcache);
	fflush(fcache);

	strcpy_s(cache[index].url, url);
	strcpy_s(cache[index].path, path);
	strcpy_s(cache[index].date, rh.date);
	strcpy_s(cache[index].cookie, cookie);


	delete[]tbuf;
	return index;
}

int recvData(SOCKET s, char * buffer, int *hasRecv, int contentlen) {
	bool load = true;
	int recvSize;
	int i = 0;
	while (load) {

		if (*hasRecv >= MAXSIZE) {
			printf("分段接收失败，超过最大长度！立即发送给客户！\n");
			return -1;
		}
		i++;
		printf("[%d/%d]正在分段接收HTTP报文[%d]：最后结尾4个字符为：[%s]\n", *hasRecv, contentlen, i, buffer + *hasRecv - 4);
		if (!strcmp(buffer + *hasRecv - 4, "\r\n\r\n") || *hasRecv > contentlen) {
			printf("分段接收成功结束！\n");
			return 0;
		}

		recvSize = recv(s, buffer + *hasRecv, MAXSIZE - *hasRecv, 0);
		if (recvSize <= 0) {
			printf("分段接收服务器数据失败，服务器可能已关闭连接！分段接收结束！%s",getWSAErrorText());
			return 1;
		}
		*hasRecv += recvSize;
	}
	
}


int ParseResponse(char* buffer, ResponseHeader* res, Job* j) {
	char* p;
	char* ptr = NULL;
	char* ptr2 = NULL;
	const char* delim = "\r\n";
	const char* delim2 = " ";
	printf("%2d：正在解析返回报头\n", j->jobid);

	p = strtok_s(buffer, delim, &ptr); //提取第一行
	if (p == NULL) return -1;
	printf("%s\n", p);
	int ret = -1;
	char *lp = strtok_s(buffer, delim2, &ptr2);
	if (lp)
	{
		lp = strtok_s(NULL, delim2, &ptr2);
		if (lp) {
			ret = atoi(lp);
			printf("返回状态码：%d[%s]\n", ret, lp);
			res->status = ret;
		}
	}
	
	p = strtok_s(NULL, delim, &ptr);
	res->keep = 1;
	res->contentlen = -1;
	while (p)
	{
		switch (p[0])
		{
		case 'D':
			if (strlen(p) > 10) {
				char header[100] = { '\0' };
				memcpy(header, p, 4);
				_strlwr_s(header);
				if (!strcmp(header, "date"))
				{
					memcpy(res->date, &p[6], strlen(p)-6);
				}
			}
			break;
		case 'C':
			if (strlen(p) > 10) {
				char header[29] = { '\0' };
				memcpy(header, p, 28);
				_strlwr_s(header);
				
				if (!strcmp(header, "connection: close"))
				{
					printf("connection: close - 套接字将在响应之后会关闭！\n");
					res->keep = 0;
				}else if (!strcmp(header, "connection: keep-alive"))
				{
					printf("connection: keep-alive - 本次响应采用Keep-Alive模式！\n");
					res->keep = 1;
				}
				else if (!_strnicmp(header, "content-length: ", 16))
				{
					// strlen("content-length: ") = 16
					int len = atoi(header+16);
					printf("本次相应的长度为 %d [%s]\n",len, header + 16);
					res->contentlen = len;
				}
				
			}
			break;
		default:
			break;
		}
		p = strtok_s(NULL, delim, &ptr);
	}
	if (res->contentlen == -1) res->contentlen = MAXSIZE + 1;
	return ret;
}

//************************************
// Method: ParseHttpHead
// FullName: ParseHttpHead
// Access: public
// Returns: void
// Qualifier: 解析 TCP 报文中的 HTTP 头部
// Parameter: char * buffer
// Parameter: HttpHeader * httpHeader
//************************************
int ParseHttpHead(char* buffer, HttpHeader* httpHeader, Job* j)
{
	char* p;
	char* ptr = NULL;
	const char* delim = "\r\n";
	int invalid = 0;
	//printf("%2d：正在解析HTTP报头\n", j->jobid);

	p = strtok_s(buffer, delim, &ptr); //提取第一行
	//printf("%s", p);
	if (p[0] == 'G')
	{ // GET 方式
		memcpy(httpHeader->method, "GET", 3);
		memcpy(httpHeader->url, &p[4], strlen(p) - 13);
	}
	else if (p[0] == 'P')
	{ // POST 方式
		memcpy(httpHeader->method, "POST", 4);
		memcpy(httpHeader->url, &p[5], strlen(p) - 14);
	}
	else
	{
		//不支持的连接类型
		invalid = 1;
	}

	//printf("%s %s\n", httpHeader->method, httpHeader->url);
	int ttlen = 0;
	int ret = 0;
	p = strtok_s(NULL, delim, &ptr);
	while (p)
	{
		switch (p[0])
		{
		case 'P':

			if (strlen(p) > 10) {
				char header[29] = { '\0' };
				memcpy(header, p, 28);
				_strlwr_s(header);
				if (!strcmp(header, "proxy-connection: keep-alive"))
				{
					ret = 1;
					strcpy_s(httpHeader->connection, "keep-alive");
				}
			}
			break;

		case 'H': // Host
			ttlen = strlen(p) - 6;
			memcpy(httpHeader->host, &p[6], ttlen);
			memcpy(j->host, &p[6], ttlen>63?63:ttlen );
			//printf("Host: %s\n", httpHeader->host);
			break;
		case 'C': // Cookie
			if (strlen(p) > 8)
			{
				char header[8] = { '\0' };
				memcpy(header, p, 6);
				if (!strcmp(header, "Cookie"))
				{
					memcpy(httpHeader->cookie, &p[8], strlen(p) - 8);
					printf("Cookie: %s\n", httpHeader->cookie);
				}

			}
			break;
		default:
			break;
		}
		p = strtok_s(NULL, delim, &ptr);
	}
	if (invalid) return -1;

	printf("%2d：正在解析HTTP报头\n", j->jobid);
	printf("%s %s\n", httpHeader->method, httpHeader->url);
	printf("Host: %s\n", httpHeader->host);
	return ret;
}


unsigned int __stdcall ProxyThreadClient(LPVOID job) {
	Job* j = (Job*)job;
	ProxyParam* IPpair = &j->socket;
	int jobid = j->jobid;
	char* Buffer, * CacheBuffer;
	Buffer = new char[MAXSIZE + 1];
	int recvSize = 1;
	int ret;
	int closeid = 1;
	int keep;
	while (recvSize > 0)
	{
		ZeroMemory(Buffer, MAXSIZE);
		printf("%2d：正在接收用户数据\n", jobid);
		recvSize = recv(IPpair->clientSocket, Buffer, MAXSIZE, 0);
		if (recvSize <= 0)
		{
			printf("%2d：接收用户数据失败: %s\n", jobid, getWSAErrorText());
			if (jobClose[j->jobindex]) {
				break;
			}
			else {
				Sleep(100);
				continue;
			}
		}
		printf("%2d：接收到用户数据大小：%d\n", jobid, recvSize);
		HttpHeader* httpHeader = new HttpHeader();
		CacheBuffer = new char[recvSize + 1];
		ZeroMemory(CacheBuffer, recvSize + 1);
		memcpy(CacheBuffer, Buffer, recvSize);
		printf("##############################\n");
		printf("%s\n", CacheBuffer);
		printf("##############################\n");

		keep = ParseHttpHead(CacheBuffer, httpHeader, j);
		delete[]CacheBuffer;
		if (keep == -1) {
			printf("%2d：不支持的连接类型，延时关闭\n", jobid);
			j->status = 10;
			delete httpHeader;
			Sleep(UNSUPPORT_METHOD_TIME_SLEEP);
			break;
		}
		printf("%2d：正在发送用户数据, keep = %d\n", jobid, keep);
		ret = send(IPpair->serverSocket, Buffer, recvSize + 1, 0);
		if (ret == -1) {
			printf("%2d：发送用户数据失败: %s\n", jobid, getWSAErrorText());
			delete httpHeader;
			if (jobClose[j->jobindex]) {
				break;
			}
			else {
				continue;
			}
		}
		printf("%2d：发送用户数据成功\n", jobid);
		delete httpHeader;
		if (keep == 0) {
			break;
		}
	}
	jobClose[j->jobindex] = true;
	delete[]Buffer;
	CloseSocket(job, closeid);
	return 0;
}

unsigned int __stdcall ProxyThreadServer(LPVOID job)
{
	Job* j = (Job*)job;
	ProxyParam* IPpair = &j->socket;
	int jobid = j->jobid;
	char* Buffer;
	Buffer = new char[MAXSIZE + 1];
	int closeid = 0;
	int recvSize = 1;
	int ret;
	while (recvSize > 0) {
		ZeroMemory(Buffer, MAXSIZE + 1);
		//等待目标服务器返回数据
		recvSize = recv(IPpair->serverSocket, Buffer, MAXSIZE, 0);
		if (recvSize <= 0)
		{
			cacheClose(jobcacheidlist[j->jobindex]);
			printf("%2d：接收服务器数据失败：%s\n", jobid, getWSAErrorText());
			if (jobClose[j->jobindex]) {
				break;
			}
			else {
				continue;
			}
		}


		char *CacheBuffer = new char[recvSize + 1];
		ZeroMemory(CacheBuffer, recvSize + 1);
		memcpy(CacheBuffer, Buffer, recvSize);
		ResponseHeader rh;
		int statuscode = ParseResponse(CacheBuffer, &rh, j);
		delete[]CacheBuffer;

		if (recvData(IPpair->serverSocket, Buffer, &recvSize, rh.contentlen) != 0) {
			printf("分段接收失败！立即发送给客户！\n");
		}


		//Sleep(50);
		printf("%2d：接收到服务器数据：%d\n", jobid, recvSize);

		char tmpBuffer[800] = { 0 };
		for (int i = 0; i < 799; i++) {
			tmpBuffer[i] = Buffer[i];
		}
		printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
		printf("%s", tmpBuffer);
		printf("\n>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");

		int keep = 0;
		keep = rh.keep;
		//将目标服务器返回的数据直接转发给客户端
		ret = send(IPpair->clientSocket, Buffer, recvSize+1, 0);
		int wi;
		wi = cacheWrite(jobcacheidlist[j->jobindex], Buffer, recvSize, 0);
		printf("%2d：写缓存大小：%d\n", jobid, wi);

		
		if (ret <= 0) {
			printf("%2d：转发服务器数据失败: %s\n", jobid, getWSAErrorText());
			if (jobClose[j->jobindex]) {
				break;
			}
			else {
				continue;
			}
		}
		printf("%2d：转发服务器数据大小：%2d\n", jobid, ret);
		if (keep == 0) {
			
			break;
		}
		/*if (rt < 1000) {
			printf("#######################\n");
			printf("%s\n", Buffer);
			printf("#######################\n");
		}*/
	}
	jobClose[j->jobindex] = true;
	cacheClose(jobcacheidlist[j->jobindex]);
	delete[]Buffer;
	CloseSocket(job, closeid);
	return 0;
}

int ForbidCheck(char* ip, char* host) {
	printf("正在检查\n%s\n%s\n", ip, host);
	for (int i = 0; i < forbidIpNum; i++) {
		if (strcmp(forbidIp[i], ip) == 0) {
			printf("禁止IP访问[%d]：%s\n", i, ip);
			return 0;
		}
	}
	for (int i = 0; i < forbidWebNum; i++) {
		if (_strnicmp(forbidWeb[i].url, host, forbidWeb[i].len) == 0) {
			printf("禁止URL访问[%d]：%s\n", i, forbidWeb[i].url);
			return 0;
		}
	}
	for (int i = 0; i < tipFishWebNum; i++) {
		if (_strnicmp(tipFishWeb[i].url, host, tipFishWeb[i].len) == 0) {
			printf("禁止URL钓鱼访问[%d]：%s\n", i, tipFishWeb[i].url);
			return -1;
		}
	}
	//strnicmp();
	return 1;
}
int addIfModifiedHeader(char* buffer, int* size, char* date, Job* j) {
	char* buf = new char[MAXSIZE];
	memset(buf, 0, MAXSIZE);
	memcpy(buf, buffer, *size);
	char* p;
	char* ptr = NULL;
	const char* delim = "\r\n";

	char time[300] = "If-Modified-Since: ";
	strcat_s(time, date);
	strcat_s(time, "\r\n");
	printf("%2d：正在添加HTTP报头[%s]\n", j->jobid,time);

	//p = strtok_s(buf, delim, &ptr); //提取第一行
	p = strchr(buffer, '\n');
	if (p == NULL) return -1;
	p = strchr(p+1, '\n');
	if (p == NULL) return -1;
	int index = p - buffer;
	int len = strlen(time);
	memcpy(p+1, time, len);
	memcpy(p + 1 + len, buf + index + 1, *size - index);
	//memcpy(buffer + *size, time, len);
	*size = *size + len;
	printf("#####################\n%s\n###################\n", buffer);
	delete[]buf;
	return 0;
}


void hideHttpsPrint(char* buf,int &num,char* text) {
	if (CONTROLDATA.hidehttps) {
		memcpy(buf + num, text, strlen(text));
		return;
	}
	else printf("%s", text);
}

//************************************
// Method: ProxyThread
// FullName: ProxyThread
// Access: public
// Returns: unsigned int __stdcall
// Qualifier: 线程执行函数
// Parameter: LPVOID lpParameter
//************************************
unsigned int __stdcall ProxyThread(LPVOID job)
{
	const int closeid = -1;
	Job* j = (Job*)job;
	int index = j->jobindex;
	int jobid = j->jobid;
	ProxyParam* IPpair = &(j->socket);

	char* HIDEHTTPSBUF = new char[MAXSIZE*10];
	char* HIDEHTTPSBUFSHIFT = HIDEHTTPSBUF;
	
	char* Buffer, * CacheBuffer;
	Buffer = new char[MAXSIZE + 1];
	ZeroMemory(Buffer, MAXSIZE + 1);
	ZeroMemory(HIDEHTTPSBUF, MAXSIZE+1);


	int recvSize;
	int ret;
	j->status = 1;
	in_addr ca;
	ca.S_un.S_addr = j->srcIp;
	char clientIp[20] = { 0 };
	strcpy_s(clientIp, inet_ntoa(ca));

	if(!CONTROLDATA.hidehttps) HIDEHTTPSBUFSHIFT+=sprintf_s(HIDEHTTPSBUFSHIFT,MAXSIZE,"%2d：正在接收用户数据 %s,%d\n", jobid, clientIp, j->srcPort);
	recvSize = recv(IPpair->clientSocket, Buffer, MAXSIZE, 0);
	if (recvSize <= 0)
	{
		if (!CONTROLDATA.hidehttps) printf("%2d：接收用户数据失败：%s\n", jobid, getWSAErrorText());
		delete[]HIDEHTTPSBUF;
		delete[]Buffer;
		CloseSocket(job, closeid);
		return 0;
	}
	j->status = 2;
	if (!CONTROLDATA.hidehttps) HIDEHTTPSBUFSHIFT += sprintf_s(HIDEHTTPSBUFSHIFT, MAXSIZE, "%2d：接收到用户数据大小：%d\n", jobid, recvSize);
	HttpHeader* httpHeader = new HttpHeader();
	CacheBuffer = new char[recvSize + 1];
	ZeroMemory(CacheBuffer, recvSize + 1);
	memcpy(CacheBuffer, Buffer, recvSize);
	if (!CONTROLDATA.hidehttps) HIDEHTTPSBUFSHIFT += sprintf_s(HIDEHTTPSBUFSHIFT, MAXSIZE, "##############################\n");
	if (!CONTROLDATA.hidehttps) HIDEHTTPSBUFSHIFT += sprintf_s(HIDEHTTPSBUFSHIFT, MAXSIZE, "%s\n", CacheBuffer);
	if (!CONTROLDATA.hidehttps) HIDEHTTPSBUFSHIFT += sprintf_s(HIDEHTTPSBUFSHIFT, MAXSIZE, "##############################\n\0");
	int keep;
	keep = ParseHttpHead(CacheBuffer, httpHeader, j);
	delete[]CacheBuffer;
	if (keep == -1) {
		if (!CONTROLDATA.hidehttps) {
			printf("%s", HIDEHTTPSBUF);
			printf("%2d：不支持的连接类型，延时关闭\n", jobid);
		}
		delete[]HIDEHTTPSBUF;
		j->status = 10;
		delete httpHeader;
		delete[]Buffer;
		Sleep(UNSUPPORT_METHOD_TIME_SLEEP);
		CloseSocket(job, closeid);
		return 0;
	}
	printf("%s", HIDEHTTPSBUF);
	delete[]HIDEHTTPSBUF;
	//禁止部分
	int forbid;
	forbid = ForbidCheck(clientIp, httpHeader->host);
	if (forbid == 0) {
		printf("%2d：网站或用户被禁止访问\n", jobid);
		delete[]Buffer;
		delete httpHeader;
		CloseSocket(job, closeid);
		return 0;
	}
	if (forbid == -1) {
		//钓鱼
		char s[] = "HTTP/1.0 200 OK\n\n<html><header><title>代理服务器警告</title></header><body><div style=\'text-align:center;width:100%;height:100%;background-color:#ffe596;display:flex;flex-direction:column;justify-self:center;justify-content:center\'><text>王炳轩提示你这是钓鱼网站！连接已关闭！\nWbx notes you that this is a fish website, connect closed.</text></div></body><html>";
		ret = send(IPpair->clientSocket, s, sizeof(s)+1, 0);
		delete[]Buffer;
		delete httpHeader;
		CloseSocket(job, closeid);
		return 0;
	}
	printf("%2d：允许访问 = %d\n", jobid, forbid);
	printf("%2d：HTTP报头已解析,keepalive=%d\n", jobid, keep);

	//缓存部分
	int cacheid;
	cacheid = cacheCheck(httpHeader->url, httpHeader->cookie);
	if (cacheid != -1) {
		//存有缓存，现在添加IF-Modified-since查询
		if (addIfModifiedHeader(Buffer, &recvSize, cache[cacheid].date, j)!=0) {
			printf("%2d：添加报头失败\n", jobid);
		}
	}

	j->status = 3;
	printf("%2d：正在建立远程套接字\n", jobid);

	if (!ConnectToServer(&(IPpair->serverSocket), httpHeader->host, j))
	{
		printf("%2d：建立远程套接字失败: %s\n", jobid, getWSAErrorText());
		delete httpHeader;
		delete[]Buffer;
		CloseSocket(job, closeid);
		return 0;
	}
	j->status = 4;
	printf("%2d：代理连接主机 %s 成功\n", jobid, httpHeader->host);
	//将客户端发送的 HTTP 数据报文直接转发给目标服务器
	ret = send(IPpair->serverSocket, Buffer, recvSize + 1, 0);
	j->status = 5;
	printf("%2d：等待接收服务器数据中\n", jobid);
	ZeroMemory(Buffer, strlen(Buffer) + 1);
	//等待目标服务器返回数据
	recvSize = recv(IPpair->serverSocket, Buffer, MAXSIZE, 0);
	if (recvSize <= 0)
	{
		printf("%2d：接收服务器数据失败: %s\n", jobid, getWSAErrorText());
		delete[]Buffer;
		delete httpHeader;
		CloseSocket(job, closeid);
		return 0;
	}


	printf("%2d：接收到服务器数据：%d\n", jobid, recvSize);

	char tmpBuffer[800] = { 0 };
	for (int i = 0; i < 799; i++) {
		tmpBuffer[i] = Buffer[i];
	}
	printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
	printf("%s", tmpBuffer);
	printf("\n>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");

	CacheBuffer = new char[recvSize + 1];
	ZeroMemory(CacheBuffer, recvSize + 1);
	memcpy(CacheBuffer, Buffer, recvSize);
	ResponseHeader rh;
	int statuscode = ParseResponse(CacheBuffer,&rh,j);
	delete[]CacheBuffer;

	if (recvData(IPpair->serverSocket, Buffer, &recvSize, rh.contentlen) != 0) {
		printf("分段接收失败！立即发送给客户！\n");
	}


	if (statuscode == 304 && cacheid != -1) {
		//未更改，发送缓存数据
		//存有缓存，现在一次性发送过去

		char fails[] = "HTTP/1.0 200 OK\n\n<html><header><title>代理服务器警告</title></header><body><div style=\'text-align:center;width:100%;height:100%;background-color:#ffe596;display:flex;flex-direction:column;justify-self:center;justify-content:center\'><text>缓存读取失败，请稍后再试！</text></div></body><html>";
		FILE* ff;
		char path[200] = "../Cache/";
		strcat_s(path, cache[cacheid].path);
		fopen_s(&ff, path, "r");
		if (ff != NULL) {
			printf("%2d：HTTP1.1缓存列表读取成功：[%s]\n", jobid, path);
			int i = 1;
			char* nbuf = new char[MAXSIZE+1];
			//ffRead = fread(Buffer, sizeof(char), MAXSIZE, ff);
			char* get = fgets(path,100,ff);
			int success = 1;
			while (get != NULL) {
				FILE* fptr;
				path[strlen(path) - 1] = '\0';
				printf("%2d：正在读取缓存[%d]：[%s]\n", jobid,i, path);
				fopen_s(&fptr, path, "r");
				if (fptr == NULL) {
					success = 0;
					break;
				}
				int fptrRead = fread(nbuf, sizeof(char), MAXSIZE, fptr);
				printf("%2d：缓存已读取：%d B\n", jobid, fptrRead);
				ret = send(IPpair->clientSocket, nbuf, fptrRead, 0);
				if (ret <= 0) {
					printf("缓存发送失败！ %s\n", getWSAErrorText());
					success = 0;
					break;
				}
				printf("%2d：缓存正在分片发送[%d]:%d B\n", jobid, i, ret);
				fclose(fptr);
				
				get = fgets(path, 100, ff);
				i++;
			}
			//printf("%2d：缓存读取[%i]\n", jobid, i);

			if(!success){
				printf("%2d：缓存读取失败：[%s]\n", jobid, path);
				
				ret = send(IPpair->clientSocket, fails, sizeof(fails), 0);
			}/*
			char* ptr;
			char* pp;
//			while (fpt != NULL) {
			int cccc = 0,lennn = strlen(FILE_DELIM),index=0;
				ptr = 0;
				pp = strtok_s(nbuf, FILE_DELIM, &ptr);
				//if (pp!=NULL) pp = strtok_s(NULL, FILE_DELIM, &ptr);
				while (pp != NULL) {
					printf("%2d：缓存已读取：%d B\n", jobid, strlen(pp));
					printf_s("%s\n",pp);
					ret = send(IPpair->clientSocket, pp, strlen(pp), 0);
					if (ret <= 0) {
						printf("缓存发送失败！ %s\n", getWSAErrorText());
					}
					printf("%2d：缓存正在分片发送[%d]:%d B\n", jobid, i, ret);
					pp = strtok_s(NULL, FILE_DELIM, &ptr);
					i++;
				}
				
				//memset(Buffer, 0, MAXSIZE);
				//fpt = fread(Buffer, sizeof(char), MAXSIZE, ff);
				
			//}*/
			delete[]nbuf;
			fclose(ff);
			delete httpHeader;
			delete[]Buffer;
			CloseSocket(job, closeid);
			return 0;
		}
		else {
			printf("%2d：缓存读取失败：[%s]\n", jobid, path);
			
			ret = send(IPpair->clientSocket, fails, sizeof(fails), 0);
			delete httpHeader;
			delete[]Buffer;
			CloseSocket(job, closeid);
			return 0;
		}
	}
	
	
	j->status = 6;
	
	//将目标服务器返回的数据直接转发给客户端
	ret = send(IPpair->clientSocket, Buffer, recvSize + 1, 0);
	printf("%2d：转发服务器数据大小：%d\n", jobid, ret);

	cacheid = cacheCreate(httpHeader->url,rh,httpHeader->cookie);
	jobcacheidlist[j->jobindex] = cacheid;
	int wi;
	wi = cacheWrite(cacheid, Buffer, recvSize, 0);
	printf("%2d：写缓存大小：%d\n", jobid, wi);
	
	j->status = 7;
	delete[]Buffer;
	delete httpHeader;
	if (keep == 1) {
		HANDLE hThread;
		hThread = (HANDLE)_beginthreadex(NULL, 0,
			&ProxyThreadClient, job, 0, 0);
		if (hThread != NULL) CloseHandle(hThread);
		hThread = (HANDLE)_beginthreadex(NULL, 0,
			&ProxyThreadServer, job, 0, 0);
		if (hThread != NULL) CloseHandle(hThread);
	}
	else {
		cacheClose(jobcacheidlist[j->jobindex]);
	}
	return 0;
}

void CloseSocket(LPVOID job, int id) {
	Job* j = (Job*)job;
	ProxyParam* IPpair = &j->socket;
	printf("%2d：关闭%s套接字\n", j->jobid,id==0?"server":(id==1?"client" : "全部"));
	Sleep(200);
	if (IPpair->clientSocket && (id == 1 || id == -1)) {
		closesocket(IPpair->clientSocket);
		IPpair->clientSocket = NULL;
	}
	if (IPpair->serverSocket && (id == 0 || id == -1)) {
		closesocket(IPpair->serverSocket);
		IPpair->serverSocket = NULL;
	}
	if (IPpair->serverSocket == NULL && IPpair->clientSocket == NULL) {
		j->status = 8;
		SHAREDATA.usingjob[j->jobindex] = false;
	}
	//writeShare();
	_endthreadex(0);
}

//************************************
// Method: ConnectToServer
// FullName: ConnectToServer
// Access: public
// Returns: BOOL
// Qualifier: 根据主机创建目标服务器套接字，并连接
// Parameter: SOCKET * serverSocket
// Parameter: char * host
//************************************
BOOL ConnectToServer(SOCKET* serverSocket, char* host, Job* j)
{
	sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	//htons(HTTP_PORT);
	printf("%2d：正在获取服务器IP: %s\n", j->jobid, host);
	char hostname[1024] = { "\0" };
	char* p, * port;
	//char* index;
	char* ptr = NULL;
	const char* dlim = ":";
	strcpy_s(hostname, host);
	p = strtok_s(hostname, dlim, &ptr);
	port = strtok_s(NULL, dlim, &ptr);
	if (port == NULL) {
		j->dstPort = HTTP_PORT;
		serverAddr.sin_port = htons(HTTP_PORT);
	}
	else {
		j->dstPort = atoi(port);
		serverAddr.sin_port = htons((short)j->dstPort);
	}

	HOSTENT* hostent = gethostbyname(p);
	if (!hostent)
	{
		printf("获取服务器IP失败\n");
		return FALSE;
	}
	printf("%2d：获取到服务器IP列表[%d]\n", j->jobid, hostent->h_length);
	if (hostent->h_length <= 0)
	{
		printf("获取到服务器IP失败\n");
		return FALSE;
	}
	in_addr Inaddr = *((in_addr*)*hostent->h_addr_list);
	serverAddr.sin_addr.s_addr = inet_addr(inet_ntoa(Inaddr));
	printf("%2d：正在建立远程套接字：%s:%d\n", j->jobid, inet_ntoa(Inaddr), j->dstPort);
	j->dstIp = serverAddr.sin_addr.s_addr;
	//j->dstPort = serverAddr.sin_port;

	*serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (*serverSocket == INVALID_SOCKET)
	{
		printf("%2d：建立远程套接字失败\n", j->jobid);
		return FALSE;
	}
	printf("%2d：正在连接服务器\n", j->jobid);
	if (connect(*serverSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
	{
		printf("%2d：连接服务器失败", j->jobid);
		printf("：%s\n", getWSAErrorText());
		closesocket(*serverSocket);
		return FALSE;
	}
	return TRUE;
}



HANDLE hMutex;
HANDLE hFileHandle;
char* sharedMemory;
char* controlMemory;

bool GetNewConsole() {
	ZeroMemory((void*)&SHAREDATA, sizeof(SharedArea));
	ZeroMemory((void*)&CONTROLDATA, sizeof(ControlArea));

	if ((hMutex = OpenMutex(MUTEX_ALL_ACCESS, false, (LPCWSTR)L"MyMutex")) == nullptr)
	{
		hMutex = CreateMutex(0, false, (LPCWSTR)L"MyMutex");
	}
	if (hMutex == NULL) return false;
	SECURITY_ATTRIBUTES sa;
	sa.bInheritHandle = true;
	sa.lpSecurityDescriptor = NULL;
	sa.nLength = sizeof(sa);
	//创建共享内存句柄
	//如果hFile是INVALID_HANDLE_VALUE ，调用进程还必须在dwMaximumSizeHigh和 dwMaximumSizeLow参数中指定文件映射对象的大小。在这种情况下， CreateFileMapping创建一个指定大小的文件映射对象，该对象由系统分页文件支持，而不是由文件系统中的文件支持。
	//为指定文件创建或打开命名或未命名文件映射对象
	hFileHandle = CreateFileMapping(INVALID_HANDLE_VALUE, &sa, PAGE_READWRITE, 0, sizeof(SharedArea)+ sizeof(ControlArea), (LPCWSTR)L"MyFile");
	printf("hFileHandle=%p\n", hFileHandle);
	if (hFileHandle == NULL) return false;
	//在调用进程的地址空间映射一个文件视图
	sharedMemory = (char*)MapViewOfFile(hFileHandle, FILE_MAP_ALL_ACCESS, 0, 0, 0);
	if (sharedMemory == NULL) return false;
	controlMemory = sharedMemory + sizeof(SharedArea);
	if (controlMemory == NULL) return false;

	//使用互斥对象保证同一时刻只允许一个进程读或写共享内存
	WaitForSingleObject(hMutex, INFINITE);
	memcpy(sharedMemory, &SHAREDATA, sizeof(SharedArea));
	ReleaseMutex(hMutex);

	//直接将文件映射对象句柄传给其他进程
	WCHAR cmd[1024];
	char szStr[512] = "";
	sprintf_s(szStr, "%p", hFileHandle);
	memset(cmd, 0, sizeof(cmd));
	MultiByteToWideChar(CP_ACP, 0, szStr, strlen(szStr) + 1, cmd, sizeof(cmd) / sizeof(cmd[0]));

	STARTUPINFOW startupInfo = { 0 };
	startupInfo.cb = sizeof(startupInfo);
	PROCESS_INFORMATION processInfo = { 0 };
	//delete count;
	//count = nullptr;
	if (!CreateProcess(L"../Debug/NetConsole.exe", cmd, 0, 0, true, CREATE_NEW_CONSOLE, 0, 0, &startupInfo, &processInfo))
	{
		printf("CreateProcess failed\n");
	}
	else
	{
		printf("CreateProcess sucessed\n");
	}

	return true;
}

void writeShare() {
	WaitForSingleObject(hMutex, INFINITE);
	memcpy(sharedMemory, &SHAREDATA, sizeof(SharedArea));
	ReleaseMutex(hMutex);
}

void refleshControl() {
	WaitForSingleObject(hMutex, INFINITE);
	memcpy(&CONTROLDATA, controlMemory, sizeof(ControlArea));
	ReleaseMutex(hMutex);
}

void writeControl() {
	WaitForSingleObject(hMutex, INFINITE);
	memcpy(controlMemory, &CONTROLDATA, sizeof(ControlArea));
	ReleaseMutex(hMutex);
}

int proexit(int err) {
	if (err != -1) {
		CONTROLDATA.stop = true;
		writeControl();

	}//被控制台进程强制关闭
	else err = 0;
	if (sharedMemory != NULL) UnmapViewOfFile(sharedMemory);	//从调用进程的地址空间取消映射文件的映射视图
	if (hFileHandle != NULL) CloseHandle(hFileHandle);
	closesocket(ProxyServer);
	if (hMutex != NULL) ReleaseMutex(hMutex);
	fclose(fcache);
	WSACleanup();
	exit(err);
}

void initServer() {
	srand((unsigned int)time(NULL));
	printf("代理服务器正在启动\n");
	//创建控制台
	if (GetNewConsole() == false) {
		exit(1);
	}
	printf("初始化...\n");
	fopen_s(&fcache, "../CacheList.txt", "r");
	if (fcache == NULL) {
		printf("打开文件失败\n");
		exit(1);
	}
	char* hasNext;
	int i = 0;
	int maxid = 0;
	const char* delim = CACHE_DELIM;
	char line[500] = { 0 };
	char* ptr;
	char* tempurl, * temppath,*tempdate,*tempcookie,*tempid;
	do {
		ZeroMemory(line, sizeof(line));
		hasNext = fgets(line, 500, fcache);
		if (hasNext == NULL) break;
		tempurl = strtok_s(line, delim, &ptr);
		if (tempurl == NULL) continue;
		temppath = strtok_s(NULL, delim, &ptr);
		if (temppath == NULL) continue;

		tempdate = strtok_s(NULL, delim, &ptr);
		if (tempdate == NULL) continue;
		tempcookie = strtok_s(NULL, delim, &ptr);
		if (tempcookie == NULL) continue;
		tempid = strtok_s(NULL, delim, &ptr);
		if (tempid == NULL) continue;
		int id = atoi(tempid);
		strcpy_s(cache[id].url, tempurl);
		strcpy_s(cache[id].path, temppath);
		strcpy_s(cache[id].cookie, tempcookie);
		strcpy_s(cache[id].date, tempdate);
		printf("%d|||%s||||%s||||%s|||%s|||\n", id ,cache[id].url, cache[id].path,cache[id].date,cache[id].cookie);
		if (id > maxid) maxid = id;
		i++;
	} while (hasNext != NULL);
	fclose(fcache);
	fopen_s(&fcache, "../CacheList.txt", "a+");
	printf("正在获取CACHE共%d项\n", maxid+1);
	cachenum = maxid+1;
	printf("正在获取IP禁止名单\n");
	FILE* f;
	fopen_s(&f, "../ipForbid.txt", "r");
	if (f == NULL) {
		printf("打开文件失败\n");
		exit(1);
	}
	i = 0;
	do {
		hasNext = fgets(forbidIp[i], 20, f);
		if (hasNext == NULL) break;
		printf("%s\n", forbidIp[i]);
		i++;
	} while (hasNext != NULL);
	forbidIpNum = i;
	fclose(f);
	printf("正在获取URL禁止名单\n");
	fopen_s(&f, "../urlForbid.txt", "r");
	if (f == NULL) {
		printf("打开文件失败\n");
		exit(1);
	}
	i = 0;
	do {
		hasNext = fgets(forbidWeb[i].url, 20, f);
		if (hasNext == NULL) break;
		forbidWeb[i].len = strlen(forbidWeb[i].url);
		printf("%s\n", forbidWeb[i].url);
		i++;
	} while (hasNext != NULL);
	forbidWebNum = i;
	fclose(f);
	printf("正在获取钓鱼网址名单\n");
	fopen_s(&f, "../urlFish.txt", "r");
	if (f == NULL) {
		printf("打开文件失败\n");
		exit(1);
	}
	i = 0;
	do {
		hasNext = fgets(tipFishWeb[i].url, 20, f);
		if (hasNext == NULL) break;
		tipFishWeb[i].len = strlen(tipFishWeb[i].url);
		printf("%s\n", tipFishWeb[i].url);
		i++;
	} while (hasNext != NULL);
	tipFishWebNum = i;
	fclose(f);

	SetConsoleTitleA("代理历史记录"); //设置一个新标题
	if (!InitSocket())
	{
		printf("socket 初始化失败\n");
		exit(1);
	}

}
/*int count = 0;
	while (count < argc) {
		printf("%s\n", argv[count]);
		count++;
	}
	printf("##############\n");if (argc > 1) {
		printf("这是子进程");
		getchar();
		return 0;
	}*/
	//UI::printStart();

unsigned int __stdcall  statusThread(LPVOID a) {
	while (true) {
		Sleep(1000);
		writeShare();
		refleshControl();
		if (CONTROLDATA.stop == true) {
			printf("服务器正在被强制关闭！\n");
			proexit(-1);
		}
		if (_kbhit())
		{
			int doid = (int)_getch();
			switch (doid)
			{
			case B_QUIT:
				proexit(0);
				break;
			default:
				break;
			}

		}
	}
}

int main(int argc, char** argv)
{
	initServer();
	printf("代理服务器正在运行，监听端口 %d\n", ProxyPort);
	SOCKET acceptSocket = INVALID_SOCKET;
	Job* job;
	HANDLE hThread;
	sockaddr_in clientAddr;

	hThread = (HANDLE)_beginthreadex(NULL, 0, &statusThread, NULL, 0, 0);
	if (hThread != NULL) CloseHandle(hThread);

	int jobid = 0;
	int lastJobIndex = 0;
	int tmpjobindex = 0;
	int validJobIndex;
	int length = sizeof(sockaddr_in);
	while (true)
	{
		printf("主线程：正等待分配资源池索引\n");
		validJobIndex = -1;
		for (int i = 0; i < MAX_TCP_CONNECT; i++) {
			tmpjobindex = (lastJobIndex + i) % 25;
			if (SHAREDATA.usingjob[tmpjobindex] == false) {
				validJobIndex = tmpjobindex;
				break;
			}
		}
		if (validJobIndex == -1) {
			Sleep(300);
			continue;
		}
		printf("主线程：正等待客户连接\n");

		acceptSocket = accept(ProxyServer, (struct sockaddr*)&clientAddr, &length);
		jobid++;

		printf("主线程：接受客户连接：%s:%d（资源索引：%d，序列号：%d）\n",
			inet_ntoa(clientAddr.sin_addr), clientAddr.sin_port, validJobIndex, jobid);
		jobClose[validJobIndex] = false;
		job = &SHAREDATA.jobs[validJobIndex];
		ZeroMemory(job, sizeof(Job));
		SHAREDATA.usingjob[validJobIndex] = true;
		job->jobid = jobid;
		job->jobindex = validJobIndex;
		job->socket.clientSocket = acceptSocket;
		job->srcPort = ntohs(clientAddr.sin_port);
		job->srcIp = inet_addr(inet_ntoa(clientAddr.sin_addr));
		strcpy_s(job->srcIpText, inet_ntoa(clientAddr.sin_addr));
		hThread = (HANDLE)_beginthreadex(NULL, 0, &ProxyThread, (LPVOID)job, 0, 0);
		if (hThread != NULL) CloseHandle(hThread);
		Sleep(200);

		
		//writeShare();
	}
	proexit(0);
	return 0;
}