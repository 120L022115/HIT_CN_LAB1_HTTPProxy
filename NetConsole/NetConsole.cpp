#include <Windows.h>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <conio.h>
#include <Winsock.h>
#pragma comment(lib, "ws2_32.lib")
using namespace std;
#define REFLESH_TIME 10
#define B_QUIT 'Q'
#define B_CLEAR_CACHE 'C'
#define B_HIDE 'H'
#define MAX_TCP_CONNECT 25
int proexit(int err);
FILE* flog = NULL;

HANDLE hMutex;
char* sharedMemory;
char* controlMemory;
struct ProxyParam
{
	SOCKET clientSocket;
	SOCKET serverSocket;
};
struct ControlArea {
	bool stop = false;
	bool hidehttps = false;
}CONTROLDATA;

struct Job {
	char host[64];
	char srcIpText[16];
	ProxyParam socket;
	unsigned long srcIp; //弃用
	unsigned long dstIp;
	unsigned long srcPort;
	unsigned long dstPort;
	int jobid;
	int status;
	int jobindex;
};

struct  SharedArea
{
	Job jobs[MAX_TCP_CONNECT];
	bool usingjob[MAX_TCP_CONNECT] = { false };
}SHAREDATA;

void writeShare() {
	memcpy(sharedMemory, &SHAREDATA, sizeof(SharedArea));
}

void refleshShare() {
	memcpy(&SHAREDATA, sharedMemory, sizeof(SharedArea));
}

void refleshControl() {
	WaitForSingleObject(hMutex, INFINITE);
	memcpy(&CONTROLDATA, controlMemory, sizeof(ControlArea));
	ReleaseMutex(hMutex);
	if (CONTROLDATA.stop) proexit(0);
}

void writeControl() {
	WaitForSingleObject(hMutex, INFINITE);
	memcpy(controlMemory, &CONTROLDATA, sizeof(ControlArea));
	ReleaseMutex(hMutex);
}
//通用清屏
void clearscreen()
{
	system("cls");
}

//通用：打印错误项（白底高亮）
void printerr(int option, char* text)
{
	char tmp[300] = { "\033[7m" };
	switch (option)
	{
	case 0:

		strcat_s(tmp, text);
		strcat_s(tmp, "\033[0m\n");
		printf("%s", tmp);
		break;
	}

}
const char* getStatusText(int status) {
	switch (status) {
	case 0: return "等待分配";
	case 1: return "接收用户数据";
	case 2: return "生成HTTP报头";
	case 3: return "建立远程套接字";
	case 4: return "转发用户数据";
	case 5: return "接收服务器数据";
	case 6: return "转发服务器数据";
	case 7: return "运行HTTP1.1";
	case 8: return "处理完成";
	case 10: return "不支持的请求"; //延时关闭
	default: return "未知";
	}
}

int jobids[MAX_TCP_CONNECT] = { 0 };

void printStatus() {
	Job* j;
	in_addr adds;
	in_addr addc;
	int count = 0;
	printf("欢迎使用王炳轩的代理服务器！\n");
	printf("A NO JID dPORT           destIP sPORT            srcIP 状态描述         Host\n");
	for (int i = 0; i < 25; i++) {
		j = &SHAREDATA.jobs[i];
		if (j->jobid == 0) {
			continue;
		}
		//printf("%d\n", j->srcIp);
		memset(&adds, 0, sizeof(adds));
		memset(&addc, 0, sizeof(addc));
		adds.S_un.S_addr = j->dstIp;
		//addc.S_un.S_addr = j->srcIp;
		if(j->jobid!=jobids[i]) {
			jobids[i] = j->jobid;
			fprintf_s(flog,"%2d %3d %5ld %-16s %5ld %-16s %s\n", i, j->jobid, j->dstPort, inet_ntoa(adds), j->srcPort, j->srcIpText,j->host);
		}
		printf("%c %2d %3d %5ld %16s %5ld %16s %-16s %s\n", SHAREDATA.usingjob[i] ?'#' :' ', i, j->jobid, j->dstPort, inet_ntoa(adds), j->srcPort, j->srcIpText, getStatusText(j->status), j->host);
		if (SHAREDATA.usingjob[i]) count++;
	}
	printf("###################\n");
	printf("资源使用率：%d/%d\n",count, MAX_TCP_CONNECT);
	if (CONTROLDATA.hidehttps) {
		printf("HTTPS静默：已启用\n");
	}
}

int proexit(int err) {
	if (err == -1) {
		CONTROLDATA.stop = true;
		writeControl();
	}
	if (flog != NULL) fclose(flog);
	if (hMutex != NULL) ReleaseMutex(hMutex);
	exit(err);
}

void clearCache() {
	fflush(stdin);
	printf("\n########################\n确认停止服务器并清空缓存？输入Y/y确认，输入其他取消：\n");
	char c = getchar();
	if (c == 'Y'||c=='y') {
		CONTROLDATA.stop = true;
		writeControl();
		printf("正在关闭服务器，如在调试模式请手动关闭进程\n");
		Sleep(3000);
		/*int t = system("cmd \"rmdir /S ../Cache\"");
		if(t != 0) {
			printf("删除缓存文件夹失败\n");
		}else{
			system((char*)getchar());
			system("mkdir ../Cache");
		}
		*/
		
		FILE* f;
		if (fopen_s(&f,"../CacheList.txt","w")!=0) {
			printf("清空缓存文件列表失败\n");
			Sleep(5000);
			proexit(0);
		}
		fclose(f);
		printf("清空缓存文件成功\n");
		proexit(0);
	}
}
int consoleMain() {
	int doid = 0;
	int refleshCount = 0;
	while (1)
	{
		if (_kbhit())
		{
			doid = (int)_getch();
			switch (doid)
			{
			case B_QUIT:
				proexit(-1);
				break;
			case B_CLEAR_CACHE:
				clearCache();
				break;
			case B_HIDE:
				CONTROLDATA.hidehttps = !CONTROLDATA.hidehttps;
				writeControl();
					if (CONTROLDATA.hidehttps) {
						printf("已启用HTTPS静默!\n");
					}
					else {
						printf("已关闭HTTPS静默!\n");
					}
					Sleep(1000);
				break;
			default:
				break;
			}

		}
		Sleep(100);
		refleshCount++;
		if (refleshCount >= REFLESH_TIME) {
			refleshControl();
			refleshShare();
			clearscreen();
			printStatus();
			refleshCount = 0;
		}
	}
}

void initConsole() {
	if ((hMutex = OpenMutex(MUTEX_ALL_ACCESS, false, L"MyMutex")) == nullptr)
	{
		hMutex = CreateMutex(0, false, L"MyMutex");
	}
	if (hMutex == NULL) {
		printf("读取共享hMutex失败");
		getchar();
		proexit(1);
	}
	HANDLE hFileHandle = OpenFileMapping(FILE_MAP_ALL_ACCESS, true, L"MyFile");
	printf("fileHandle=%p\n", hFileHandle);
	if (hFileHandle == NULL) {
		printf("读取共享文件句柄失败");
		getchar();
		proexit(1);
	}
	sharedMemory = (char*)MapViewOfFile(hFileHandle, FILE_MAP_ALL_ACCESS, 0, 0, 0);
	if (sharedMemory == NULL) {
		printf("读取共享文件失败");
		getchar();
		proexit(1);
	}
	
	//在调用进程的地址空间映射一个文件视图
	controlMemory = sharedMemory + sizeof(SharedArea);
	
	int err = fopen_s(&flog, "../log.txt", "a+");
	if (err != 0 || flog == NULL) {
		printf("打开文件失败");
		proexit(1);
	}

	SetConsoleTitleA("代理控制台"); //设置一个新标题

}

/*int count = 0;
	while (count < argc) {
		printf("%s\n", argv[count]);
		count++;
	}
	printf("##############\n");
	*/

int main(int argc, char** argv)
{
	initConsole();
	consoleMain();
	proexit(0);
}