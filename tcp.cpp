
#include "stdafx.h"
#include <stdlib.h>
#include "Resource.h"
#include <winsock2.h>
#include <richedit.h>
#pragma comment(lib, "ws2_32.lib") 
#include "TCP.h"
#define BUFSIZE  (1024*64)
#define WM_SOCKET (WM_USER+1)
// 全局变量:
SOCKET tcps,tcps_listen;
BOOL beSendBlock=FALSE;
HINSTANCE hInst;								// 当前实例
HWND  hWnd,hOWnd,hIWnd,hIPEdit,hPortEdit,hConnBtn,hListenBtn;
TCHAR buf[BUFSIZE];
//cur:缓冲区中空闲区当前的位置
//sendpos:缓冲区中需要发送的数据的初始位置
int cur=0,sendpos=0;
//指示当前程序状态
BOOL conned=FALSE,listening=FALSE;
void Input();
void ClearBuf();

 int NetInit()
{
	WORD wVersionRequested;
	WSADATA wsaData;
	int err;

	wVersionRequested=MAKEWORD(1,1);

	err=WSAStartup(wVersionRequested,&wsaData);
	return 1;
}
 void NetEnd()
{
	closesocket(tcps);
	WSACleanup();
}
//socket 监听
 int NetListen(int port)
{
	ClearBuf();

	tcps_listen=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
	if (SOCKET_ERROR==WSAAsyncSelect(tcps_listen,hWnd,WM_SOCKET,
		FD_ACCEPT|FD_READ|FD_WRITE|FD_CLOSE))
	{
		MessageBox(NULL,_T("SELECT ERROR!"),"ERROR",0);
		return 0;
	}
	SOCKADDR_IN tcpaddr;
	tcpaddr.sin_family=AF_INET;
	tcpaddr.sin_port=htons(port);
	tcpaddr.sin_addr.S_un.S_addr=htonl(INADDR_ANY);

	//绑定
	int err=bind(tcps_listen,(SOCKADDR *)&tcpaddr,sizeof(tcpaddr));
	err=listen(tcps_listen,1);
	SendMessage(hListenBtn,WM_SETTEXT,0,(long)"Cancel");
	listening=TRUE;
	return 1;
}

//Socket 连接
 int NetConn(LPCSTR ipstr,int port)
{
	ClearBuf();
	tcps=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
	if (SOCKET_ERROR==WSAAsyncSelect(tcps,hWnd,WM_SOCKET,
		FD_CONNECT|FD_ACCEPT|FD_READ|FD_WRITE|FD_CLOSE))
	{
		MessageBox(NULL,_T("SELECT ERROR!"),"ERROR",0);
		return 0;
	}
	unsigned long ip=inet_addr(ipstr);
	if (INADDR_NONE==ip)
	{
		HOSTENT *he=gethostbyname(ipstr);
		ip=*((unsigned long *)(he->h_addr_list[0]));
	}
	struct sockaddr_in addr;
	addr.sin_family=AF_INET;
	addr.sin_addr.S_un.S_addr=ip;
	addr.sin_port=htons(port);
	//连接
	if (SOCKET_ERROR==connect(tcps,(sockaddr*)&addr,sizeof(addr)))
	{
		if (WSAGetLastError()!=WSAEWOULDBLOCK)
		{
			MessageBox(NULL,"CONNECT ERROR!","ERROR",0);
			return 0;
		}
	}
	return 1;
}
//socket 断开
 void NetDisconn()
{
	closesocket(tcps);
}

//发送缓冲区中
 int NetSend()
{
	if (beSendBlock)
	{
		return 0;
	}
	while (sendpos<cur)
	{
		int rs=send(tcps,buf+sendpos,cur-sendpos,0);
		if (SOCKET_ERROR==rs)
		{
			if (WSAEWOULDBLOCK==WSAGetLastError())
			//如果Socket缓冲区满了，要等到FD_WRITE才能发送
			{
				beSendBlock=TRUE;
				//输入控件设置为只读
				SendMessage(hIWnd,EM_SETREADONLY,TRUE,0);
				return 0;
			}
		}
		else
		{
			sendpos+=rs;
		}
	}
	return 1;
}

//socket 处理函数
inline void ProcessSocketMsg(WPARAM wParam,LPARAM lParam)
{
	if(WSAGETSELECTERROR(lParam))
	{
		switch(WSAGETSELECTERROR(lParam))
		{
		case WSAETIMEDOUT:MessageBox(hWnd,"超时！","ERROR",0);
			break;
		default:MessageBox(hWnd,"出错！","EEROR",0);
			break;
		}
		NetDisconn();
		SendMessage(hConnBtn,WM_SETTEXT,0,(long)"Connect");
		EnableWindow(hIPEdit,TRUE);
		EnableWindow(hPortEdit,TRUE);
		EnableWindow(hConnBtn,TRUE);
		EnableWindow(hListenBtn,TRUE);
		conned=FALSE;
		listening=FALSE;
	}
	else 
	{
		switch(WSAGETSELECTEVENT(lParam))
		{
		case FD_ACCEPT:
			tcps=accept(tcps_listen,NULL,NULL);
			closesocket(tcps_listen);
			SendMessage(hConnBtn,WM_SETTEXT,0,(long)"DisConnect");
			SendMessage(hListenBtn,WM_SETTEXT,0,(long)"Listen");
			SendMessage(hIWnd,EM_SETREADONLY,FALSE,0);
			EnableWindow(hListenBtn,FALSE);
			EnableWindow(hConnBtn,TRUE);
			listening=FALSE;
			conned=TRUE;
			break;

		case FD_CONNECT:
			SendMessage(hConnBtn,WM_SETTEXT,0,(long)"DisConnect");
			EnableWindow(hConnBtn,TRUE);
			SendMessage(hIWnd,EM_SETREADONLY,FALSE,0);
			conned=TRUE;
			break;
		case FD_CLOSE:
			MessageBox(hWnd,"对方关闭连接","OK",0);
			NetDisconn();
			SendMessage(hConnBtn,WM_SETTEXT,0,(long)"Connect");
			EnableWindow(hListenBtn,TRUE);
			EnableWindow(hIPEdit,TRUE);
			EnableWindow(hPortEdit,TRUE);
			SendMessage(hIWnd,EM_SETREADONLY,TRUE,0);
			conned=FALSE;
			break;
			//对方发来的数据
		case FD_READ:
			if (cur>=BUFSIZE-1)
			{
				ClearBuf();
			}
			cur+=recv(tcps,buf+cur,BUFSIZE-cur-1,0);
			SendMessage(hOWnd,WM_SETTEXT,0,(long)buf);
			break;
		case FD_WRITE:
			if (beSendBlock)
			{
				beSendBlock=FALSE;
				if (NetSend())
				{
					SendMessage(hOWnd,WM_SETTEXT,0,(long)buf);
					SendMessage(hIWnd,WM_SETTEXT,0,NULL);
					SendMessage(hIWnd,EM_SETREADONLY,FALSE,0);

				}
			}
			break;
		}
	}
}


// 此代码模块中包含的函数的前向声明:
ATOM				MyRegisterClass(HINSTANCE hInstance);

LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);


int APIENTRY _tWinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

 	// TODO: 在此放置代码。
	MSG msg;
	HACCEL hAccelTable;

	
	MyRegisterClass(hInstance);

	hWnd =CreateWindow("CHAT","Chat",WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME
		,200,200,505,410,NULL,NULL,hInstance,NULL);
		
		
	
	if (!hWnd)
	{
		return FALSE;
	}
	//显示窗口
	ShowWindow(hWnd,nCmdShow);
	UpdateWindow(hWnd);
	//初始化
	if(!NetInit()) return 0;
	hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_ACCEL));

	// 主消息循环:
	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (!TranslateAccelerator(hWnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	//关闭socket
	NetEnd();
	return (int) msg.wParam;
}



//
//  函数: MyRegisterClass()
//
//  目的: 注册窗口类。
//
//  注释:
//
//    仅当希望
//    此代码与添加到 Windows 95 中的“RegisterClassEx”
//    函数之前的 Win32 系统兼容时，才需要此函数及其用法。调用此函数十分重要，
//    这样应用程序就可以获得关联的
//    “格式正确的”小图标。
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= NULL;
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_BTNFACE+1);
	wcex.lpszMenuName	= NULL;
	wcex.lpszClassName	= "CHAT";
	wcex.hIconSm		= NULL;

	return RegisterClassEx(&wcex);
}


//
//  函数: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  目的: 处理主窗口的消息。
//
//  WM_COMMAND	- 处理应用程序菜单
//  WM_PAINT	- 绘制主窗口
//  WM_DESTROY	- 发送退出消息并返回
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
	PAINTSTRUCT ps;
	HDC hdc;
	HFONT hfont;
	switch (message)
	{
	case WM_SOCKET:
		//SOCKET消息
		ProcessSocketMsg(wParam,lParam);
		break;
	case WM_CREATE:
		//创建窗口消息初始化控件
		LoadLibrary("RICHED20.DLL");
		//输出聊天内容
		hOWnd=CreateWindowEx(WS_EX_CLIENTEDGE,RICHEDIT_CLASS,NULL,WS_CHILD|WS_VISIBLE|WS_VSCROLL|
			ES_LEFT|ES_MULTILINE|ES_AUTOHSCROLL|ES_READONLY,10,10,350,250,hWnd,NULL,hInst,NULL
			);
		//输入聊天内容控件
		hIWnd=CreateWindowEx(WS_EX_CLIENTEDGE,RICHEDIT_CLASS,NULL,WS_CHILD|WS_VISIBLE|WS_VSCROLL|
			ES_LEFT|ES_MULTILINE|ES_AUTOHSCROLL|ES_READONLY,10,270,350,100,hWnd,NULL,hInst,NULL
			);
		//输入IP的控件
		hIPEdit=CreateWindowEx(WS_EX_CLIENTEDGE,"EDIT","127.0.0.1",WS_CHILD|WS_VISIBLE|
			ES_LEFT|ES_AUTOHSCROLL,370,10,120,15,hWnd,(HMENU)ID_IPEDIT,hInst,NULL
			);
		//输入端口号
		hPortEdit=CreateWindowEx(WS_EX_CLIENTEDGE,"EDIT","8888",WS_CHILD|WS_VISIBLE|
			ES_LEFT|ES_NUMBER,370,45,50,25,hWnd,(HMENU)ID_PORTEDIT,hInst,NULL
			);
		//连接按钮
		hConnBtn=CreateWindowEx(0,"BUTTON","Connect",WS_CHILD|WS_VISIBLE|
			BS_PUSHBUTTON|BS_DEFPUSHBUTTON,370,80,100,25,hWnd,(HMENU)ID_CONNBTN,hInst,NULL
			);
		//侦听按钮
		hListenBtn=CreateWindowEx(0,"BUTTON","Listen",WS_CHILD|WS_VISIBLE|
			BS_PUSHBUTTON|BS_DEFPUSHBUTTON,370,115,100,25,hWnd,(HMENU)ID_LISTENBTN,hInst,NULL
			);
		//设置字体
		LOGFONT lf;
		lf.lfCharSet=DEFAULT_CHARSET;
		lf.lfClipPrecision=CLIP_DEFAULT_PRECIS;
		lf.lfEscapement=0;
		strcpy(lf.lfFaceName,"Verdana");
		lf.lfHeight=15;
		lf.lfItalic=FALSE;
		lf.lfOrientation=0;
		lf.lfOutPrecision=OUT_DEFAULT_PRECIS;
		lf.lfPitchAndFamily=FF_MODERN|DEFAULT_PITCH;
		lf.lfQuality=DEFAULT_QUALITY;
		lf.lfStrikeOut=FALSE;
		lf.lfUnderline=FALSE;
		lf.lfWeight=FW_BOLD;
		lf.lfWidth=7;
		hfont=CreateFontIndirect(&lf);
		SendMessage(hOWnd,WM_SETFONT,(WPARAM)hfont,MAKELPARAM(FALSE,0));
		SendMessage(hIWnd,WM_SETFONT,(WPARAM)hfont,MAKELPARAM(FALSE,0));
		SendMessage(hIPEdit,WM_SETFONT,(WPARAM)hfont,MAKELPARAM(FALSE,0));
		SendMessage(hPortEdit,WM_SETFONT,(WPARAM)hfont,MAKELPARAM(FALSE,0));
		SendMessage(hConnBtn,WM_SETFONT,(WPARAM)hfont,MAKELPARAM(FALSE,0));
		break;
	case WM_COMMAND:
		wmId    = LOWORD(wParam);
		wmEvent = HIWORD(wParam);
		// 分析菜单选择:
		switch (wmId)
		{
		case IDM_INPUT:Input();
			break;
		//按下连接按钮
		case ID_CONNBTN:
			if (conned)
			//已经连接
			{
				NetDisconn();
				SendMessage(hConnBtn,WM_SETTEXT,0,(long)"Connect");
				EnableWindow(hIPEdit,TRUE);
				EnableWindow(hPortEdit,TRUE);
				EnableWindow(hListenBtn,TRUE);
				SendMessage(hIWnd,EM_SETREADONLY,TRUE,0);
				conned=FALSE;
			}
			else
			//用户请求连接
			{
				char ipstr[1024],portstr[16];
				SendMessage(hIPEdit,WM_GETTEXT,1024,(long)ipstr);
				SendMessage(hPortEdit,WM_GETTEXT,16,(long)portstr);
				//连接
				if (NetConn(ipstr,atoi(portstr)))
				{
					EnableWindow(hConnBtn,FALSE);
					EnableWindow(hIPEdit,FALSE);
					EnableWindow(hPortEdit,FALSE);
					EnableWindow(hListenBtn,FALSE);
				}
			}
			break;
		case ID_LISTENBTN:
			if (listening)
			//如果已经侦探，取消侦听
			{
				closesocket(tcps_listen);
				EnableWindow(hConnBtn,TRUE);
				EnableWindow(hIPEdit,TRUE);
				EnableWindow(hPortEdit,TRUE);
				SendMessage(hListenBtn,WM_SETTEXT,0,(long)"Listen");
				listening=FALSE;
			}
			else
			{
				char portstr[16];
				SendMessage(hPortEdit,WM_GETTEXT,16,(long)portstr);
				//侦听
				if (NetListen(atoi(portstr)))
				{
					EnableWindow(hConnBtn,FALSE);
					EnableWindow(hIPEdit,FALSE);
					EnableWindow(hPortEdit,FALSE);
				}
			}
			break;
		}
		break;
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		// TODO: 在此添加任意绘图代码...
		RECT rt;
		GetClientRect(hWnd,&rt);
		EndPaint(hWnd, &ps);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

void ClearBuf()
{
	SendMessage(hOWnd,WM_SETTEXT,0,NULL);
	memset(buf,0,BUFSIZE);
	cur=0;
}
//发送用户输入内容，并将其显示在输出文本显示控件中
inline void Input()
{
	sendpos=cur;
	if (cur+SendMessage(hIWnd,WM_GETTEXTLENGTH,0,0)+1>=BUFSIZE)
	{
		ClearBuf();
	}
	cur+=SendMessage(hIWnd,WM_GETTEXT,BUFSIZE-cur-1,long(buf+cur));

	buf[cur++]='\n';
	if (NetSend())
	{
		SendMessage(hOWnd,WM_SETTEXT,0,(long)buf);
		SendMessage(hIWnd,WM_SETTEXT,0,NULL);
	}
}
