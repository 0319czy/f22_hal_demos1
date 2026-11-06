/* Narrow Win32 bridge for the vendor ProgISP GUI; no USB commands here. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
static const char *mode,*label;static int value;static HWND mainwin;
BOOL CALLBACK child(HWND h,LPARAM unused){char cls[80]={0},txt[8192]={0};GetClassNameA(h,cls,sizeof cls);SendMessageA(h,WM_GETTEXT,sizeof txt,(LPARAM)txt);
 if(!strcmp(mode,"log")&&!strcmp(cls,"TRichEdit"))puts(txt);
 if(!strcmp(mode,"clear")&&!strcmp(cls,"TRichEdit"))SendMessageA(h,WM_SETTEXT,0,(LPARAM)"");
 if(!strcmp(mode,"check")&&!strcmp(cls,"TCheckBox")&&!strcmp(txt,label)){if(SendMessageA(h,BM_GETCHECK,0,0)!=value)SendMessageA(h,BM_CLICK,0,0);}
 return TRUE;}
BOOL CALLBACK top(HWND h,LPARAM unused){char txt[256]={0};GetWindowTextA(h,txt,sizeof txt);if(!strcmp(txt,"PROGISP (Ver 1.72)"))mainwin=h;return TRUE;}
int main(int argc,char **argv){mode=argc>1?argv[1]:"log";label=argc>2?argv[2]:"";value=argc>3?atoi(argv[3]):0;EnumWindows(top,0);if(!mainwin)return 2;if(!strcmp(mode,"init")){SetWindowPos(mainwin,0,10,10,836,570,SWP_NOZORDER);SetForegroundWindow(mainwin);}EnumChildWindows(mainwin,child,0);return 0;}
