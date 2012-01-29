
/*
Copyright (c) 2009-2012 Maximus5
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. The name of the authors may not be used to endorse or promote products
   derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ''AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#define AssertCantActivate(x) //MBoxAssert(x)

#define SHOWDEBUGSTR
//#define ALLOWUSEFARSYNCHRO

#include "Header.h"
#include <Tlhelp32.h>
#include "../common/ConEmuCheck.h"
#include "../common/RgnDetect.h"
#include "../common/Execute.h"
#include "RealConsole.h"
#include "RealBuffer.h"
#include "VirtualConsole.h"
#include "TabBar.h"
#include "ConEmu.h"
#include "ConEmuApp.h"
#include "ConEmuChild.h"
#include "ConEmuPipe.h"
#include "Macro.h"

#define DEBUGSTRDRAW(s) DEBUGSTR(s)
#define DEBUGSTRINPUT(s) //DEBUGSTR(s)
#define DEBUGSTRINPUTPIPE(s) //DEBUGSTR(s)
#define DEBUGSTRSIZE(s) //DEBUGSTR(s)
#define DEBUGSTRPROC(s) DEBUGSTR(s)
#define DEBUGSTRCMD(s) DEBUGSTR(s)
#define DEBUGSTRPKT(s) //DEBUGSTR(s)
#define DEBUGSTRCON(s) //DEBUGSTR(s)
#define DEBUGSTRLANG(s) //DEBUGSTR(s)// ; Sleep(2000)
#define DEBUGSTRLOG(s) //OutputDebugStringA(s)
#define DEBUGSTRALIVE(s) //DEBUGSTR(s)
#define DEBUGSTRTABS(s) DEBUGSTR(s)
#define DEBUGSTRMACRO(s) //DEBUGSTR(s)

// ������ �� �������������� ������ ������ ��������� - ������ ����� ������� ����������� ����������.
// ������ ������ �����������, �� ����� �� ������ "..." �����������

//PRAGMA_ERROR("��� ������� ���������� F:\\VCProject\\FarPlugin\\PPCReg\\compile.cmd - Enter � ������� �� ������");

WARNING("� ������ VCon ������� ����� BYTE[256] ��� �������� ������������ ������ (Ctrl,...,Up,PgDn,Add,� ��.");
WARNING("�������������� ����� �������� � ����� {VKEY,wchar_t=0}, � ������� ��������� ��������� wchar_t �� WM_CHAR/WM_SYSCHAR");
WARNING("��� WM_(SYS)CHAR �������� wchar_t � ������, � ������ ��������� VKEY");
WARNING("��� ������������ WM_KEYUP - �����(� ������) wchar_t �� ����� ������, ������ � ������� UP");
TODO("� ������������ - ��������� �������� isKeyDown, � ������� �����");
WARNING("��� ������������ �� ������ ������� (�� �������� � � �������� ������ ������� - ����������� ����� ���� ������� � ����� ���������) ��������� �������� caps, scroll, num");
WARNING("� ����� ���������� �������/������� ��������� ����� �� �� ���������� Ctrl/Shift/Alt");

WARNING("����� ����� ��������������� ���������� ������ ������� ���������� (OK), �� ����������� ���������� ������� �� �������� � GUI - ��� ������� ���������� ������� � ������ ������");


//������, ��� ���������, ������ �������, ���������� �����, � ��...

#define VCURSORWIDTH 2
#define HCURSORHEIGHT 2

#define Free SafeFree
#define Alloc calloc

#define Assert(V) if ((V)==FALSE) { wchar_t szAMsg[MAX_PATH*2]; _wsprintf(szAMsg, SKIPLEN(countof(szAMsg)) L"Assertion (%s) at\n%s:%i", _T(#V), _T(__FILE__), __LINE__); Box(szAMsg); }

#ifdef _DEBUG
#define HEAPVAL MCHKHEAP
#else
#define HEAPVAL
#endif

#ifdef _DEBUG
#define FORCE_INVALIDATE_TIMEOUT 999999999
#else
#define FORCE_INVALIDATE_TIMEOUT 300
#endif


static BOOL gbInSendConEvent = FALSE;


wchar_t CRealConsole::ms_LastRConStatus[80] = {};

//static bool gbInTransparentAssert = false;

CRealConsole::CRealConsole(CVirtualConsole* apVCon)
{
	MCHKHEAP;
	SetConStatus(L"Initializing ConEmu..");
	mp_VCon = apVCon;
	HWND hView = apVCon->GetView();
	if (!hView)
	{
		_ASSERTE(hView!=NULL);
	}
	else
	{
		PostMessage(apVCon->GetView(), WM_SETCURSOR, -1, -1);
	}
	//mp_Rgn = new CRgnDetect();
	//mn_LastRgnFlags = -1;
	m_ConsoleKeyShortcuts = 0;
	memset(Title,0,sizeof(Title)); memset(TitleCmp,0,sizeof(TitleCmp));
	mn_tabsCount = 0; ms_PanelTitle[0] = 0; mn_ActiveTab = 0;
	mn_MaxTabs = 20; mb_TabsWasChanged = FALSE;
	mp_tabs = (ConEmuTab*)Alloc(mn_MaxTabs, sizeof(ConEmuTab));
	_ASSERTE(mp_tabs!=NULL);
	//memset(&m_PacketQueue, 0, sizeof(m_PacketQueue));
	mn_FlushIn = mn_FlushOut = 0;
	mb_MouseButtonDown = FALSE;
	mb_BtnClicked = FALSE; mrc_BtnClickPos = MakeCoord(-1,-1);
	mcr_LastMouseEventPos = MakeCoord(-1,-1);
	//m_DetectedDialogs.Count = 0;
	//mn_DetectCallCount = 0;
	wcscpy_c(Title, gpConEmu->GetDefaultTitle());
	wcscpy_c(TitleFull, Title);
	TitleAdmin[0] = 0;
	wcscpy_c(ms_PanelTitle, Title);
	mb_ForceTitleChanged = FALSE;
	mn_Progress = mn_PreWarningProgress = mn_LastShownProgress = -1; // ��������� ���
	mn_ConsoleProgress = mn_LastConsoleProgress = -1;
	mn_LastConProgrTick = mn_LastWarnCheckTick = 0;
	hPictureView = NULL; mb_PicViewWasHidden = FALSE;
	mh_MonitorThread = NULL; mn_MonitorThreadID = 0;
	mh_PostMacroThread = NULL; mn_PostMacroThreadID = 0;
	//mh_InputThread = NULL; mn_InputThreadID = 0;
	mp_sei = NULL;
	mn_ConEmuC_PID = 0; //mn_ConEmuC_Input_TID = 0;
	mh_ConEmuC = NULL; mh_ConEmuCInput = NULL; mb_UseOnlyPipeInput = FALSE;
	//mh_CreateRootEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	mb_InCreateRoot = FALSE;
	mb_NeedStartProcess = FALSE; mb_IgnoreCmdStop = FALSE;
	ms_ConEmuC_Pipe[0] = 0; ms_ConEmuCInput_Pipe[0] = 0; ms_VConServer_Pipe[0] = 0;
	mh_TermEvent = CreateEvent(NULL,TRUE/*MANUAL - ������������ � ���������� �����!*/,FALSE,NULL); ResetEvent(mh_TermEvent);
	mh_MonitorThreadEvent = CreateEvent(NULL,TRUE,FALSE,NULL); //2009-09-09 �������� Manual. ����� ��� �����������, ��� ����� ������� ���� Detached
	mh_ApplyFinished = CreateEvent(NULL,TRUE,FALSE,NULL);
	//mh_EndUpdateEvent = CreateEvent(NULL,FALSE,FALSE,NULL);
	//WARNING("mh_Sync2WindowEvent ������");
	//mh_Sync2WindowEvent = CreateEvent(NULL,FALSE,FALSE,NULL);
	//mh_ConChanged = CreateEvent(NULL,FALSE,FALSE,NULL);
	//mh_PacketArrived = CreateEvent(NULL,FALSE,FALSE,NULL);
	//mh_CursorChanged = NULL;
	//mb_Detached = FALSE;
	//m_Args.pszSpecialCmd = NULL; -- �� ���������
	mb_FullRetrieveNeeded = FALSE;
	memset(&m_LastMouse, 0, sizeof(m_LastMouse));
	memset(&m_LastMouseGuiPos, 0, sizeof(m_LastMouseGuiPos));
	mb_DataChanged = FALSE;
	mb_RConStartedSuccess = FALSE;
	ms_LogShellActivity[0] = 0; mb_ShellActivityLogged = false;
	mn_ProgramStatus = 0; mn_FarStatus = 0; mn_Comspec4Ntvdm = 0;
	isShowConsole = gpSet->isConVisible;
	//mb_ConsoleSelectMode = false;
	mn_SelectModeSkipVk = 0;
	mn_ProcessCount = 0;
	mn_FarPID = mn_ActivePID = 0; //mn_FarInputTID = 0;
	memset(m_FarPlugPIDs, 0, sizeof(m_FarPlugPIDs)); mn_FarPlugPIDsCount = 0;
	memset(m_TerminatedPIDs, 0, sizeof(m_TerminatedPIDs)); mn_TerminatedIdx = 0;
	mb_SkipFarPidChange = FALSE;
	mn_InRecreate = 0; mb_ProcessRestarted = FALSE; mb_InCloseConsole = FALSE;
	mn_LastSetForegroundPID = 0;
	mh_ServerSemaphore = NULL;
	memset(mh_RConServerThreads, 0, sizeof(mh_RConServerThreads));
	mh_ActiveRConServerThread = NULL;
	memset(mn_RConServerThreadsId, 0, sizeof(mn_RConServerThreadsId));
	mb_ThawRefreshThread = FALSE;
	
	//mb_BuferModeChangeLocked = FALSE;
	
	mn_DefaultBufferHeight = gpSetCls->bForceBufferHeight ? gpSetCls->nForceBufferHeight : gpSet->DefaultBufferHeight;
	
	mp_RBuf = new CRealBuffer(this);
	_ASSERTE(mp_RBuf!=NULL);
	mp_ABuf = mp_RBuf;
	mp_EBuf = NULL;
	mp_SBuf = NULL;
	mb_ABufChaged = false;
	
	mn_LastInactiveRgnCheck = 0;
	#ifdef _DEBUG
	mb_DebugLocked = FALSE;
	#endif
	
	ZeroStruct(m_ServerClosing);
	ZeroStruct(m_Args);
	mn_LastInvalidateTick = 0;

	hConWnd = NULL;
	hGuiWnd = NULL; mb_GuiExternMode = FALSE; mn_GuiWndStyle = mn_GuiWndStylEx = mn_GuiWndPID = 0;
	mb_InSetFocus = FALSE; mb_InGuiAttaching = FALSE;
	rcPreGuiWndRect = MakeRect(0,0);
	//hFileMapping = NULL; pConsoleData = NULL;
	mh_GuiAttached = NULL;
	mn_Focused = -1;
	mn_LastVKeyPressed = 0;
	mh_LogInput = NULL; mpsz_LogInputFile = NULL; //mpsz_LogPackets = NULL; mn_LogPackets = 0;
	//mh_FileMapping = mh_FileMappingData = mh_FarFileMapping =
	//mh_FarAliveEvent = NULL;
	//mp_ConsoleInfo = NULL;
	//mp_ConsoleData = NULL;
	//mp_FarInfo = NULL;
	mn_LastConsoleDataIdx = mn_LastConsolePacketIdx = /*mn_LastFarReadIdx =*/ -1;
	mn_LastFarReadTick = 0;
	//ms_HeaderMapName[0] = ms_DataMapName[0] = 0;
	//mh_ColorMapping = NULL;
	//mp_ColorHdr = NULL;
	//mp_ColorData = NULL;
	mn_LastColorFarID = 0;
	//ms_ConEmuC_DataReady[0] = 0; mh_ConEmuC_DataReady = NULL;
	m_UseLogs = gpSetCls->isAdvLogging;

	mp_TrueColorerData = NULL;
	memset(&m_TrueColorerHeader, 0, sizeof(m_TrueColorerHeader));

	//mb_PluginDetected = FALSE;
	mn_FarPID_PluginDetected = 0; //mn_Far_PluginInputThreadId = 0;
	memset(&m_FarInfo, 0, sizeof(m_FarInfo));
	lstrcpy(ms_Editor, L"edit ");
	MultiByteToWideChar(CP_ACP, 0, "�������������� ", -1, ms_EditorRus, countof(ms_EditorRus));
	lstrcpy(ms_Viewer, L"view ");
	MultiByteToWideChar(CP_ACP, 0, "�������� ", -1, ms_ViewerRus, countof(ms_ViewerRus));
	lstrcpy(ms_TempPanel, L"{Temporary panel");
	MultiByteToWideChar(CP_ACP, 0, "{��������� ������", -1, ms_TempPanelRus, countof(ms_TempPanelRus));
	//lstrcpy(ms_NameTitle, L"Name");
	SetTabs(NULL,1); // ��� ������ - ���������� ������� Console, � ��� ��� ����������
	PreInit(); // ������ ���������������� ���������� ��������...
	MCHKHEAP;
}

CRealConsole::~CRealConsole()
{
	DEBUGSTRCON(L"CRealConsole::~CRealConsole()\n");

	if (!gpConEmu->isMainThread())
	{
		//_ASSERTE(gpConEmu->isMainThread());
		MBoxA(L"~CRealConsole() called from background thread");
	}

	if (gbInSendConEvent)
	{
#ifdef _DEBUG
		_ASSERTE(gbInSendConEvent==FALSE);
#endif
		Sleep(100);
	}

	StopThread();
	MCHKHEAP

	if (mp_RBuf)
		{ delete mp_RBuf; mp_RBuf = NULL; }
	if (mp_EBuf)
		{ delete mp_EBuf; mp_EBuf = NULL; }
	if (mp_SBuf)
		{ delete mp_SBuf; mp_SBuf = NULL; }
	mp_ABuf = NULL;

		
	//// ��������, �.�. ��� ������ ������ SafeFree, � �� Free
	//if (m_Args.pszSpecialCmd)
	//    { Free(m_Args.pszSpecialCmd); m_Args.pszSpecialCmd = NULL; }
	//if (m_Args.pszStartupDir)
	//    { Free(m_Args.pszStartupDir); m_Args.pszStartupDir = NULL; }
	SafeCloseHandle(mh_ConEmuC); mn_ConEmuC_PID = 0; //mn_ConEmuC_Input_TID = 0;
	SafeCloseHandle(mh_ConEmuCInput);
	m_ConDataChanged.Close();
	SafeCloseHandle(mh_ServerSemaphore);
	SafeCloseHandle(mh_GuiAttached);

	//DeleteCriticalSection(&csPRC);
	//DeleteCriticalSection(&csCON);
	//DeleteCriticalSection(&csPKT);

	if (mp_tabs) Free(mp_tabs);

	mp_tabs = NULL; mn_tabsCount = 0; mn_ActiveTab = 0; mn_MaxTabs = 0;
	//
	CloseLogFiles();

	if (mp_sei)
	{
		SafeCloseHandle(mp_sei->hProcess);
		GlobalFree(mp_sei); mp_sei = NULL;
	}

	//CloseMapping();
	CloseMapHeader(); // CloseMapData() & CloseFarMapData() ����� ��� CloseMapHeader
	CloseColorMapping(); // Colorer data

	//if (mp_Rgn)
	//{
	//	delete mp_Rgn;
	//	mp_Rgn = NULL;
	//}
}

BOOL CRealConsole::PreCreate(RConStartArgs *args)
//(BOOL abDetached, LPCWSTR asNewCmd /*= NULL*/, , BOOL abAsAdmin /*= FALSE*/)
{
	_ASSERTE(args!=NULL);

	// 111211 - ����� ����� ���� ������� "-new_console:..."
	if (args->pszSpecialCmd)
	{
		// ������ ���� ��������� � ���������� ������� (CVirtualConsole::CreateVCon?)
		_ASSERTE(wcsstr(args->pszSpecialCmd, L"-new_console")==NULL);
		args->ProcessNewConArg();
	}

	mb_NeedStartProcess = FALSE;
	
	// ���� ���� - ����������� ������������ ������
	if (gpConEmu->mb_PortableRegExist)
	{
		// ���� ������ ���������, ��� ���� ������ "�� ����������"
		if (!gpConEmu->PreparePortableReg())
		{
			return FALSE;
		}
	}

	if (args->pszSpecialCmd /*&& !m_Args.pszSpecialCmd*/)
	{
		SafeFree(m_Args.pszSpecialCmd);
		_ASSERTE(args->bDetached == FALSE);
		m_Args.pszSpecialCmd = lstrdup(args->pszSpecialCmd);

		if (!m_Args.pszSpecialCmd)
			return FALSE;
	}

	// ���������� �������. � ����������� ������� ��������� � CurDir � conemu.exe,
	// �� ����� ���� ������ �� �������, ���� ������ ���� ����� "-new_console"
	_ASSERTE(m_Args.pszStartupDir==NULL);
	SafeFree(m_Args.pszStartupDir);
	if (args->pszStartupDir)
	{
		m_Args.pszStartupDir = lstrdup(args->pszStartupDir);

		if (!m_Args.pszStartupDir)
			return FALSE;
	}

	m_Args.bRunAsRestricted = args->bRunAsRestricted;
	m_Args.bRunAsAdministrator = args->bRunAsAdministrator;
	SafeFree(m_Args.pszUserName); //SafeFree(m_Args.pszUserPassword);
	SafeFree(m_Args.pszDomain);

	//if (m_Args.hLogonToken) { CloseHandle(m_Args.hLogonToken); m_Args.hLogonToken = NULL; }
	if (args->pszUserName)
	{
		m_Args.pszUserName = lstrdup(args->pszUserName);
		if (args->pszDomain)
			m_Args.pszDomain = lstrdup(args->pszDomain);
		lstrcpy(m_Args.szUserPassword, args->szUserPassword);
		SecureZeroMemory(args->szUserPassword, sizeof(args->szUserPassword));

		//m_Args.pszUserPassword = lstrdup(args->pszUserPassword ? args->pszUserPassword : L"");
		//m_Args.hLogonToken = args->hLogonToken; args->hLogonToken = NULL;
		if (!m_Args.pszUserName || !*m_Args.szUserPassword)
			return FALSE;
	}

	m_Args.bBackgroundTab = args->bBackgroundTab;
	m_Args.bBufHeight = args->bBufHeight;
	m_Args.nBufHeight = args->nBufHeight;
	m_Args.eConfirmation = args->eConfirmation;
	m_Args.bForceUserDialog = args->bForceUserDialog;

	if (m_Args.bBufHeight)
	{
		mn_DefaultBufferHeight = m_Args.nBufHeight;
		mp_RBuf->SetBufferHeightMode(mn_DefaultBufferHeight>0);
	}

	if (args->bDetached)
	{
		// ���� ������ �� ������ - ������ ��������� ��������� ����
		if (!PreInit())  //TODO: ������-�� PreInit() ��� �������� ������...
		{
			return FALSE;
		}

		m_Args.bDetached = TRUE;
	}
	else if (gpSetCls->nAttachPID)
	{
		// Attach - only once
		DWORD dwPID = gpSetCls->nAttachPID; gpSetCls->nAttachPID = 0;

		if (!AttachPID(dwPID))
		{
			return FALSE;
		}
	}
	else
	{
		mb_NeedStartProcess = TRUE;
	}

	if (!StartMonitorThread())
	{
		return FALSE;
	}
	
	// � ������� �������?
	args->bBackgroundTab = m_Args.bBackgroundTab;

	return TRUE;
}

RealBufferType CRealConsole::GetActiveBufferType()
{
	if (!this || !mp_ABuf)
		return rbt_Undefined;
	return mp_ABuf->m_Type;
}

void CRealConsole::DumpConsole(HANDLE ahFile)
{
	_ASSERTE(mp_ABuf!=NULL);
	
	return mp_ABuf->DumpConsole(ahFile);
}

bool CRealConsole::LoadDumpConsole(LPCWSTR asDumpFile)
{
	if (!this)
		return false;
	
	if (!mp_SBuf)
	{
		mp_SBuf = new CRealBuffer(this, rbt_DumpScreen);
		if (!mp_SBuf)
		{
			_ASSERTE(mp_SBuf!=NULL);
			return false;
		}
	}
	
	if (!mp_SBuf->LoadDumpConsole(asDumpFile))
	{
		return false;
	}
	
	mp_ABuf = mp_SBuf;
	
	return true;
}

bool CRealConsole::SetActiveBuffer(RealBufferType aBufferType)
{
	if (!this)
		return false;

	bool lbRc;
	switch (aBufferType)
	{
	case rbt_Primary:
		lbRc = SetActiveBuffer(mp_RBuf);
		break;
	default:
		// ������ ��� ���� �� ��������������
		_ASSERTE(aBufferType==rbt_Primary);
		lbRc = false;
	}

	//mp_VCon->Invalidate();

	return lbRc;
}

bool CRealConsole::SetActiveBuffer(CRealBuffer* aBuffer)
{
	if (!this)
		return false;
	if (!aBuffer || (aBuffer != mp_RBuf && aBuffer != mp_EBuf && aBuffer != mp_SBuf))
	{
		_ASSERTE(aBuffer && (aBuffer == mp_RBuf || aBuffer == mp_EBuf || aBuffer == mp_SBuf));
		return false;
	}
	
	mp_ABuf = mp_RBuf;
	mb_ABufChaged = true;
	
	// ����������� ���� MonitorThread
	SetEvent(mh_MonitorThreadEvent);
	
	return true;
}

BOOL CRealConsole::SetConsoleSize(USHORT sizeX, USHORT sizeY, USHORT sizeBuffer, DWORD anCmdID/*=CECMD_SETSIZESYNC*/)
{
	if (!this) return FALSE;
	
	// ������ ������ _��������_ ����� �������.
	return mp_RBuf->SetConsoleSize(sizeX, sizeY, sizeBuffer, anCmdID);
}

void CRealConsole::SyncGui2Window(RECT* prcClient)
{
	if (!this)
		return;

	if (hGuiWnd && !mb_GuiExternMode)
	{
		RECT rcClient;
		if (prcClient)
			rcClient = *prcClient;
		else
			rcClient = gpConEmu->GetGuiClientRect();

		RECT rcGui = gpConEmu->CalcRect(CER_WORKSPACE, rcClient, CER_MAINCLIENT, mp_VCon);
		OffsetRect(&rcGui, -rcGui.left, -rcGui.top);
		DWORD dwExStyle = GetWindowLong(hGuiWnd, GWL_EXSTYLE);
		DWORD dwStyle = GetWindowLong(hGuiWnd, GWL_STYLE);
		CorrectGuiChildRect(dwStyle, dwExStyle, rcGui);
		RECT rcCur = {};
		GetWindowRect(hGuiWnd, &rcCur);
		MapWindowPoints(NULL, GetView(), (LPPOINT)&rcCur, 2);
		if (memcmp(&rcCur, &rcGui, sizeof(RECT)) != 0)
		{
			// ����� ������� �����, � �� ���� �� "��� �������" ����� Access denied
			SetOtherWindowPos(hGuiWnd, HWND_TOP, rcGui.left,rcGui.top, rcGui.right-rcGui.left, rcGui.bottom-rcGui.top,
				SWP_ASYNCWINDOWPOS|SWP_NOACTIVATE);
		}
	}
}

// �������� ������ ������� �� ������� ���� (��������)
// prcNewWnd ���������� �� CConEmuMain::OnSizing(WPARAM wParam, LPARAM lParam)
// ��� ������������ ������� ������� (�� ��������� ��������� ��������� �������)
void CRealConsole::SyncConsole2Window(BOOL abNtvdmOff/*=FALSE*/, LPRECT prcNewWnd/*=NULL*/)
{
	if (!this)
		return;

	//2009-06-17 ��������� ���. ����� ������� � �������� ������ ������������� �� ������
	/*
	if (GetCurrentThreadId() != mn_MonitorThreadID) {
	    RECT rcClient; Get ClientRect(ghWnd, &rcClient);
	    _ASSERTE(rcClient.right>250 && rcClient.bottom>200);

	    // ��������� ������ ������ �������
	    RECT newCon = gpConEmu->CalcRect(CER_CONSOLE, rcClient, CER_MAINCLIENT);

	    if (newCon.right==TextWidth() && newCon.bottom==TextHeight())
	        return; // ������� �� ��������

	    SetEvent(mh_Sync2WindowEvent);
	    return;
	}
	*/
	DEBUGLOGFILE("SyncConsoleToWindow\n");
	RECT rcClient;

	if (prcNewWnd == NULL)
	{
		rcClient = gpConEmu->GetGuiClientRect();
	}
	else
	{
		rcClient = gpConEmu->CalcRect(CER_MAINCLIENT, *prcNewWnd, CER_MAIN);
	}

	//_ASSERTE(rcClient.right>140 && rcClient.bottom>100);
	// ��������� ������ ������ �������
	gpConEmu->AutoSizeFont(rcClient, CER_MAINCLIENT);
	RECT newCon = gpConEmu->CalcRect(abNtvdmOff ? CER_CONSOLE_NTVDMOFF : CER_CONSOLE, rcClient, CER_MAINCLIENT, mp_VCon);
	_ASSERTE(newCon.right>20 && newCon.bottom>6);
	
	if (hGuiWnd && !mb_GuiExternMode)
		SyncGui2Window(&rcClient);
	//{
	//	RECT rcGui = gpConEmu->CalcRect(CER_WORKSPACE, rcClient, CER_MAINCLIENT, mp_VCon);
	//	OffsetRect(&rcGui, -rcGui.left, -rcGui.top);
	//	DWORD dwExStyle = GetWindowLong(hGuiWnd, GWL_EXSTYLE);
	//	DWORD dwStyle = GetWindowLong(hGuiWnd, GWL_STYLE);
	//	CorrectGuiChildRect(dwStyle, dwExStyle, rcGui);
	//	RECT rcCur = {};
	//	GetWindowRect(hGuiWnd, &rcCur);
	//	MapWindowPoints(NULL, GetView(), (LPPOINT)&rcCur, 2);
	//	if (memcmp(&rcCur, &rcGui, sizeof(RECT)) != 0)
	//	{
	//		// ����� ������� �����, � �� ���� �� "��� �������" ����� Access denied
	//		SetOtherWindowPos(hGuiWnd, HWND_TOP, rcGui.left,rcGui.top, rcGui.right-rcGui.left, rcGui.bottom-rcGui.top,
	//			SWP_ASYNCWINDOWPOS|SWP_NOACTIVATE);
	//	}
	//}

	// ������ �������� ����� (����� ���� � �����������...)
	mp_ABuf->SyncConsole2Window(newCon.right, newCon.bottom);
}

// ���������� ��� ������ (����� ������), ��� ����� RunAs?
// sbi ���������� �� �������, � ������, ��� �� �� ������
BOOL CRealConsole::AttachConemuC(HWND ahConWnd, DWORD anConemuC_PID, const CESERVER_REQ_STARTSTOP* rStartStop, CESERVER_REQ_STARTSTOPRET* pRet)
{
	DWORD dwErr = 0;
	HANDLE hProcess = NULL;
	_ASSERTE(pRet!=NULL);

	// ������� ������� ����� ShellExecuteEx ��� ������ ������������� (Administrator)
	if (mp_sei)
	{
		// � ��������� ������� (10% �� x64) hProcess �� �������� ����������� (��� �� ���������� �������?)
		if (!mp_sei->hProcess)
		{
			DWORD dwStart = GetTickCount();
			DWORD dwDelay = 2000;

			do
			{
				Sleep(100);
			}
			while((GetTickCount() - dwStart) < dwDelay);

			_ASSERTE(mp_sei->hProcess!=NULL);
		}

		// �������
		if (mp_sei->hProcess)
		{
			hProcess = mp_sei->hProcess;
			mp_sei->hProcess = NULL; // ����� �� ���������. ����� ��������� � ������ �����
		}
	}
	else
	{
		SafeFree(m_Args.pszSpecialCmd);
		_ASSERTE(m_Args.bDetached == TRUE);
		m_Args.pszSpecialCmd = lstrdup(rStartStop->sCmdLine);
	}

	if (rStartStop->hServerProcessHandle)
	{
		if (hProcess)
			CloseHandle((HANDLE)rStartStop->hServerProcessHandle);
		else if (!hProcess)
			hProcess = (HANDLE)rStartStop->hServerProcessHandle;
	}

	// ����� - ���������� ��� ������
	if (!hProcess)
		hProcess = OpenProcess(PROCESS_QUERY_INFORMATION|SYNCHRONIZE, FALSE, anConemuC_PID);

	if (!hProcess)
	{
		DisplayLastError(L"Can't open ConEmuC process! Attach is impossible!", dwErr = GetLastError());
		return FALSE;
	}

	//2010-03-03 ���������� ��� ������ ����� ����
	CONSOLE_SCREEN_BUFFER_INFO lsbi = rStartStop->sbi;
	BOOL bCurBufHeight = rStartStop->bRootIsCmdExe || mp_RBuf->isScroll() || mp_RBuf->BufferHeightTurnedOn(&lsbi);

	// ������� �������� ����� - ���������� �� ������� ���������?
	if (mp_RBuf->isScroll() != bCurBufHeight)
	{
		_ASSERTE(mp_RBuf->isBuferModeChangeLocked()==FALSE);
		mp_RBuf->SetBufferHeightMode(bCurBufHeight, FALSE);
	}

	RECT rcWnd = gpConEmu->GetGuiClientRect();
	TODO("DoubleView: ?");
	gpConEmu->AutoSizeFont(rcWnd, CER_MAINCLIENT);
	RECT rcCon = gpConEmu->CalcRect(CER_CONSOLE, rcWnd, CER_MAINCLIENT);
	// ��������������� sbi �� �����, ������� ����� ���������� ����� ��������� �������� ������
	lsbi.dwSize.X = rcCon.right;
	lsbi.srWindow.Left = 0; lsbi.srWindow.Right = rcCon.right-1;

	if (bCurBufHeight)
	{
		// sbi.dwSize.Y �� �������
		lsbi.srWindow.Bottom = lsbi.srWindow.Top + rcCon.bottom - 1;
	}
	else
	{
		lsbi.dwSize.Y = rcCon.bottom;
		lsbi.srWindow.Top = 0; lsbi.srWindow.Bottom = rcCon.bottom - 1;
	}

	mp_RBuf->InitSBI(&lsbi);
	
	//// ������� "���������" ������� //2009-05-14 ������ ������� �������������� � GUI, �� ������ �� ������� ����� ��������� ������� �������
	//swprintf_c(ms_ConEmuC_Pipe, CE_CURSORUPDATE, mn_ConEmuC_PID);
	//mh_CursorChanged = CreateEvent ( NULL, FALSE, FALSE, ms_ConEmuC_Pipe );
	//if (!mh_CursorChanged) {
	//    ms_ConEmuC_Pipe[0] = 0;
	//    DisplayLastError(L"Can't create event!");
	//    return FALSE;
	//}
	SetHwnd(ahConWnd);
	ProcessUpdate(&anConemuC_PID, 1);
	mh_ConEmuC = hProcess;
	mn_ConEmuC_PID = anConemuC_PID;
	CreateLogFiles();
	// ���������������� ����� ������, �������, ��������� � �.�.
	InitNames();
	//// ��� ����� ��� ���������� ConEmuC
	//swprintf_c(ms_ConEmuC_Pipe, CESERVERPIPENAME, L".", mn_ConEmuC_PID);
	//swprintf_c(ms_ConEmuCInput_Pipe, CESERVERINPUTNAME, L".", mn_ConEmuC_PID);
	//MCHKHEAP
	// ������� map � �������, �� ��� ������ ���� ������
	OpenMapHeader(TRUE);
	//SetConsoleSize(MakeCoord(TextWidth,TextHeight));
	// �������� - ����, �.�. ������ ������ ������ ��� ���������� � ���� GUI
	//SetConsoleSize(rcCon.right,rcCon.bottom);
	pRet->bWasBufferHeight = bCurBufHeight;
	pRet->hWnd = ghWnd;
	pRet->hWndDC = mp_VCon->GetView();
	pRet->dwPID = GetCurrentProcessId();
	pRet->nBufferHeight = bCurBufHeight ? lsbi.dwSize.Y : 0;
	pRet->nWidth = rcCon.right;
	pRet->nHeight = rcCon.bottom;
	pRet->dwSrvPID = anConemuC_PID;
	pRet->bNeedLangChange = TRUE;
	TODO("��������� �� x64, �� ����� �� ������� � 0xFFFFFFFFFFFFFFFFFFFFF");
	pRet->NewConsoleLang = gpConEmu->GetActiveKeyboardLayout();
	// ���������� ����� ��� �������
	pRet->Font.cbSize = sizeof(pRet->Font);
	pRet->Font.inSizeY = gpSet->ConsoleFont.lfHeight;
	pRet->Font.inSizeX = gpSet->ConsoleFont.lfWidth;
	lstrcpy(pRet->Font.sFontName, gpSet->ConsoleFont.lfFaceName);
	// ����������� ���� MonitorThread
	SetEvent(mh_MonitorThreadEvent);
	return TRUE;
}

BOOL CRealConsole::AttachPID(DWORD dwPID)
{
	TODO("AttachPID ���� �������� �� �����");
	return FALSE;
#ifdef ALLOW_ATTACHPID
#ifdef MSGLOGGER
	TCHAR szMsg[100]; _wsprintf(szMsg, countof(szMsg), _T("Attach to process %i"), (int)dwPID);
	DEBUGSTRPROC(szMsg);
#endif
	BOOL lbRc = AttachConsole(dwPID);

	if (!lbRc)
	{
		DEBUGSTRPROC(_T(" - failed\n"));
		BOOL lbFailed = TRUE;
		DWORD dwErr = GetLastError();

		if (/*dwErr==0x1F || dwErr==6 &&*/ dwPID == -1)
		{
			// ���� ConEmu ����������� �� FAR'� �������� - �� ������������ ������� - CMD.EXE, � �� ��� ������ ����� ������. �� ���� ����������� �� �������
			HWND hConsole = FindWindowEx(NULL,NULL,_T("ConsoleWindowClass"),NULL);

			if (hConsole && IsWindowVisible(hConsole))
			{
				DWORD dwCurPID = 0;

				if (GetWindowThreadProcessId(hConsole,  &dwCurPID))
				{
					// PROCESS_ALL_ACCESS may fails on WinXP!
					HANDLE hProcess = OpenProcess((STANDARD_RIGHTS_REQUIRED|SYNCHRONIZE|0xFFF),FALSE,dwCurPID);
					dwErr = GetLastError();

					if (AttachConsole(dwCurPID))
						lbFailed = FALSE;
					else
						dwErr = GetLastError();
				}
			}
		}

		if (lbFailed)
		{
			TCHAR szErr[255];
			_wsprintf(szErr, countof(szErr), _T("AttachConsole failed (PID=%i)!"), dwPID);
			DisplayLastError(szErr, dwErr);
			return FALSE;
		}
	}

	DEBUGSTRPROC(_T(" - OK"));
	TODO("InitHandler � GUI �������� ��� � �� �����...");
	//InitHandlers(FALSE);
	// ���������� ������� ������ ��� ��������� ������.
	CConEmuPipe pipe;

	//DEBUGSTRPROC(_T("CheckProcesses\n"));
	//gpConEmu->CheckProcesses(0,TRUE);

	if (pipe.Init(_T("DefFont.in.attach"), TRUE))
		pipe.Execute(CMD_DEFFONT);

	return TRUE;
#endif
}

//BOOL CRealConsole::FlushInputQueue(DWORD nTimeout /*= 500*/)
//{
//	if (!this) return FALSE;
//
//	if (nTimeout > 1000) nTimeout = 1000;
//	DWORD dwStartTick = GetTickCount();
//
//	mn_FlushOut = mn_FlushIn;
//	mn_FlushIn++;
//
//	_ASSERTE(mn_ConEmuC_Input_TID!=0);
//
//	TODO("�������� ����� ���� ���� ����� ������� ���� � �� �������!");
//
//	//TODO("�������� ��������� ���� � �� ����������");
//	PostThreadMessage(mn_ConEmuC_Input_TID, INPUT_THREAD_ALIVE_MSG, mn_FlushIn, 0);
//
//	while (mn_FlushOut != mn_FlushIn) {
//		if (WaitForSingleObject(mh_ConEmuC, 100) == WAIT_OBJECT_0)
//			break; // ������� ������� ����������
//
//		DWORD dwCurTick = GetTickCount();
//		DWORD dwDelta = dwCurTick - dwStartTick;
//		if (dwDelta > nTimeout) break;
//	}
//
//	return (mn_FlushOut == mn_FlushIn);
//}

void CRealConsole::PostKeyPress(WORD vkKey, DWORD dwControlState, wchar_t wch, int ScanCode /*= -1*/)
{
	if (!this)
		return;

	if (ScanCode == -1)
		ScanCode = MapVirtualKey(vkKey, 0/*MAPVK_VK_TO_VSC*/);

	INPUT_RECORD r = {KEY_EVENT};
	r.Event.KeyEvent.bKeyDown = TRUE;
	r.Event.KeyEvent.wRepeatCount = 1;
	r.Event.KeyEvent.wVirtualKeyCode = vkKey;
	r.Event.KeyEvent.wVirtualScanCode = ScanCode;
	r.Event.KeyEvent.uChar.UnicodeChar = wch;
	r.Event.KeyEvent.dwControlKeyState = dwControlState;
	TODO("����� ����� � dwControlKeyState ��������� �����������, ���� �� � ���� vkKey?");
	PostConsoleEvent(&r);
	r.Event.KeyEvent.bKeyDown = FALSE;
	r.Event.KeyEvent.dwControlKeyState = dwControlState;
	PostConsoleEvent(&r);
}

void CRealConsole::PostKeyUp(WORD vkKey, DWORD dwControlState, wchar_t wch, int ScanCode /*= -1*/)
{
	if (!this)
		return;

	if (ScanCode == -1)
		ScanCode = MapVirtualKey(vkKey, 0/*MAPVK_VK_TO_VSC*/);

	INPUT_RECORD r = {KEY_EVENT};
	r.Event.KeyEvent.bKeyDown = FALSE;
	r.Event.KeyEvent.wRepeatCount = 1;
	r.Event.KeyEvent.wVirtualKeyCode = vkKey;
	r.Event.KeyEvent.wVirtualScanCode = ScanCode;
	r.Event.KeyEvent.uChar.UnicodeChar = wch;
	r.Event.KeyEvent.dwControlKeyState = dwControlState;
	PostConsoleEvent(&r);
}

void CRealConsole::PostLeftClickSync(COORD crDC)
{
	if (!this)
	{
		_ASSERTE(this!=NULL);
	}

	DWORD nFarPID = GetFarPID();
	if (!nFarPID)
	{
		_ASSERTE(nFarPID!=NULL);
		return;
	}

	COORD crMouse = ScreenToBuffer(mp_VCon->ClientToConsole(crDC.X, crDC.Y));
	CConEmuPipe pipe(nFarPID, CONEMUREADYTIMEOUT);

	if (pipe.Init(_T("CConEmuMain::EMenu"), TRUE))
	{
		gpConEmu->DebugStep(_T("PostLeftClickSync: Waiting for result (10 sec)"));

		DWORD nClickData[2] = {TRUE, MAKELONG(crMouse.X,crMouse.Y)};

		if (!pipe.Execute(CMD_LEFTCLKSYNC, nClickData, sizeof(nClickData)))
		{
			LogString("pipe.Execute(CMD_LEFTCLKSYNC) failed");
		}

		gpConEmu->DebugStep(NULL);
	}
}

void CRealConsole::PostConsoleEvent(INPUT_RECORD* piRec)
{
	if (!this) return;

	if (mn_ConEmuC_PID == 0 || !m_ConsoleMap.IsValid())
		return; // ������ ��� �� ���������. ������� ����� ���������...

	// ���� GUI-����� - ��� �������� � ����, � ������� ������ �� �����
	if (hGuiWnd)
	{
		if (piRec->EventType == KEY_EVENT)
		{
			UINT msg = WM_CHAR;
			WPARAM wParam = 0;
			LPARAM lParam = 0;
			
			if (piRec->Event.KeyEvent.bKeyDown && piRec->Event.KeyEvent.uChar.UnicodeChar)
				wParam = piRec->Event.KeyEvent.uChar.UnicodeChar;

			if (wParam || lParam)
				PostConsoleMessage(hGuiWnd, msg, wParam, lParam);
		}
		return;
	}

	//DWORD dwTID = 0;
	//#ifdef ALLOWUSEFARSYNCHRO
	//	if (isFar() && mn_FarInputTID) {
	//		dwTID = mn_FarInputTID;
	//	} else {
	//#endif
	//if (mn_ConEmuC_Input_TID == 0) // ������ ��� TID ����� �� ��������
	//	return;
	//dwTID = mn_ConEmuC_Input_TID;
	//#ifdef ALLOWUSEFARSYNCHRO
	//	}
	//#endif
	//if (dwTID == 0) {
	//	//_ASSERTE(dwTID!=0);
	//	gpConEmu->DebugStep(L"ConEmu: Input thread id is NULL");
	//	return;
	//}

	if (piRec->EventType == MOUSE_EVENT)
	{
#ifdef _DEBUG
		static DWORD nLastBtnState;
#endif
		//WARNING!!! ��� �������� ���������.
		// ��������� AltIns ������� ��������� MOUSE_MOVE � ��� �� ����������, ��� ������ ����.
		//  ����� �������� ���������� �� � "���������" � �� ��������� �������.
		// � ������ ������� ����������� �������� � �������. ��������, � �������
		//  UCharMap. ��� ��� �����������, ���� �������� �� ������ �����������
		//  � ��� ������� MOUSE_MOVE - �� ������ ��������� ��� ���������� ������ �����.
		//2010-07-12 ��������� �������� � CRealConsole::OnMouse � ������ GUI ������� �� ��������
		//if (piRec->Event.MouseEvent.dwEventFlags == MOUSE_MOVED)
		//{
		//    if (m_LastMouse.dwButtonState     == piRec->Event.MouseEvent.dwButtonState
		//     && m_LastMouse.dwControlKeyState == piRec->Event.MouseEvent.dwControlKeyState
		//     && m_LastMouse.dwMousePosition.X == piRec->Event.MouseEvent.dwMousePosition.X
		//     && m_LastMouse.dwMousePosition.Y == piRec->Event.MouseEvent.dwMousePosition.Y)
		//    {
		//        //#ifdef _DEBUG
		//        //wchar_t szDbg[60];
		//        //swprintf_c(szDbg, L"!!! Skipping ConEmu.Mouse event at: {%ix%i}\n", m_LastMouse.dwMousePosition.X, m_LastMouse.dwMousePosition.Y);
		//        //DEBUGSTRINPUT(szDbg);
		//        //#endif
		//        return; // ��� ������� ������. �������� ����� ������� �� ����, ������ �� ��������
		//    }
		//    #ifdef _DEBUG
		//    if ((nLastBtnState&FROM_LEFT_1ST_BUTTON_PRESSED)) {
		//    	nLastBtnState = nLastBtnState;
		//    }
		//    #endif
		//}
		#ifdef _DEBUG
		if (piRec->Event.MouseEvent.dwButtonState == RIGHTMOST_BUTTON_PRESSED
			&& ((piRec->Event.MouseEvent.dwControlKeyState & 9) != 9))
		{
			nLastBtnState = piRec->Event.MouseEvent.dwButtonState;
		}
		#endif
		
		// ��������
		m_LastMouse.dwMousePosition   = piRec->Event.MouseEvent.dwMousePosition;
		m_LastMouse.dwEventFlags      = piRec->Event.MouseEvent.dwEventFlags;
		m_LastMouse.dwButtonState     = piRec->Event.MouseEvent.dwButtonState;
		m_LastMouse.dwControlKeyState = piRec->Event.MouseEvent.dwControlKeyState;
#ifdef _DEBUG
		nLastBtnState = piRec->Event.MouseEvent.dwButtonState;
#endif
		//#ifdef _DEBUG
		//wchar_t szDbg[60];
		//swprintf_c(szDbg, L"ConEmu.Mouse event at: {%ix%i}\n", m_LastMouse.dwMousePosition.X, m_LastMouse.dwMousePosition.Y);
		//DEBUGSTRINPUT(szDbg);
		//#endif
	}
	else if (piRec->EventType == KEY_EVENT)
	{
		if (!piRec->Event.KeyEvent.wRepeatCount)
		{
			_ASSERTE(piRec->Event.KeyEvent.wRepeatCount!=0);
			piRec->Event.KeyEvent.wRepeatCount = 0;
		}
	}
	
	if (ghOpWnd && gpSetCls->hDebug && gpSetCls->m_ActivityLoggingType == glt_Input)
	{
		//INPUT_RECORD *prCopy = (INPUT_RECORD*)calloc(sizeof(INPUT_RECORD),1);
		CESERVER_REQ_PEEKREADINFO* pCopy = (CESERVER_REQ_PEEKREADINFO*)malloc(sizeof(CESERVER_REQ_PEEKREADINFO));
		if (pCopy)
		{
			pCopy->nCount = 1;
			pCopy->bMainThread = TRUE;
			pCopy->cPeekRead = 'S';
			pCopy->cUnicode = 'W';
			pCopy->Buffer[0] = *piRec;
			PostMessage(gpSetCls->hDebug, DBGMSG_LOG_ID, DBGMSG_LOG_INPUT_MAGIC, (LPARAM)pCopy);
		}
	}

	MSG64 msg = {sizeof(msg)};

	if (PackInputRecord(piRec, &msg))
	{
		if (m_UseLogs>=2)
			LogInput(piRec);

		_ASSERTE(msg.message!=0);
		//if (mb_UseOnlyPipeInput) {
		PostConsoleEventPipe(&msg);
#ifdef _DEBUG

		if (gbInSendConEvent)
		{
			_ASSERTE(!gbInSendConEvent);
		}

#endif
		//} else
		//// ERROR_INVALID_THREAD_ID == 1444 (0x5A4)
		//// On Vista PostThreadMessage failed with code 5, if target process created 'As administrator'
		//if (!PostThreadMessage(dwTID, msg.message, msg.wParam, msg.lParam)) {
		//	DWORD dwErr = GetLastError();
		//	if (dwErr == ERROR_ACCESS_DENIED/*5*/) {
		//		mb_UseOnlyPipeInput = TRUE;
		//		PostConsoleEventPipe(&msg);
		//	} else if (dwErr == ERROR_INVALID_THREAD_ID/*1444*/) {
		//		// In shutdown?
		//	} else {
		//		wchar_t szErr[100];
		//		swprintf_c(szErr, L"ConEmu: PostThreadMessage(%i) failed, code=0x%08X", dwTID, dwErr);
		//		gpConEmu->DebugStep(szErr);
		//	}
		//}
	}
	else
	{
		gpConEmu->DebugStep(L"ConEmu: PackInputRecord failed!");
	}
}

//DWORD CRealConsole::InputThread(LPVOID lpParameter)
//{
//    CRealConsole* pRCon = (CRealConsole*)lpParameter;
//
//    MSG msg;
//    while (GetMessage(&msg,0,0,0)) {
//    	if (msg.message == WM_QUIT) break;
//    	if (WaitForSingleObject(pRCon->mh_TermEvent, 0) == WAIT_OBJECT_0) break;
//
//    	if (msg.message == INPUT_THREAD_ALIVE_MSG) {
//    		pRCon->mn_FlushOut = msg.wParam;
//    		continue;
//
//    	} else {
//
//    		INPUT_RECORD r = {0};
//
//    		if (UnpackInputRecord(&msg, &r)) {
//    			pRCon->SendConsoleEvent(&r);
//    		}
//
//    	}
//    }
//
//    return 0;
//}

DWORD CRealConsole::MonitorThread(LPVOID lpParameter)
{
	BOOL lbChildProcessCreated = FALSE;
	CRealConsole* pRCon = (CRealConsole*)lpParameter;

	BOOL bDetached = pRCon->m_Args.bDetached;

	pRCon->SetConStatus(bDetached ? L"Detached" : L"Initializing RealConsole...");

	if (pRCon->mb_NeedStartProcess)
	{
		_ASSERTE(pRCon->mh_ConEmuC==NULL);
		pRCon->mb_NeedStartProcess = FALSE;

		if (!pRCon->StartProcess())
		{
			wchar_t szErrInfo[128];
			_wsprintf(szErrInfo, SKIPLEN(countof(szErrInfo)) L"Can't start root process, ErrCode=0x%08X...", GetLastError());
			DEBUGSTRPROC(L"### Can't start process\n");
			pRCon->SetConStatus(szErrInfo);
			return 0;
		}

		// ���� ConEmu ��� ������� � ������ "/single /cmd xxx" �� ����� ���������
		// �������� - �������� �������, ������� ������ �� "/cmd" - ��������� ���������
		if (gpSetCls->SingleInstanceArg)
		{
			gpSetCls->ResetCmdArg();
		}
	}

	//_ASSERTE(pRCon->mh_ConChanged!=NULL);
	// ���� ���� ����������� - ��������� "�����" ��� ��� ��� ���������...
	//_ASSERTE(pRCon->mb_Detached || pRCon->mh_ConEmuC!=NULL);

	// � ��� �� ����� ������ �������...

	#define IDEVENT_TERM  0              // ���������� ����/�������/conemu
	#define IDEVENV_MONITORTHREADEVENT 1 // ������������, ����� ������� Update & Invalidate
	#define IDEVENT_SERVERPH 2           // ConEmuC.exe process handle (server)
	#define EVENTS_COUNT (IDEVENT_SERVERPH+1)

	HANDLE hEvents[EVENTS_COUNT];
	_ASSERTE(EVENTS_COUNT==countof(hEvents)); // ��������� �����������

	hEvents[IDEVENT_TERM] = pRCon->mh_TermEvent;
	hEvents[IDEVENV_MONITORTHREADEVENT] = pRCon->mh_MonitorThreadEvent; // ������������, ����� ������� Update & Invalidate
	hEvents[IDEVENT_SERVERPH] = pRCon->mh_ConEmuC;

	DWORD  nEvents = countof(hEvents);

	// ������ ����� ����� NULL, ���� ������ ���� ����� ShellExecuteEx(runas)
	if (hEvents[IDEVENT_SERVERPH] == NULL)
		nEvents --;

	DWORD  nWait = 0, nSrvWait = -1;
	BOOL   bException = FALSE, bIconic = FALSE, /*bFirst = TRUE,*/ bActive = TRUE, bGuiVisible = FALSE;
	DWORD nElapse = max(10,gpSet->nMainTimerElapse);
	DWORD nInactiveElapse = max(10,gpSet->nMainTimerInactiveElapse);
	DWORD nLastFarPID = 0;
	bool bLastAlive = false, bLastAliveActive = false;
	bool lbForceUpdate = false;
	WARNING("���������� �������� �� hDataReadyEvent, ������� ������������ � �������?");
	TODO("���� �� ����������� ��� F10 � ���� - �������� ���� �� ����������������...");
	DWORD nConsoleStartTick = GetTickCount();
	DWORD nTimeout = 0;
	CRealBuffer* pLastBuf = NULL;
	bool lbActiveBufferChanged = false;

	while (TRUE)
	{
		bActive = pRCon->isActive();

		if (bActive)
			gpSetCls->Performance(tPerfInterval, TRUE); // ��������� �� ������

		#ifdef _DEBUG
		int nVConNo = gpConEmu->IsVConValid(pRCon->mp_VCon);
		nVConNo = nVConNo;
		#endif

		// ��������, ����� �������� ������ "�������" �������?
		if (hEvents[IDEVENT_SERVERPH] == NULL && pRCon->mh_ConEmuC)
		{
			nSrvWait = WaitForSingleObject(pRCon->mh_ConEmuC,0);
			if (nSrvWait == WAIT_OBJECT_0)
			{
				// ConEmuC was terminated!
				_ASSERTE(bDetached == FALSE);
				nWait = IDEVENT_SERVERPH;
				break;
			}
		}

		bIconic = gpConEmu->isIconic();
		// � ����������������/���������� ������ - ��������� �������
		nTimeout = bIconic ? max(1000,nInactiveElapse) : !bActive ? nInactiveElapse : nElapse;
		nWait = WaitForMultipleObjects(nEvents, hEvents, FALSE, nTimeout);
		_ASSERTE(nWait!=(DWORD)-1);

		if (nWait == IDEVENT_TERM || nWait == IDEVENT_SERVERPH)
		{
			//if (nWait == IDEVENT_SERVERPH) -- �����
			//{
			//	DEBUGSTRPROC(L"### ConEmuC.exe was terminated\n");
			//}

			break; // ���������� ���������� ����
		}

		// ��� ������� ������ ManualReset
		if (nWait == IDEVENV_MONITORTHREADEVENT
		        || WaitForSingleObject(hEvents[IDEVENV_MONITORTHREADEVENT],0) == WAIT_OBJECT_0)
		{
			ResetEvent(hEvents[IDEVENV_MONITORTHREADEVENT]);

			// ���� �� ��������, ��������, ��� ������� ����� ������� "��� �������", ����� GUI �� "��� �������"
			// � ������ ���� (pRCon->mh_ConEmuC == NULL), ���� ������ ���� ����� ShellExecuteEx(runas)
			if (hEvents[IDEVENT_SERVERPH] == NULL)
			{
				if (pRCon->mh_ConEmuC)
				{
					if (bDetached || pRCon->m_Args.bRunAsAdministrator)
					{
						bDetached = FALSE;
						hEvents[IDEVENT_SERVERPH] = pRCon->mh_ConEmuC;
						nEvents = countof(hEvents);
					}
					else
					{
						_ASSERTE(bDetached==TRUE);
					}
				}
				else
				{
					_ASSERTE(pRCon->mh_ConEmuC!=NULL);
				}
			}
		}

		if (!lbChildProcessCreated
			&& (pRCon->mn_ProcessCount > 1)
			&& ((GetTickCount() - nConsoleStartTick) > PROCESS_WAIT_START_TIME))
		{
			lbChildProcessCreated = TRUE;
			pRCon->OnRConStartedSuccess();
		}

		// IDEVENT_SERVERPH ��� ��������, � ��� �������� ������������ ��� ������ �� �����
		//// ��������, ��� ConEmuC ���
		//if (pRCon->mh_ConEmuC)
		//{
		//	DWORD dwExitCode = 0;
		//	#ifdef _DEBUG
		//	BOOL fSuccess =
		//	#endif
		//	    GetExitCodeProcess(pRCon->mh_ConEmuC, &dwExitCode);
		//	if (dwExitCode!=STILL_ACTIVE)
		//	{
		//		pRCon->StopSignal();
		//		return 0;
		//	}
		//}

		// ���� ������� �� ������ ���� �������� - �� �� ���-�� ���������
		if (!pRCon->isShowConsole && !gpSet->isConVisible)
		{
			/*if (foreWnd == hConWnd)
			    apiSetForegroundWindow(ghWnd);*/
			bool bMonitorVisibility = true;
#ifdef _DEBUG

			if ((GetKeyState(VK_SCROLL) & 1))
				bMonitorVisibility = false;

			WARNING("bMonitorVisibility = false - ��� ������ ������ ������");
			bMonitorVisibility = false;
#endif

			if (bMonitorVisibility && IsWindowVisible(pRCon->hConWnd))
				pRCon->ShowOtherWindow(pRCon->hConWnd, SW_HIDE);
		}

		// ������ ������� ������ � ��� �����, � ������� ��� ���������. ����� ����� ��������������� ��� Update (InitDC)
		// ��������� ��������� �������� �������
		/*if (nWait == (IDEVENT_SYNC2WINDOW)) {
		    pRCon->SetConsoleSize(pRCon->m_ReqSetSize);
		    //SetEvent(pRCon->mh_ReqSetSizeEnd);
		    //continue; -- � ����� ������� ���������� � ���
		}*/
		DWORD dwT1 = GetTickCount();
		SAFETRY
		{
			//ResetEvent(pRCon->mh_EndUpdateEvent);

			// ��� � ApplyConsole ����������
			//if (pRCon->mp_ConsoleInfo)

			lbActiveBufferChanged = (pRCon->mp_ABuf != pLastBuf);
			if (lbActiveBufferChanged)
			{
				pRCon->mb_ABufChaged = lbForceUpdate = true;
			}

			if (pRCon->mp_RBuf != pRCon->mp_ABuf)
			{
				pRCon->mn_LastFarReadTick = GetTickCount();
				if (lbActiveBufferChanged || pRCon->mp_ABuf->isConsoleDataChanged())
					lbForceUpdate = true;
			}
			// ���� ������ ��� - ����� ��������� ������� ���� � ��� ������
			else if ((!pRCon->mb_SkipFarPidChange) && pRCon->m_ConsoleMap.IsValid())
			{
				bool lbFarChanged = false;
				// Alive?
				DWORD nCurFarPID = pRCon->GetFarPID(TRUE);

				if (!nCurFarPID)
				{
					// ��������, �������� FAR (������� �� cmd.exe, ��� ���������� ����)
					DWORD nPID = pRCon->GetFarPID(FALSE);

					if (nPID)
					{
						for (UINT i = 0; i < pRCon->mn_FarPlugPIDsCount; i++)
						{
							if (pRCon->m_FarPlugPIDs[i] == nPID)
							{
								pRCon->mn_FarPID_PluginDetected = nCurFarPID = nPID;
								break;
							}
						}
					}
				}

				// ���� ���� (� ��������) ���, � ������ ���
				if (!nCurFarPID && nLastFarPID)
				{
					// ������� � �������� PID
					pRCon->CloseFarMapData();
					nLastFarPID = 0;
					lbFarChanged = true;
				}

				// ���� PID ���� (� ��������) ��������
				if (nCurFarPID && nLastFarPID != nCurFarPID)
				{
					//pRCon->mn_LastFarReadIdx = -1;
					pRCon->mn_LastFarReadTick = 0;
					nLastFarPID = nCurFarPID;

					// ������������� ������� ��� ����� PID ����
					// (�� ������ ���� ��������� ������, ������� ������� � ��������� � ������)
					if (!pRCon->OpenFarMapData())
					{
						// ������ ��� ���� ��� (��� ���) ���?
						if (pRCon->mn_FarPID_PluginDetected == nCurFarPID)
						{
							for (UINT i = 0; i < pRCon->mn_FarPlugPIDsCount; i++)  // �������� �� ������ ��������
							{
								if (pRCon->m_FarPlugPIDs[i] == nCurFarPID)
									pRCon->m_FarPlugPIDs[i] = 0;
							}

							pRCon->mn_FarPID_PluginDetected = 0;
						}
					}

					lbFarChanged = true;
				}

				bool bAlive = false;

				//PRAGMA_ERROR("���������� �� ������� ��� mp_FarInfo");
				//if (nCurFarPID && pRCon->mn_LastFarReadIdx != pRCon->mp_ConsoleInfo->nFarReadIdx) {
				if (nCurFarPID && pRCon->m_FarInfo.cbSize && pRCon->m_FarAliveEvent.Open())
				{
					DWORD nCurTick = GetTickCount();

					// ����� ����� ������� �� ��� ������� �����.
					if (pRCon->mn_LastFarReadTick == 0 ||
					        (nCurTick - pRCon->mn_LastFarReadTick) >= (FAR_ALIVE_TIMEOUT/2))
					{
						//if (WaitForSingleObject(pRCon->mh_FarAliveEvent, 0) == WAIT_OBJECT_0)
						if (pRCon->m_FarAliveEvent.Wait(0) == WAIT_OBJECT_0)
						{
							pRCon->mn_LastFarReadTick = nCurTick ? nCurTick : 1;
							bAlive = true; // �����
						}

#ifdef _DEBUG
						else
						{
							pRCon->mn_LastFarReadTick = nCurTick - FAR_ALIVE_TIMEOUT - 1;
							bAlive = false; // �����
						}

#endif
					}
					else
					{
						bAlive = true; // ��� �� ������ ����������
					}

					//if (pRCon->mn_LastFarReadIdx != pRCon->mp_FarInfo->nFarReadIdx) {
					//	pRCon->mn_LastFarReadIdx = pRCon->mp_FarInfo->nFarReadIdx;
					//	pRCon->mn_LastFarReadTick = GetTickCount();
					//	DEBUGSTRALIVE(L"*** FAR ReadTick updated\n");
					//	bAlive = true;
					//}
				}
				else
				{
					bAlive = true; // ���� ��� ���������� �������, ��� ��� �� ���
				}

				//if (!bAlive) {
				//	bAlive = pRCon->isAlive();
				//}
				if (pRCon->isActive())
				{
					WARNING("��� ����� �� ���������� � ����������, ���������� � gpConEmu, � �� � ���� instance RCon!");
#ifdef _DEBUG

					if (!IsDebuggerPresent())
					{
						bool lbIsAliveDbg = pRCon->isAlive();

						if (lbIsAliveDbg != bAlive)
						{
							_ASSERTE(lbIsAliveDbg == bAlive);
						}
					}

#endif

					if (bLastAlive != bAlive || !bLastAliveActive)
					{
						DEBUGSTRALIVE(bAlive ? L"MonitorThread: Alive changed to TRUE\n" : L"MonitorThread: Alive changed to FALSE\n");
						PostMessage(pRCon->GetView(), WM_SETCURSOR, -1, -1);
					}

					bLastAliveActive = true;

					if (lbFarChanged)
						gpConEmu->UpdateProcessDisplay(FALSE); // �������� PID � ���� ���������
				}
				else
				{
					bLastAliveActive = false;
				}

				bLastAlive = bAlive;
				//����� �� ����
				//UpdateFarSettings(mn_FarPID_PluginDetected);
				// ��������� ��������� �� �������
				//if ((HWND)pRCon->mp_ConsoleInfo->hConWnd && pRCon->mp_ConsoleInfo->nCurDataMapIdx
				//	&& pRCon->mp_ConsoleInfo->nPacketId
				//	&& pRCon->mn_LastConsolePacketIdx != pRCon->mp_ConsoleInfo->nPacketId)
				WARNING("!!! ���� �������� m_ConDataChanged ����� ��������� ���� - �� ��� ����� ���������� ���������� ���� �������� !!!");

				if (!pRCon->m_ConDataChanged.Wait(0,TRUE))
				{
					// ���� �������� ������ (Far/�� Far) - ������������ �� ������ ������, 
					// ����� ����� �������� �� �������, �������������� ������������ � ������ ����
					_ASSERTE(pRCon->mp_RBuf==pRCon->mp_ABuf);
					if (pRCon->mp_RBuf->ApplyConsoleInfo())
						lbForceUpdate = true;
				}
			}

			bool bCheckStatesFindPanels = false, lbForceUpdateProgress = false;

			// ���� ������� ��������� - CVirtualConsole::Update �� ���������� � ������� �� ����������. � ��� ���������.
			// ������� ������ mp_ABuf, �.�. ����� ��� ���������� ��, ��� ����� �������� �� ������!
			if ((!bActive || bIconic) && (lbActiveBufferChanged || pRCon->mp_ABuf->isConsoleDataChanged()))
			{
				DWORD nCurTick = GetTickCount();
				DWORD nDelta = nCurTick - pRCon->mn_LastInactiveRgnCheck;

				if (nDelta > CONSOLEINACTIVERGNTIMEOUT)
				{
					pRCon->mn_LastInactiveRgnCheck = nCurTick;

					// ���� ��� ������ ConEmu ������� ��������� �������� ����� '@'
					// �� ��� ����� �������� - �� ���������������� (InitDC �� ���������),
					// ��� ����� ������ � ������� ����, LoadConsoleData �� ���� �����������
					if (pRCon->mp_VCon->LoadConsoleData())
						bCheckStatesFindPanels = true;
				}
			}


			// �������� �������, ����� ������, � �.�.
			if (pRCon->mb_DataChanged || pRCon->mb_TabsWasChanged)
			{
				lbForceUpdate = true; // ����� ���� ������� ��������� - �� ������ ��� �� ��������� ����������� ��� �����...
				pRCon->mb_TabsWasChanged = FALSE;
				pRCon->mb_DataChanged = FALSE;
				// ������� ��������� ������ ms_PanelTitle, ����� ��������
				// ���������� ����� � ��������, �������������� �������
				pRCon->CheckPanelTitle();
				// �������� ���� CheckFarStates & FindPanels
				bCheckStatesFindPanels = true;
			}

			if (!bCheckStatesFindPanels)
			{
				// ���� ���� ���������� "������" ��������� - ��������� ������.
				// ��� ����� ����� ��������� ��� ���������� ����� �� ������ ����� MA.
				// �������� ����� (������� ���), �������� ��������, ������ ����������, ��
				// ���� �� ���������� ���� �����-������ ��������� � ������� - ������ �� ����������.
				if (pRCon->mn_LastWarnCheckTick || pRCon->mn_FarStatus & (CES_WASPROGRESS|CES_OPER_ERROR))
					bCheckStatesFindPanels = true;
			}

			if (bCheckStatesFindPanels)
			{
				// ������� mn_FarStatus & mn_PreWarningProgress
				// ��������� ������� �� ������������ ��������!
				// � ������� ���� ��������� ������, ��������,
				// mp_ABuf->GetDetector()->GetFlags()
				pRCon->CheckFarStates();
				// ���� �������� ������ - ����� �� � �������,
				// ������ ������ ��������� CheckProgressInConsole
				pRCon->mp_RBuf->FindPanels();
			}

			if (pRCon->mn_ConsoleProgress == -1 && pRCon->mn_LastConsoleProgress >= 0)
			{
				// ���� ����� ��������� 7z - ������ �������� � ������, ����� �� ����� ������ ��������� ��� ���
				DWORD nDelta = GetTickCount() - pRCon->mn_LastConProgrTick;

				if (nDelta >= CONSOLEPROGRESSTIMEOUT)
				{
					pRCon->mn_LastConsoleProgress = -1; pRCon->mn_LastConProgrTick = 0;
					lbForceUpdateProgress = true;
				}
			}


			// ��������� ���-������� - ��� ������ "��������� ������"
			if (pRCon->hGuiWnd && bActive)
			{
				BOOL lbVisible = ::IsWindowVisible(pRCon->hGuiWnd);
				if (lbVisible != bGuiVisible)
				{
					gpConEmu->OnBufferHeight();
					bGuiVisible = lbVisible;
				}
			}

			if (pRCon->hConWnd || pRCon->hGuiWnd)  // ���� ����� ����� ���� -
				GetWindowText(pRCon->hGuiWnd ? pRCon->hGuiWnd : pRCon->hConWnd, pRCon->TitleCmp, countof(pRCon->TitleCmp)-2);

			// ��������, ��������� �������� ��������
			//bool lbCheckProgress = (pRCon->mn_PreWarningProgress != -1);

			if (pRCon->mb_ForceTitleChanged
			        || wcscmp(pRCon->Title, pRCon->TitleCmp))
			{
				pRCon->mb_ForceTitleChanged = FALSE;
				pRCon->OnTitleChanged();
				lbForceUpdateProgress = false; // �������� ��������
			}
			else if (bActive)
			{
				// ���� � ������� ��������� �� �������, �� �� ���������� �� ��������� � ConEmu
				if (wcscmp(pRCon->GetTitle(), gpConEmu->GetLastTitle(false)))
					gpConEmu->UpdateTitle();
			}

			if (lbForceUpdateProgress)
			{
				gpConEmu->UpdateProgress();
			}

			//if (lbCheckProgress && pRCon->mn_LastShownProgress >= 0) {
			//	if (pRCon->GetProgress(NULL) != -1) {
			//		pRCon->OnTitleChanged();
			//	}
			//	//DWORD nDelta = GetTickCount() - pRCon->mn_LastProgressTick;
			//	//if (nDelta >= 500) {
			//	//}
			//}

			bool lbIsActive = pRCon->isActive();

			#ifdef _DEBUG
			if (pRCon->mb_DebugLocked)
				lbIsActive = false;
			#endif

			TODO("����� DoubleView ����� ����� �������� �� IsVisible");
			if (!lbIsActive)
			{
				if (lbForceUpdate)
					pRCon->mp_VCon->UpdateThumbnail();
			}
			else
			{
				//2009-01-21 �����������, ��� ����� ������������� ����� �������������� �������� ����
				//if (lbForceUpdate) // ������ �������� ����������� ���� ��� �������
				//	gpConEmu->OnSize(-1); // ������� � ������� ���� ������ �� ���������� �������
				bool lbNeedRedraw = false;

				if ((nWait == (WAIT_OBJECT_0+1)) || lbForceUpdate)
				{
					//2010-05-18 lbForceUpdate ������� CVirtualConsole::Update(abForce=true), ��� ��������� � ��������
					bool bForce = false; //lbForceUpdate;
					lbForceUpdate = false;
					pRCon->mp_VCon->Validate(); // �������� ������

					if (pRCon->m_UseLogs>2) pRCon->LogString("mp_VCon->Update from CRealConsole::MonitorThread");

					if (pRCon->mp_VCon->Update(bForce))
						lbNeedRedraw = true;
				}
				else if (gpSet->isCursorBlink)
				{
					// ��������, ������� ����� ������� ��������?
					bool lbNeedBlink = false;
					pRCon->mp_VCon->UpdateCursor(lbNeedBlink);

					// UpdateCursor Invalidate �� �����
					if (lbNeedBlink)
					{
						if (pRCon->m_UseLogs>2) pRCon->LogString("Invalidating from CRealConsole::MonitorThread.1");

						lbNeedRedraw = true;
					}
				}
				else if (((GetTickCount() - pRCon->mn_LastInvalidateTick) > FORCE_INVALIDATE_TIMEOUT))
				{
					DEBUGSTRDRAW(L"+++ Force invalidate by timeout\n");

					if (pRCon->m_UseLogs>2) pRCon->LogString("Invalidating from CRealConsole::MonitorThread.2");

					lbNeedRedraw = true;
				}

				// ���� ����� ��������� - ������ �������� ����
				if (lbNeedRedraw)
				{
					//#ifndef _DEBUG
					//WARNING("******************");
					TODO("����� ����� ������� ��������� �� ����������?");
					pRCon->mp_VCon->Redraw();
					//#endif
					pRCon->mn_LastInvalidateTick = GetTickCount();
				}
			}

			pLastBuf = pRCon->mp_ABuf;

		} SAFECATCH
		{
			bException = TRUE;
		}
		// ����� �� ���� ������� ������� ��������� (����� ��������� ����������� �� 100%)
		// ������ ����� ������ ��������
		DWORD dwT2 = GetTickCount();
		DWORD dwD = max(10,(dwT2 - dwT1));
		nElapse = (DWORD)(nElapse*0.7 + dwD*0.3);

		if (nElapse > 1000) nElapse = 1000;  // ������ ������� - �� �����! ����� ������ ������ �� �����

		if (bException)
		{
			bException = FALSE;
#ifdef _DEBUG
			_ASSERTE(FALSE);
#endif
			pRCon->Box(_T("Exception triggered in CRealConsole::MonitorThread"));
		}

		//if (bActive)
		//	gpSetCls->Performance(tPerfInterval, FALSE);

		if (pRCon->m_ServerClosing.nServerPID
		        && pRCon->m_ServerClosing.nServerPID == pRCon->mn_ConEmuC_PID
		        && (GetTickCount() - pRCon->m_ServerClosing.nRecieveTick) >= SERVERCLOSETIMEOUT)
		{
			// ������, ������ ����� �� ����� ������?
			pRCon->isConsoleClosing(); // ������� ������� TerminateProcess(mh_ConEmuC)
			nWait = IDEVENT_SERVERPH;
			break;
		}
	}

	if (nWait == IDEVENT_SERVERPH)
	{
		DEBUGSTRPROC(L"### ConEmuC.exe was terminated\n");
		DWORD nExitCode = 999;
		GetExitCodeProcess(pRCon->mh_ConEmuC, &nExitCode);
		wchar_t szErrInfo[255];
		_wsprintf(szErrInfo, SKIPLEN(countof(szErrInfo))
			(nExitCode > 0 && nExitCode <= 2048) ?
				L"Server process was terminated, ExitCode=%i" :
				L"Server process was terminated, ExitCode=0x%08X",
			nExitCode);
		if (nExitCode == 0xC000013A)
			wcscat_c(szErrInfo, L" (by Ctrl+C)");
		//if (nExitCode >= CERR_FIRSTEXITCODE && nExitCode <= CERR_LASTEXITCODE)
		//{
		//}
		if (nExitCode == 0)
		{
			pRCon->SetConStatus(NULL);
			// � ��� ����� �� �������� ������ ���� ConEmu, ��� ������ ���������� ���������
			if (!lbChildProcessCreated)
				pRCon->OnRConStartedSuccess();
		}
		else
		{
			pRCon->SetConStatus(szErrInfo);
		}
	}

	pRCon->StopSignal();
	// 2010-08-03 - ������� � StopThread, ����� �� ����������� �������� ���� ����
	//// ���������� ��������� ����� ���� �������
	//DEBUGSTRPROC(L"About to terminate main server thread (MonitorThread)\n");
	//if (pRCon->ms_VConServer_Pipe[0]) // ������ ���� �� ���� ���� ���� ��������
	//{
	//    pRCon->StopSignal(); // ��� ������ ���� ���������, �� �� ������ ������
	//    //
	//    HANDLE hPipe = INVALID_HANDLE_VALUE;
	//    DWORD dwWait = 0;
	//    // ����������� �����, ����� ���� ������� �����������
	//    for (int i=0; i<MAX_SERVER_THREADS; i++) {
	//        DEBUGSTRPROC(L"Touching our server pipe\n");
	//        HANDLE hServer = pRCon->mh_ActiveRConServerThread;
	//        hPipe = CreateFile(pRCon->ms_VConServer_Pipe,GENERIC_WRITE,0,NULL,OPEN_EXISTING,0,NULL);
	//        if (hPipe == INVALID_HANDLE_VALUE) {
	//            DEBUGSTRPROC(L"All pipe instances closed?\n");
	//            break;
	//        }
	//        DEBUGSTRPROC(L"Waiting server pipe thread\n");
	//        dwWait = WaitForSingleObject(hServer, 200); // �������� ���������, ���� ���� ����������
	//        // ������ ������� ���� - ��� ����� ���� �����������
	//        CloseHandle(hPipe);
	//        hPipe = INVALID_HANDLE_VALUE;
	//    }
	//    // ������� ��������, ���� ��� ���� ����������
	//    DEBUGSTRPROC(L"Checking server pipe threads are closed\n");
	//    WaitForMultipleObjects(MAX_SERVER_THREADS, pRCon->mh_RConServerThreads, TRUE, 500);
	//    for (int i=0; i<MAX_SERVER_THREADS; i++) {
	//        if (WaitForSingleObject(pRCon->mh_RConServerThreads[i],0) != WAIT_OBJECT_0) {
	//            DEBUGSTRPROC(L"### Terminating mh_RConServerThreads\n");
	//            TerminateThread(pRCon->mh_RConServerThreads[i],0);
	//        }
	//        CloseHandle(pRCon->mh_RConServerThreads[i]);
	//        pRCon->mh_RConServerThreads[i] = NULL;
	//    }
	//	pRCon->ms_VConServer_Pipe[0] = 0;
	//}
	// Finalize
	//SafeCloseHandle(pRCon->mh_MonitorThread);
	DEBUGSTRPROC(L"Leaving MonitorThread\n");
	return 0;
}

BOOL CRealConsole::PreInit(BOOL abCreateBuffers/*=TRUE*/)
{
	TODO("������������� ��������� �������?");
	
	_ASSERTE(mp_RBuf==mp_ABuf);
	MCHKHEAP;
	
	return mp_RBuf->PreInit();
}

BOOL CRealConsole::StartMonitorThread()
{
	BOOL lbRc = FALSE;
	_ASSERTE(mh_MonitorThread==NULL);
	//_ASSERTE(mh_InputThread==NULL);
	//_ASSERTE(mb_Detached || mh_ConEmuC!=NULL); -- ������� ������ ��������� � MonitorThread
	SetConStatus(L"Initializing ConEmu...");
	mh_MonitorThread = CreateThread(NULL, 0, MonitorThread, (LPVOID)this, 0, &mn_MonitorThreadID);
	SetConStatus(L"Initializing ConEmu....");

	//mh_InputThread = CreateThread(NULL, 0, InputThread, (LPVOID)this, 0, &mn_InputThreadID);

	if (mh_MonitorThread == NULL /*|| mh_InputThread == NULL*/)
	{
		DisplayLastError(_T("Can't create console thread!"));
	}
	else
	{
		//lbRc = SetThreadPriority(mh_MonitorThread, THREAD_PRIORITY_ABOVE_NORMAL);
		lbRc = TRUE;
	}

	return lbRc;
}

BOOL CRealConsole::StartProcess()
{
	if (!this)
	{
		_ASSERTE(this);
		return FALSE;
	}

	BOOL lbRc = FALSE;
	SetConStatus(L"Preparing process startup line...");

	// ��� ��������� ��������
	CVirtualConsole* pVCon = mp_VCon;

	if (!mb_ProcessRestarted)
	{
		if (!PreInit())
			return FALSE;
	}

	mb_UseOnlyPipeInput = FALSE;

	if (mp_sei)
	{
		SafeCloseHandle(mp_sei->hProcess);
		GlobalFree(mp_sei); mp_sei = NULL;
	}

	//ResetEvent(mh_CreateRootEvent);
	mb_InCreateRoot = TRUE;
	mb_InCloseConsole = FALSE;
	ZeroStruct(m_ServerClosing);
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	wchar_t szInitConTitle[255];
	MCHKHEAP;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESHOWWINDOW|STARTF_USECOUNTCHARS|STARTF_USESIZE/*|STARTF_USEPOSITION*/;
	si.lpTitle = wcscpy(szInitConTitle, CEC_INITTITLE);
	// � ���������, ����� ������ ������ ������ ������ � ��������.
	si.dwXCountChars = mp_RBuf->GetBufferWidth() /*con.m_sbi.dwSize.X*/;
	si.dwYCountChars = mp_RBuf->GetBufferHeight() /*con.m_sbi.dwSize.Y*/;

	// ������ ���� ����� �������� � ��������, � �� ������� �� ����� ������� ����� �����...
	// �� ����� ������ ���� ���-��, ����� ������ ����� �� ����������� (� ������� �� ����� 4*6)...
	if (mp_RBuf->isScroll() /*con.bBufferHeight*/)
	{
		si.dwXSize = 4 * mp_RBuf->GetTextWidth()/*con.m_sbi.dwSize.X*/ + 2*GetSystemMetrics(SM_CXFRAME) + GetSystemMetrics(SM_CXVSCROLL);
		si.dwYSize = 6 * mp_RBuf->GetTextHeight()/*con.nTextHeight*/ + 2*GetSystemMetrics(SM_CYFRAME) + GetSystemMetrics(SM_CYCAPTION);
	}
	else
	{
		si.dwXSize = 4 * mp_RBuf->GetTextWidth()/*con.m_sbi.dwSize.X*/ + 2*GetSystemMetrics(SM_CXFRAME);
		si.dwYSize = 6 * mp_RBuf->GetTextHeight()/*con.m_sbi.dwSize.X*/ + 2*GetSystemMetrics(SM_CYFRAME) + GetSystemMetrics(SM_CYCAPTION);
	}

	// ���� ������ "����������" ����� - ������� ������
	si.wShowWindow = gpSet->isConVisible ? SW_SHOWNORMAL : SW_HIDE;
	isShowConsole = gpSet->isConVisible;
	//RECT rcDC; GetWindowRect('ghWnd DC', &rcDC);
	//si.dwX = rcDC.left; si.dwY = rcDC.top;
	ZeroMemory(&pi, sizeof(pi));
	MCHKHEAP;
	int nStep = (m_Args.pszSpecialCmd!=NULL) ? 2 : 1;
	wchar_t* psCurCmd = NULL;

	while (nStep <= 2)
	{
		MCHKHEAP;
		/*if (!*gpSet->GetCmd()) {
		    gpSet->psCurCmd = lstrdup(gpSet->Buffer Height == 0 ? _T("far") : _T("cmd"));
		    nStep ++;
		}*/
		MCHKHEAP;
		LPCWSTR lpszCmd = NULL;

		if (m_Args.pszSpecialCmd)
			lpszCmd = m_Args.pszSpecialCmd;
		else
			lpszCmd = gpSet->GetCmd();
		_ASSERTE(lpszCmd && *lpszCmd);

		int nCurLen = 0;
		int nLen = _tcslen(lpszCmd);
		nLen += _tcslen(gpConEmu->ms_ConEmuExe) + 300 + MAX_PATH;
		MCHKHEAP;
		psCurCmd = (wchar_t*)malloc(nLen*sizeof(wchar_t));
		_ASSERTE(psCurCmd);
		_wcscpy_c(psCurCmd, nLen, L"\"");
		_wcscat_c(psCurCmd, nLen, gpConEmu->ms_ConEmuCExeFull);
		//lstrcat(psCurCmd, L"\\");
		//lstrcpy(psCurCmd, gpConEmu->ms_ConEmuCExeName);
		_wcscat_c(psCurCmd, nLen, L"\" ");

		if (m_Args.bRunAsAdministrator)
		{
			m_Args.bDetached = TRUE;
			_wcscat_c(psCurCmd, nLen, L" /ATTACH ");
		}

		if (gpSet->nConInMode != (DWORD)-1)
		{
			nCurLen = _tcslen(psCurCmd);
			_wsprintf(psCurCmd+nCurLen, SKIPLEN(nLen-nCurLen) L" /CINMODE=%X ", gpSet->nConInMode);
		}

		_ASSERTE(mp_RBuf==mp_ABuf);
		int nWndWidth = mp_RBuf->GetTextWidth();
		int nWndHeight = mp_RBuf->GetTextHeight();
		/*���� - GetConWindowSize(con.m_sbi, nWndWidth, nWndHeight);*/
		_ASSERTE(nWndWidth>0 && nWndHeight>0);
		
		nCurLen = _tcslen(psCurCmd);
		_wsprintf(psCurCmd+nCurLen, SKIPLEN(nLen-nCurLen)
		          L"/GID=%i /BW=%i /BH=%i /BZ=%i \"/FN=%s\" /FW=%i /FH=%i",
		          GetCurrentProcessId(), nWndWidth, nWndHeight, mn_DefaultBufferHeight,
		          gpSet->ConsoleFont.lfFaceName, gpSet->ConsoleFont.lfWidth, gpSet->ConsoleFont.lfHeight);

		/*if (gpSet->FontFile[0]) { --  ����������� ������ �� ������� �� ��������!
		    wcscat(psCurCmd, L" \"/FF=");
		    wcscat(psCurCmd, gpSet->FontFile);
		    wcscat(psCurCmd, L"\"");
		}*/
		if (m_UseLogs) _wcscat_c(psCurCmd, nLen, (m_UseLogs==3) ? L" /LOG3" : (m_UseLogs==2) ? L" /LOG2" : L" /LOG");

		if (!gpSet->isConVisible) _wcscat_c(psCurCmd, nLen, L" /HIDE");
		
		if (m_Args.eConfirmation == RConStartArgs::eConfAlways)
			_wcscat_c(psCurCmd, nLen, L" /CONFIRM");
		else if (m_Args.eConfirmation == RConStartArgs::eConfNever)
			_wcscat_c(psCurCmd, nLen, L" /NOCONFIRM");

		_wcscat_c(psCurCmd, nLen, L" /ROOT ");
		_wcscat_c(psCurCmd, nLen, lpszCmd);
		MCHKHEAP;
		DWORD dwLastError = 0;
#ifdef MSGLOGGER
		DEBUGSTRPROC(psCurCmd); DEBUGSTRPROC(_T("\n"));
#endif

		SetConEmuEnvVar(GetView());

		if (!m_Args.bRunAsAdministrator)
		{
			LockSetForegroundWindow(LSFW_LOCK);
			SetConStatus(L"Starting root process...");

			if (m_Args.pszUserName != NULL)
			{
				if (CreateProcessWithLogonW(m_Args.pszUserName, m_Args.pszDomain, m_Args.szUserPassword,
				                           LOGON_WITH_PROFILE, NULL, psCurCmd,
				                           NORMAL_PRIORITY_CLASS|CREATE_DEFAULT_ERROR_MODE|CREATE_NEW_CONSOLE
				                           , NULL, m_Args.pszStartupDir, &si, &pi))
					//if (CreateProcessAsUser(m_Args.hLogonToken, NULL, psCurCmd, NULL, NULL, FALSE,
					//	NORMAL_PRIORITY_CLASS|CREATE_DEFAULT_ERROR_MODE|CREATE_NEW_CONSOLE
					//	, NULL, m_Args.pszStartupDir, &si, &pi))
				{
					lbRc = TRUE;
					mn_ConEmuC_PID = pi.dwProcessId;
				}
				else
				{
					dwLastError = GetLastError();
				}

				SecureZeroMemory(m_Args.szUserPassword, sizeof(m_Args.szUserPassword));
			}
			else if (m_Args.bRunAsRestricted)
			{
				HANDLE hToken = NULL, hTokenRest = NULL;
				lbRc = FALSE;

				if (OpenProcessToken(GetCurrentProcess(),
				                    //TOKEN_ASSIGN_PRIMARY|TOKEN_DUPLICATE|TOKEN_QUERY|TOKEN_ADJUST_DEFAULT,
				                    TOKEN_DUPLICATE | TOKEN_ASSIGN_PRIMARY | TOKEN_QUERY,
				                    &hToken))
				{
					enum WellKnownAuthorities
					{
						NullAuthority = 0, WorldAuthority, LocalAuthority, CreatorAuthority,
						NonUniqueAuthority, NtAuthority, MandatoryLabelAuthority = 16
					};
					SID *pAdmSid = (SID*)calloc(sizeof(SID)+sizeof(DWORD)*2,1);
					pAdmSid->Revision = SID_REVISION;
					pAdmSid->SubAuthorityCount = 2;
					pAdmSid->IdentifierAuthority.Value[5] = NtAuthority;
					pAdmSid->SubAuthority[0] = SECURITY_BUILTIN_DOMAIN_RID;
					pAdmSid->SubAuthority[1] = DOMAIN_ALIAS_RID_ADMINS;
					SID_AND_ATTRIBUTES sidsToDisable[] =
					{
						{pAdmSid}
					};
					
					#ifdef __GNUC__
					HMODULE hAdvApi = GetModuleHandle(L"AdvApi32.dll");
					CreateRestrictedToken_t CreateRestrictedToken = (CreateRestrictedToken_t)GetProcAddress(hAdvApi, "CreateRestrictedToken");
					#endif

					if (CreateRestrictedToken(hToken, DISABLE_MAX_PRIVILEGE,
					                         countof(sidsToDisable), sidsToDisable,
					                         0, NULL, 0, NULL, &hTokenRest))
					{
						if (CreateProcessAsUserW(hTokenRest, NULL, psCurCmd, NULL, NULL, FALSE,
						                        NORMAL_PRIORITY_CLASS|CREATE_DEFAULT_ERROR_MODE|CREATE_NEW_CONSOLE
						                        , NULL, m_Args.pszStartupDir, &si, &pi))
						{
							lbRc = TRUE;
							mn_ConEmuC_PID = pi.dwProcessId;
						}

						CloseHandle(hTokenRest); hTokenRest = NULL;
					}
					else
					{
						dwLastError = GetLastError();
					}

					free(pAdmSid);
					CloseHandle(hToken); hToken = NULL;
				}
				else
				{
					dwLastError = GetLastError();
				}
			}
			else
			{
				lbRc = CreateProcess(NULL, psCurCmd, NULL, NULL, FALSE,
				                     NORMAL_PRIORITY_CLASS|CREATE_DEFAULT_ERROR_MODE|CREATE_NEW_CONSOLE
				                     //|CREATE_NEW_PROCESS_GROUP - ����! ��������� ����������� Ctrl-C
				                     , NULL, m_Args.pszStartupDir, &si, &pi);

				if (!lbRc)
				{
					dwLastError = GetLastError();
				}
				else
				{
					mn_ConEmuC_PID = pi.dwProcessId;
				}
			}

			DEBUGSTRPROC(L"CreateProcess finished\n");
			//if (m_Args.hLogonToken) { CloseHandle(m_Args.hLogonToken); m_Args.hLogonToken = NULL; }
			LockSetForegroundWindow(LSFW_UNLOCK);
		}
		else
		{
			LPCWSTR pszCmd = psCurCmd;
			wchar_t szExec[MAX_PATH+1];

			if (NextArg(&pszCmd, szExec) != 0)
			{
				lbRc = FALSE;
				dwLastError = -1;
			}
			else
			{
				if (mp_sei)
				{
					SafeCloseHandle(mp_sei->hProcess);
					GlobalFree(mp_sei); mp_sei = NULL;
				}

				wchar_t szCurrentDirectory[MAX_PATH+1];

				if (m_Args.pszStartupDir)
					wcscpy(szCurrentDirectory, m_Args.pszStartupDir);
				else if (!GetCurrentDirectory(MAX_PATH+1, szCurrentDirectory))
					szCurrentDirectory[0] = 0;

				int nWholeSize = sizeof(SHELLEXECUTEINFO)
				                 + sizeof(wchar_t) *
				                 (10  /* Verb */
				                  + _tcslen(szExec)+2
				                  + ((pszCmd == NULL) ? 0 : (_tcslen(pszCmd)+2))
				                  + _tcslen(szCurrentDirectory) + 2
				                 );
				mp_sei = (SHELLEXECUTEINFO*)GlobalAlloc(GPTR, nWholeSize);
				mp_sei->cbSize = sizeof(SHELLEXECUTEINFO);
				mp_sei->hwnd = ghWnd;
				//mp_sei->hwnd = /*NULL; */ ghWnd; // ������ � ��� NULL ������?
				mp_sei->fMask = SEE_MASK_NO_CONSOLE|SEE_MASK_NOCLOSEPROCESS|SEE_MASK_NOASYNC;
				mp_sei->lpVerb = (wchar_t*)(mp_sei+1);
				wcscpy((wchar_t*)mp_sei->lpVerb, L"runas");
				mp_sei->lpFile = mp_sei->lpVerb + _tcslen(mp_sei->lpVerb) + 2;
				wcscpy((wchar_t*)mp_sei->lpFile, szExec);
				mp_sei->lpParameters = mp_sei->lpFile + _tcslen(mp_sei->lpFile) + 2;

				if (pszCmd)
				{
					*(wchar_t*)mp_sei->lpParameters = L' ';
					wcscpy((wchar_t*)(mp_sei->lpParameters+1), pszCmd);
				}

				mp_sei->lpDirectory = mp_sei->lpParameters + _tcslen(mp_sei->lpParameters) + 2;

				if (szCurrentDirectory[0])
					wcscpy((wchar_t*)mp_sei->lpDirectory, szCurrentDirectory);
				else
					mp_sei->lpDirectory = NULL;

				//mp_sei->nShow = gpSet->isConVisible ? SW_SHOWNORMAL : SW_HIDE;
				mp_sei->nShow = SW_SHOWMINIMIZED;
				SetConStatus(L"Starting root process as user...");
				lbRc = gpConEmu->GuiShellExecuteEx(mp_sei, TRUE);
				// ������ ������� ������
				dwLastError = GetLastError();
			}
		}

		if (lbRc)
		{
			if (!m_Args.bRunAsAdministrator)
			{
				ProcessUpdate(&pi.dwProcessId, 1);
				AllowSetForegroundWindow(pi.dwProcessId);
			}

			apiSetForegroundWindow(ghWnd);
			DEBUGSTRPROC(L"CreateProcess OK\n");
			lbRc = TRUE;
			/*if (!AttachPID(pi.dwProcessId)) {
			    DEBUGSTRPROC(_T("AttachPID failed\n"));
				SetEvent(mh_CreateRootEvent); mb_InCreateRoot = FALSE;
			    return FALSE;
			}
			DEBUGSTRPROC(_T("AttachPID OK\n"));*/
			break; // OK, ���������
		}
		else
		{
			//Box("Cannot execute the command.");
			//DWORD dwLastError = GetLastError();
			DEBUGSTRPROC(L"CreateProcess failed\n");
			size_t nErrLen = _tcslen(psCurCmd)+100;
			TCHAR* pszErr = (TCHAR*)Alloc(nErrLen,sizeof(TCHAR));

			if (0==FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			                    NULL, dwLastError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
			                    pszErr, 1024, NULL))
			{
				_wsprintf(pszErr, SKIPLEN(nErrLen) L"Unknown system error: 0x%x", dwLastError);
			}

			nErrLen += _tcslen(pszErr);
			TCHAR* psz = (TCHAR*)Alloc(nErrLen+100,sizeof(TCHAR));
			int nButtons = MB_OK|MB_ICONEXCLAMATION|MB_SETFOREGROUND;
			_wcscpy_c(psz, nErrLen, _T("Cannot execute the command.\r\n"));
			_wcscat_c(psz, nErrLen, psCurCmd); _wcscat_c(psz, nErrLen, _T("\r\n"));
			_wcscat_c(psz, nErrLen, pszErr);

			if (m_Args.pszSpecialCmd == NULL)
			{
				if (psz[_tcslen(psz)-1]!=_T('\n')) _wcscat_c(psz, nErrLen, _T("\r\n"));

				if (!gpSet->psCurCmd && StrStrI(gpSet->GetCmd(), gpSetCls->GetDefaultCmd())==NULL)
				{
					_wcscat_c(psz, nErrLen, _T("\r\n\r\n"));
					_wcscat_c(psz, nErrLen, _T("Do You want to simply start "));
					_wcscat_c(psz, nErrLen, gpSetCls->GetDefaultCmd());
					_wcscat_c(psz, nErrLen, _T("?"));
					nButtons |= MB_YESNO;
				}
			}

			MCHKHEAP
			//Box(psz);
			int nBrc = MessageBox(NULL, psz, gpConEmu->GetDefaultTitle(), nButtons);
			Free(psz); Free(pszErr);

			if (nBrc!=IDYES)
			{
				// ??? ����� ���� ���� ��������� ��������. ������ ��� ��������� �������� ����!
				//gpConEmu->Destroy();
				//SetEvent(mh_CreateRootEvent);
				if (gpConEmu->isValid(pVCon))
				{
					mb_InCreateRoot = FALSE;
					CloseConsole();
				}
				else
				{
					_ASSERTE(gpConEmu->isValid(pVCon));
				}
				return FALSE;
			}

			// ��������� ����������� �������...
			if (m_Args.pszSpecialCmd == NULL)
			{
				_ASSERTE(gpSet->psCurCmd==NULL);
				gpSet->psCurCmd = lstrdup(gpSetCls->GetDefaultCmd());
			}

			nStep ++;
			MCHKHEAP

			if (psCurCmd) free(psCurCmd); psCurCmd = NULL;
		}
	}

	MCHKHEAP

	if (psCurCmd) free(psCurCmd); psCurCmd = NULL;

	MCHKHEAP
	//TODO: � ������ �� ���?
	SafeCloseHandle(pi.hThread); pi.hThread = NULL;
	//CloseHandle(pi.hProcess); pi.hProcess = NULL;
	mn_ConEmuC_PID = pi.dwProcessId;
	mh_ConEmuC = pi.hProcess; pi.hProcess = NULL;

	if (!m_Args.bRunAsAdministrator)
	{
		CreateLogFiles();
		//// ������� "���������" ������� //2009-05-14 ������ ������� �������������� � GUI, �� ������ �� ������� ����� ��������� ������� �������
		//swprintf_c(ms_ConEmuC_Pipe, CE_CURSORUPDATE, mn_ConEmuC_PID);
		//mh_CursorChanged = CreateEvent ( NULL, FALSE, FALSE, ms_ConEmuC_Pipe );
		// ���������������� ����� ������, �������, ��������� � �.�.
		InitNames();
		//// ��� ����� ��� ���������� ConEmuC
		//swprintf_c(ms_ConEmuC_Pipe, CESERVERPIPENAME, L".", mn_ConEmuC_PID);
		//swprintf_c(ms_ConEmuCInput_Pipe, CESERVERINPUTNAME, L".", mn_ConEmuC_PID);
		//MCHKHEAP
	}

	//SetEvent(mh_CreateRootEvent);
	mb_InCreateRoot = FALSE;
	return lbRc;
}

// ���������������� ����� ������, �������, ��������� � �.�.
// ������ ����� - �������� ������� ��� ����� �� ����!
void CRealConsole::InitNames()
{
	// ��� ����� ��� ���������� ConEmuC
	_wsprintf(ms_ConEmuC_Pipe, SKIPLEN(countof(ms_ConEmuC_Pipe)) CESERVERPIPENAME, L".", mn_ConEmuC_PID);
	_wsprintf(ms_ConEmuCInput_Pipe, SKIPLEN(countof(ms_ConEmuCInput_Pipe)) CESERVERINPUTNAME, L".", mn_ConEmuC_PID);
	// ��� ������� ������������ ������ � �������
	m_ConDataChanged.InitName(CEDATAREADYEVENT, mn_ConEmuC_PID);
	//swprintf_c(ms_ConEmuC_DataReady, CEDATAREADYEVENT, mn_ConEmuC_PID);
	MCHKHEAP;
	m_GetDataPipe.InitName(gpConEmu->GetDefaultTitle(), CESERVERREADNAME, L".", mn_ConEmuC_PID);
}

// ���� �������� ��������� - ��������������� ������ ������ �� �������� � ��������
COORD CRealConsole::ScreenToBuffer(COORD crMouse)
{
	if (!this)
		return crMouse;
	return mp_ABuf->ScreenToBuffer(crMouse);
}

bool CRealConsole::ProcessFarHyperlink(UINT messg, COORD crFrom)
{
	if (!this)
		return false;
	return mp_ABuf->ProcessFarHyperlink(messg, crFrom);
}

// x,y - �������� ����������
// ���� abForceSend==true - �� ��������� �� "�����������" �������, � �� ��������� "isPressed(VK_?BUTTON)"
void CRealConsole::OnMouse(UINT messg, WPARAM wParam, int x, int y, bool abForceSend /*= false*/)
{
#ifndef WM_MOUSEHWHEEL
#define WM_MOUSEHWHEEL                  0x020E
#endif
#ifdef _DEBUG
	wchar_t szDbg[60]; _wsprintf(szDbg, SKIPLEN(countof(szDbg)) L"RCon::MouseEvent at DC {%ix%i}\n", x,y);
	DEBUGSTRINPUT(szDbg);
#endif

	if (!this || !hConWnd)
		return;

	if (messg != WM_MOUSEMOVE)
	{
		mcr_LastMouseEventPos.X = mcr_LastMouseEventPos.Y = -1;
	}

	// ���� ������� ��������� ������� - �� ���������� ����� �����������������, ����� ����� ������� ��������
	TODO("StrictMonospace �������� ���� ������, �.�. ��������� ���� � ���������, ��������. �� � � �������� ���� ��������� ����!");
	bool bStrictMonospace = false; //!isConSelectMode(); // ��� ��������� � �� ��������� �������

	// �������� ��������� ���������� ��������
	COORD crMouse = ScreenToBuffer(mp_VCon->ClientToConsole(x,y, bStrictMonospace));
	
	if (mp_ABuf->OnMouse(messg, wParam, x, y, crMouse))
		return; // � ������� �� ����������, ������� ��������� "��� �����"

	// ���� ���� �������� ������� ������� ������� � �������
	if (gpSet->isDisableMouse)
		return;

	BOOL lbFarBufferSupported = isFarBufferSupported();

	// ���� ������� � ������ � ���������� - �� �������� ���� � �������
	// ����� ���������� ������. ���� �� ����� ���������� ������� (�������� "dir c: /s")
	// ������ ������� ������ - �� ��� �������� � ��� ����� ������ ��������� ����
	if (isBufferHeight() && !lbFarBufferSupported)
		return;

	PostMouseEvent(messg, wParam, crMouse, abForceSend);

	if (messg == WM_MOUSEMOVE)
	{
		m_LastMouseGuiPos.x = x; m_LastMouseGuiPos.y = y;
	}
}

void CRealConsole::PostMouseEvent(UINT messg, WPARAM wParam, COORD crMouse, bool abForceSend /*= false*/)
{
	// �� ����, ���� � ������� ����� ������������, ������
	// ���� �������� ����� - ����� �������� �������
	_ASSERTE(mp_ABuf==mp_RBuf);

	INPUT_RECORD r; memset(&r, 0, sizeof(r));
	r.EventType = MOUSE_EVENT;

	// Mouse Buttons
	if (messg != WM_LBUTTONUP && (messg == WM_LBUTTONDOWN || messg == WM_LBUTTONDBLCLK || (!abForceSend && isPressed(VK_LBUTTON))))
		r.Event.MouseEvent.dwButtonState |= FROM_LEFT_1ST_BUTTON_PRESSED;

	if (messg != WM_RBUTTONUP && (messg == WM_RBUTTONDOWN || messg == WM_RBUTTONDBLCLK || (!abForceSend && isPressed(VK_RBUTTON))))
		r.Event.MouseEvent.dwButtonState |= RIGHTMOST_BUTTON_PRESSED;

	if (messg != WM_MBUTTONUP && (messg == WM_MBUTTONDOWN || messg == WM_MBUTTONDBLCLK || (!abForceSend && isPressed(VK_MBUTTON))))
		r.Event.MouseEvent.dwButtonState |= FROM_LEFT_2ND_BUTTON_PRESSED;

	if (messg != WM_XBUTTONUP && (messg == WM_XBUTTONDOWN || messg == WM_XBUTTONDBLCLK))
	{
		if ((HIWORD(wParam) & 0x0001/*XBUTTON1*/))
			r.Event.MouseEvent.dwButtonState |= 0x0008/*FROM_LEFT_3ND_BUTTON_PRESSED*/;
		else if ((HIWORD(wParam) & 0x0002/*XBUTTON2*/))
			r.Event.MouseEvent.dwButtonState |= 0x0010/*FROM_LEFT_4ND_BUTTON_PRESSED*/;
	}

	mb_MouseButtonDown = (r.Event.MouseEvent.dwButtonState
	                      & (FROM_LEFT_1ST_BUTTON_PRESSED|FROM_LEFT_2ND_BUTTON_PRESSED|RIGHTMOST_BUTTON_PRESSED)) != 0;

	// Key modifiers
	if (GetKeyState(VK_CAPITAL) & 1)
		r.Event.MouseEvent.dwControlKeyState |= CAPSLOCK_ON;

	if (GetKeyState(VK_NUMLOCK) & 1)
		r.Event.MouseEvent.dwControlKeyState |= NUMLOCK_ON;

	if (GetKeyState(VK_SCROLL) & 1)
		r.Event.MouseEvent.dwControlKeyState |= SCROLLLOCK_ON;

	if (isPressed(VK_LMENU))
		r.Event.MouseEvent.dwControlKeyState |= LEFT_ALT_PRESSED;

	if (isPressed(VK_RMENU))
		r.Event.MouseEvent.dwControlKeyState |= RIGHT_ALT_PRESSED;

	if (isPressed(VK_LCONTROL))
		r.Event.MouseEvent.dwControlKeyState |= LEFT_CTRL_PRESSED;

	if (isPressed(VK_RCONTROL))
		r.Event.MouseEvent.dwControlKeyState |= RIGHT_CTRL_PRESSED;

	if (isPressed(VK_SHIFT))
		r.Event.MouseEvent.dwControlKeyState |= SHIFT_PRESSED;

	if (messg == WM_LBUTTONDBLCLK || messg == WM_RBUTTONDBLCLK || messg == WM_MBUTTONDBLCLK)
		r.Event.MouseEvent.dwEventFlags = DOUBLE_CLICK;
	else if (messg == WM_MOUSEMOVE)
		r.Event.MouseEvent.dwEventFlags = MOUSE_MOVED;
	else if (messg == WM_MOUSEWHEEL)
	{
		if (m_UseLogs>=2)
		{
			char szDbgMsg[128]; _wsprintfA(szDbgMsg, SKIPLEN(countof(szDbgMsg)) "WM_MOUSEWHEEL(wParam=0x%08X, x=%i, y=%i)", (DWORD)wParam, crMouse.X, crMouse.Y);
			LogString(szDbgMsg);
		}

		WARNING("���� ������� ����� ��������� - �������� ������� �����, � �� ������� �������");
		// ����� �� 2008 server ������ �� ��������
		r.Event.MouseEvent.dwEventFlags = MOUSE_WHEELED;
		SHORT nScroll = (SHORT)(((DWORD)wParam & 0xFFFF0000)>>16);

		if (nScroll<0) { if (nScroll>-120) nScroll=-120; }
		else { if (nScroll<120) nScroll=120; }

		if (nScroll<-120 || nScroll>120)
			nScroll = ((SHORT)(nScroll / 120)) * 120;

		r.Event.MouseEvent.dwButtonState |= ((DWORD)(WORD)nScroll) << 16;
		//r.Event.MouseEvent.dwButtonState |= /*(0xFFFF0000 & wParam)*/ (nScroll > 0) ? 0x00780000 : 0xFF880000;
	}
	else if (messg == WM_MOUSEHWHEEL)
	{
		if (m_UseLogs>=2)
		{
			char szDbgMsg[128]; _wsprintfA(szDbgMsg, SKIPLEN(countof(szDbgMsg)) "WM_MOUSEHWHEEL(wParam=0x%08X, x=%i, y=%i)", (DWORD)wParam, crMouse.X, crMouse.Y);
			LogString(szDbgMsg);
		}

		r.Event.MouseEvent.dwEventFlags = 8; //MOUSE_HWHEELED
		SHORT nScroll = (SHORT)(((DWORD)wParam & 0xFFFF0000)>>16);

		if (nScroll<0) { if (nScroll>-120) nScroll=-120; }
		else { if (nScroll<120) nScroll=120; }

		if (nScroll<-120 || nScroll>120)
			nScroll = ((SHORT)(nScroll / 120)) * 120;

		r.Event.MouseEvent.dwButtonState |= ((DWORD)(WORD)nScroll) << 16;
	}

	if (messg == WM_LBUTTONDOWN || messg == WM_RBUTTONDOWN || messg == WM_MBUTTONDOWN)
	{
		mb_BtnClicked = TRUE; mrc_BtnClickPos = crMouse;
	}

	// � Far3 �������� �������� ��� 0_0
	bool lbRBtnDrag = (r.Event.MouseEvent.dwButtonState & RIGHTMOST_BUTTON_PRESSED) == RIGHTMOST_BUTTON_PRESSED;
	bool lbNormalRBtnMode = false;
	// gpSet->isRSelFix ��������, ����� ���� fix ����� ���� ���������
	if (lbRBtnDrag && isFar(TRUE) && m_FarInfo.cbSize && gpSet->isRSelFix)
	{
		if ((m_FarInfo.FarVer.dwVerMajor > 3) || (m_FarInfo.FarVer.dwVerMajor == 3 && m_FarInfo.FarVer.dwBuild >= 2381))
		{
			if (gpSet->isRClickSendKey == 0)
			{
				// ���� �� ������ LCtrl/RCtrl/LAlt/RAlt - ������� ��� ��������� �� ����, ������ � ��� "��� ����"
				if (0 == (r.Event.MouseEvent.dwControlKeyState & (RIGHT_ALT_PRESSED|LEFT_ALT_PRESSED|RIGHT_CTRL_PRESSED|LEFT_CTRL_PRESSED)))
					lbRBtnDrag = false;
			}
			else
			{
				// ���������� �������� � ������ (������� ������/����� �����)?
				if (CoordInPanel(crMouse) && !(r.Event.MouseEvent.dwControlKeyState & (SHIFT_PRESSED|RIGHT_ALT_PRESSED|LEFT_ALT_PRESSED|RIGHT_CTRL_PRESSED|LEFT_CTRL_PRESSED)))
				{
					lbNormalRBtnMode = true;
					r.Event.MouseEvent.dwControlKeyState |= RIGHT_ALT_PRESSED|LEFT_CTRL_PRESSED;
				}
			}
		}
	}

	if (messg == WM_MOUSEMOVE /*&& mb_MouseButtonDown*/)
	{
		// Issue 172: �������� � ������ ������ �� PanelTabs
		//if (mcr_LastMouseEventPos.X == crMouse.X && mcr_LastMouseEventPos.Y == crMouse.Y)
		//	return; // �� �������� � ������� MouseMove �� ��� �� �����
		//mcr_LastMouseEventPos.X = crMouse.X; mcr_LastMouseEventPos.Y = crMouse.Y;
		//// ��������� ����� �� ��������, ����� AltIns �������� �������� �� ��������� �������
		//int nDeltaX = (m_LastMouseGuiPos.x > x) ? (m_LastMouseGuiPos.x - x) : (x - m_LastMouseGuiPos.x);
		//int nDeltaY = (m_LastMouseGuiPos.y > y) ? (m_LastMouseGuiPos.y - y) : (y - m_LastMouseGuiPos.y);
		// ������ - ��������� �� ����������� �������, � �� ������.
		// ����� ����������, AltIns �� ������, �.�. ����� "���� �������" (����/��������) ����� �������������
		int nDeltaX = m_LastMouse.dwMousePosition.X - crMouse.X;
		int nDeltaY = m_LastMouse.dwMousePosition.Y - crMouse.Y;

		// ��������� ��������� m_LastMouse ������������ � PostConsoleEvent
		if (m_LastMouse.dwEventFlags == MOUSE_MOVED // ������ ���� ��������� - ��� ������ �� ����
				&& m_LastMouse.dwButtonState     == r.Event.MouseEvent.dwButtonState
		        && m_LastMouse.dwControlKeyState == r.Event.MouseEvent.dwControlKeyState
		        //&& (nDeltaX <= 1 && nDeltaY <= 1) // ��� 1 ������
		        && !nDeltaX && !nDeltaY // ���� 1 ������
		        && !abForceSend // � ���� �� ������� ����� �������
		        )
			return; // �� �������� � ������� MouseMove �� ��� �� �����

		if (mb_BtnClicked)
		{
			// ���� ����� LBtnDown � ��� �� ������� �� ��� ������ MOUSE_MOVE - ������� � mrc_BtnClickPos
			if (mb_MouseButtonDown && (mrc_BtnClickPos.X != crMouse.X || mrc_BtnClickPos.Y != crMouse.Y))
			{
				r.Event.MouseEvent.dwMousePosition = mrc_BtnClickPos;
				PostConsoleEvent(&r);
			}

			mb_BtnClicked = FALSE;
		}

		//m_LastMouseGuiPos.x = x; m_LastMouseGuiPos.y = y;
		mcr_LastMouseEventPos.X = crMouse.X; mcr_LastMouseEventPos.Y = crMouse.Y;
	}

	// ��� ������� ����� ������ ������� ����� ��������� � ������ ���������� �����������. �������� ���.
	if (gpSet->isRSelFix)
	{
		// ����� ����� ������ ���� � GUI ������ ������������ �������� �����
		if (mp_ABuf != mp_RBuf)
		{
			mp_RBuf->SetRBtnDrag(FALSE);
		}
		else
		{
			//BOOL lbRBtnDrag = (r.Event.MouseEvent.dwButtonState & RIGHTMOST_BUTTON_PRESSED) == RIGHTMOST_BUTTON_PRESSED;
			COORD con_crRBtnDrag = {};
			BOOL con_bRBtnDrag = mp_RBuf->GetRBtnDrag(&con_crRBtnDrag);

			if (con_bRBtnDrag && !lbRBtnDrag)
			{
				con_bRBtnDrag = FALSE;
				mp_RBuf->SetRBtnDrag(FALSE);
			}
			else if (con_bRBtnDrag)
			{
				#ifdef _DEBUG
				SHORT nXDelta = crMouse.X - con_crRBtnDrag.X;
				#endif
				SHORT nYDelta = crMouse.Y - con_crRBtnDrag.Y;

				if (nYDelta < -1 || nYDelta > 1)
				{
					// ���� ����� ����������� ����� ������ ����� 1 ������
					SHORT nYstep = (nYDelta < -1) ? -1 : 1;
					SHORT nYend = crMouse.Y; // - nYstep;
					crMouse.Y = con_crRBtnDrag.Y + nYstep;

					// �������� ����������� ������
					while (crMouse.Y != nYend)
					{
						#ifdef _DEBUG
						wchar_t szDbg[60]; _wsprintf(szDbg, SKIPLEN(countof(szDbg)) L"+++ Add right button drag: {%ix%i}\n", crMouse.X, crMouse.Y);
						DEBUGSTRINPUT(szDbg);
						#endif
						
						r.Event.MouseEvent.dwMousePosition = crMouse;
						PostConsoleEvent(&r);
						crMouse.Y += nYstep;
					}
				}
			}

			if (lbRBtnDrag)
			{
				mp_RBuf->SetRBtnDrag(TRUE, &crMouse);
			}
		}
	}

	r.Event.MouseEvent.dwMousePosition = crMouse;

	if (mn_FarPID && mn_FarPID != mn_LastSetForegroundPID)
	{
		AllowSetForegroundWindow(mn_FarPID);
		mn_LastSetForegroundPID = mn_FarPID;
	}

	// �������� ������� � ������� ����� ConEmuC
	PostConsoleEvent(&r);
}

void CRealConsole::StartSelection(BOOL abTextMode, SHORT anX/*=-1*/, SHORT anY/*=-1*/, BOOL abByMouse/*=FALSE*/)
{
	mp_ABuf->StartSelection(abTextMode, anX, anY, abByMouse);
}

void CRealConsole::ExpandSelection(SHORT anX/*=-1*/, SHORT anY/*=-1*/)
{
	mp_ABuf->ExpandSelection(anX, anY);
}

void CRealConsole::DoSelectionStop()
{
	mp_ABuf->DoSelectionStop();
}

bool CRealConsole::DoSelectionCopy()
{
	return mp_ABuf->DoSelectionCopy();
}

BOOL CRealConsole::OpenConsoleEventPipe()
{
	if (mh_ConEmuCInput && mh_ConEmuCInput!=INVALID_HANDLE_VALUE)
	{
		CloseHandle(mh_ConEmuCInput); mh_ConEmuCInput = NULL;
	}

	TODO("���� ���� � ����� ������ �� �������� � ������� 10 ������ (������?) - ������� VirtualConsole ������� ������");
	// Try to open a named pipe; wait for it, if necessary.
	int nSteps = 10;
	BOOL fSuccess;
	DWORD dwErr = 0, dwWait = 0;

	while((nSteps--) > 0)
	{
		mh_ConEmuCInput = CreateFile(
		                      ms_ConEmuCInput_Pipe,// pipe name
		                      GENERIC_WRITE,
		                      0,              // no sharing
		                      NULL,           // default security attributes
		                      OPEN_EXISTING,  // opens existing pipe
		                      0,              // default attributes
		                      NULL);          // no template file

		// Break if the pipe handle is valid.
		if (mh_ConEmuCInput != INVALID_HANDLE_VALUE)
		{
			// The pipe connected; change to message-read mode.
			DWORD dwMode = PIPE_READMODE_MESSAGE;
			fSuccess = SetNamedPipeHandleState(
			               mh_ConEmuCInput,    // pipe handle
			               &dwMode,  // new pipe mode
			               NULL,     // don't set maximum bytes
			               NULL);    // don't set maximum time

			if (!fSuccess)
			{
				DEBUGSTRINPUT(L" - FAILED!\n");
				dwErr = GetLastError();
				SafeCloseHandle(mh_ConEmuCInput);

				//if (!IsDebuggerPresent())
				if (!isConsoleClosing())
					DisplayLastError(L"SetNamedPipeHandleState failed", dwErr);

				return FALSE;
			}

			return TRUE;
		}

		// Exit if an error other than ERROR_PIPE_BUSY occurs.
		dwErr = GetLastError();

		if (dwErr != ERROR_PIPE_BUSY)
		{
			TODO("���������, ���� �������� ���� � ����� ������, �� ������ ���� ��� mh_ConEmuC");
			dwWait = WaitForSingleObject(mh_ConEmuC, 100);

			if (dwWait == WAIT_OBJECT_0)
			{
				DEBUGSTRINPUT(L"ConEmuC was closed. OpenPipe FAILED!\n");
				return FALSE;
			}

			if (!isConsoleClosing())
				break;

			continue;
			//DisplayLastError(L"Could not open pipe", dwErr);
			//return 0;
		}

		// All pipe instances are busy, so wait for 0.1 second.
		if (!WaitNamedPipe(ms_ConEmuCInput_Pipe, 100))
		{
			dwErr = GetLastError();
			dwWait = WaitForSingleObject(mh_ConEmuC, 100);

			if (dwWait == WAIT_OBJECT_0)
			{
				DEBUGSTRINPUT(L"ConEmuC was closed. OpenPipe FAILED!\n");
				return FALSE;
			}

			if (!isConsoleClosing())
				DisplayLastError(L"WaitNamedPipe failed", dwErr);

			return FALSE;
		}
	}

	if (mh_ConEmuCInput == NULL || mh_ConEmuCInput == INVALID_HANDLE_VALUE)
	{
		// �� ��������� ��������� �����. ��������, ConEmuC ��� �� ����������
		//DEBUGSTRINPUT(L" - mh_ConEmuCInput not found!\n");
#ifdef _DEBUG
		DWORD dwTick1 = GetTickCount();
		struct ServerClosing sc1 = m_ServerClosing;
#endif

		if (!isConsoleClosing())
		{
#ifdef _DEBUG
			DWORD dwTick2 = GetTickCount();
			struct ServerClosing sc2 = m_ServerClosing;

			if (dwErr == WAIT_TIMEOUT)
			{
				TODO("������ ��������. ��������� m_ServerClosing.nServerPID. ����� ��� ���������� ��� ������ �� ��������?");
				MyAssertTrap();
			}

			DWORD dwTick3 = GetTickCount();
			struct ServerClosing sc3 = m_ServerClosing;
#endif
			int nLen = _tcslen(ms_ConEmuCInput_Pipe) + 128;
			wchar_t* pszErrMsg = (wchar_t*)malloc(nLen*sizeof(wchar_t));
			_wsprintf(pszErrMsg, SKIPLEN(nLen) L"ConEmuCInput not found, ErrCode=0x%08X\n%s", dwErr, ms_ConEmuCInput_Pipe);
			//DisplayLastError(L"mh_ConEmuCInput not found", dwErr);
			// ��������� Post-��, �.�. ������� ����� ��� ����������� (�� ����� ���������)? � ������ ��� �� ��������...
			gpConEmu->PostDisplayRConError(this, pszErrMsg);
		}

		return FALSE;
	}

	return FALSE;
}

void CRealConsole::PostConsoleEventPipe(MSG64 *pMsg)
{
	DWORD dwErr = 0; //, dwMode = 0;
	BOOL fSuccess = FALSE;
#ifdef _DEBUG

	if (gbInSendConEvent)
	{
		_ASSERTE(!gbInSendConEvent);
	}

#endif
	// ���� ����. ��������, ��� ConEmuC ���
	DWORD dwExitCode = 0;
	fSuccess = GetExitCodeProcess(mh_ConEmuC, &dwExitCode);

	if (dwExitCode!=STILL_ACTIVE)
	{
		//DisplayLastError(L"ConEmuC was terminated");
		return;
	}

	TODO("���� ���� � ����� ������ �� �������� � ������� 10 ������ (������?) - ������� VirtualConsole ������� ������");

	if (mh_ConEmuCInput==NULL || mh_ConEmuCInput==INVALID_HANDLE_VALUE)
	{
		// Try to open a named pipe; wait for it, if necessary.
		if (!OpenConsoleEventPipe())
			return;

		//int nSteps = 10;
		//while ((nSteps--) > 0)
		//{
		//  mh_ConEmuCInput = CreateFile(
		//     ms_ConEmuCInput_Pipe,// pipe name
		//     GENERIC_WRITE,
		//     0,              // no sharing
		//     NULL,           // default security attributes
		//     OPEN_EXISTING,  // opens existing pipe
		//     0,              // default attributes
		//     NULL);          // no template file
		//
		//  // Break if the pipe handle is valid.
		//  if (mh_ConEmuCInput != INVALID_HANDLE_VALUE)
		//     break;
		//
		//  // Exit if an error other than ERROR_PIPE_BUSY occurs.
		//  dwErr = GetLastError();
		//  if (dwErr != ERROR_PIPE_BUSY)
		//  {
		//    TODO("���������, ���� �������� ���� � ����� ������, �� ������ ���� ��� mh_ConEmuC");
		//    dwErr = WaitForSingleObject(mh_ConEmuC, 100);
		//    if (dwErr == WAIT_OBJECT_0) {
		//        return;
		//    }
		//    continue;
		//    //DisplayLastError(L"Could not open pipe", dwErr);
		//    //return 0;
		//  }
		//
		//  // All pipe instances are busy, so wait for 0.1 second.
		//  if (!WaitNamedPipe(ms_ConEmuCInput_Pipe, 100) )
		//  {
		//    dwErr = WaitForSingleObject(mh_ConEmuC, 100);
		//    if (dwErr == WAIT_OBJECT_0) {
		//        DEBUGSTRINPUT(L" - FAILED!\n");
		//        return;
		//    }
		//    //DisplayLastError(L"WaitNamedPipe failed");
		//    //return 0;
		//  }
		//}
		//if (mh_ConEmuCInput == NULL || mh_ConEmuCInput == INVALID_HANDLE_VALUE) {
		//    // �� ��������� ��������� �����. ��������, ConEmuC ��� �� ����������
		//    DEBUGSTRINPUT(L" - mh_ConEmuCInput not found!\n");
		//    return;
		//}
		//
		//// The pipe connected; change to message-read mode.
		//dwMode = PIPE_READMODE_MESSAGE;
		//fSuccess = SetNamedPipeHandleState(
		//  mh_ConEmuCInput,    // pipe handle
		//  &dwMode,  // new pipe mode
		//  NULL,     // don't set maximum bytes
		//  NULL);    // don't set maximum time
		//if (!fSuccess)
		//{
		//  DEBUGSTRINPUT(L" - FAILED!\n");
		//  DWORD dwErr = GetLastError();
		//  SafeCloseHandle(mh_ConEmuCInput);
		//  if (!IsDebuggerPresent())
		//    DisplayLastError(L"SetNamedPipeHandleState failed", dwErr);
		//  return;
		//}
	}

	//// ���� ����. ��������, ��� ConEmuC ���
	//dwExitCode = 0;
	//fSuccess = GetExitCodeProcess(mh_ConEmuC, &dwExitCode);
	//if (dwExitCode!=STILL_ACTIVE) {
	//    //DisplayLastError(L"ConEmuC was terminated");
	//    return;
	//}
#ifdef _DEBUG

	switch(pMsg->message)
	{
		case WM_KEYDOWN: case WM_SYSKEYDOWN:
			DEBUGSTRINPUTPIPE(L"ConEmu: Sending key down\n"); break;
		case WM_KEYUP: case WM_SYSKEYUP:
			DEBUGSTRINPUTPIPE(L"ConEmu: Sending key up\n"); break;
		default:
			DEBUGSTRINPUTPIPE(L"ConEmu: Sending input\n");
	}

#endif
	gbInSendConEvent = TRUE;
	DWORD dwSize = sizeof(MSG64), dwWritten;
	_ASSERTE(pMsg->cbSize==dwSize);
	fSuccess = WriteFile(mh_ConEmuCInput, pMsg, dwSize, &dwWritten, NULL);

	if (!fSuccess)
	{
		dwErr = GetLastError();

		if (!isConsoleClosing())
		{
			if (dwErr == 0x000000E8/*The pipe is being closed.*/)
			{
				fSuccess = GetExitCodeProcess(mh_ConEmuC, &dwExitCode);

				if (fSuccess && dwExitCode!=STILL_ACTIVE)
					goto wrap;

				if (OpenConsoleEventPipe())
				{
					fSuccess = WriteFile(mh_ConEmuCInput, pMsg, dwSize, &dwWritten, NULL);

					if (fSuccess)
						goto wrap; // ���� ��
				}
			}

#ifdef _DEBUG
			//DisplayLastError(L"Can't send console event (pipe)", dwErr);
			wchar_t szDbg[128];
			_wsprintf(szDbg, SKIPLEN(countof(szDbg)) L"Can't send console event (pipe)", dwErr);
			gpConEmu->DebugStep(szDbg);
#endif
		}

		goto wrap;
	}

wrap:
	gbInSendConEvent = FALSE;
}

LRESULT CRealConsole::PostConsoleMessage(HWND hWnd, UINT nMsg, WPARAM wParam, LPARAM lParam)
{
	LRESULT lRc = 0;
	bool bNeedCmd = isAdministrator() || (m_Args.pszUserName != NULL);

	if (nMsg == WM_INPUTLANGCHANGE || nMsg == WM_INPUTLANGCHANGEREQUEST)
		bNeedCmd = true;

#ifdef _DEBUG

	if (nMsg == WM_INPUTLANGCHANGE || nMsg == WM_INPUTLANGCHANGEREQUEST)
	{
		wchar_t szDbg[255];
		const wchar_t* pszMsgID = (nMsg == WM_INPUTLANGCHANGE) ? L"WM_INPUTLANGCHANGE" : L"WM_INPUTLANGCHANGEREQUEST";
		const wchar_t* pszVia = bNeedCmd ? L"CmdExecute" : L"PostThreadMessage";
		_wsprintf(szDbg, SKIPLEN(countof(szDbg)) L"RealConsole: %s, CP:%i, HKL:0x%08I64X via %s\n",
		          pszMsgID, (DWORD)wParam, (unsigned __int64)(DWORD_PTR)lParam, pszVia);
		DEBUGSTRLANG(szDbg);
	}

#endif

	if (!bNeedCmd)
	{
		POSTMESSAGE(hWnd/*hConWnd*/, nMsg, wParam, lParam, FALSE);
	}
	else
	{
		CESERVER_REQ in;
		ExecutePrepareCmd(&in, CECMD_POSTCONMSG, sizeof(CESERVER_REQ_HDR) + sizeof(CESERVER_REQ_POSTMSG));
		// ����������, ���������
		in.Msg.bPost = TRUE;
		in.Msg.hWnd = hWnd;
		in.Msg.nMsg = nMsg;
		in.Msg.wParam = wParam;
		in.Msg.lParam = lParam;
		
		DWORD dwTickStart = timeGetTime();
		
		CESERVER_REQ *pOut = ExecuteSrvCmd(GetServerPID(), &in, ghWnd);

		gpSetCls->debugLogCommand(&in, FALSE, dwTickStart, timeGetTime()-dwTickStart, L"ExecuteSrvCmd", pOut);

		if (pOut) ExecuteFreeResult(pOut);
	}

	return lRc;
}

void CRealConsole::StopSignal()
{
	DEBUGSTRCON(L"CRealConsole::StopSignal()\n");

	if (!this)
		return;

	if (mn_ProcessCount)
	{
		MSectionLock SPRC; SPRC.Lock(&csPRC, TRUE);
		m_Processes.clear();
		SPRC.Unlock();
		mn_ProcessCount = 0;
	}

	SetEvent(mh_TermEvent);

	if (!mn_InRecreate)
	{
		// ����� ��� �������� �� ���� ������� ������������
		// ������ ������� ���� �������
		mn_tabsCount = 0;
		// ������� ������� �������� � ���������� �������
		gpConEmu->OnVConTerminated(mp_VCon);
	}
}

void CRealConsole::StopThread(BOOL abRecreating)
{
#ifdef _DEBUG
	/*
	    HeapValidate(mh_Heap, 0, NULL);
	*/
#endif
	_ASSERTE(abRecreating==mb_ProcessRestarted);
	DEBUGSTRPROC(L"Entering StopThread\n");

	// ����������� ������ � ���������� ������
	if (mh_MonitorThread)
	{
		// ������� ��������� ����� ��������
		StopSignal(); //SetEvent(mh_TermEvent);

		// � ������ ����� ����� ����������
		if (WaitForSingleObject(mh_MonitorThread, 300) != WAIT_OBJECT_0)
		{
			DEBUGSTRPROC(L"### Main Thread wating timeout, terminating...\n");
			TerminateThread(mh_MonitorThread, 1);
		}
		else
		{
			DEBUGSTRPROC(L"Main Thread closed normally\n");
		}

		SafeCloseHandle(mh_MonitorThread);
	}

	if (mh_PostMacroThread != NULL)
	{
		DWORD nWait = WaitForSingleObject(mh_PostMacroThread, 0);
		if (nWait == WAIT_OBJECT_0)
		{
			CloseHandle(mh_PostMacroThread);
			mh_PostMacroThread = NULL;
		}
		else
		{
			// ������ ���� NULL, ���� ��� - ������ ����� ����������� ������
			_ASSERTE(mh_PostMacroThread==NULL);
			TerminateThread(mh_PostMacroThread, 100);
			CloseHandle(mh_PostMacroThread);
		}
	}


	// ���������� ��������� ����� ���� �������
	DEBUGSTRPROC(L"About to terminate main server thread (MonitorThread)\n");

	if (ms_VConServer_Pipe[0])  // ������ ���� �� ���� ���� ���� ��������
	{
		StopSignal(); // ��� ������ ���� ���������, �� �� ������ ������
		//
		HANDLE hPipe = INVALID_HANDLE_VALUE;
		DWORD dwWait = 0;

		// ����������� �����, ����� ���� ������� �����������
		for(int i=0; i<MAX_SERVER_THREADS; i++)
		{
			DEBUGSTRPROC(L"Touching our server pipe\n");
			HANDLE hServer = mh_ActiveRConServerThread;
			hPipe = CreateFile(ms_VConServer_Pipe,GENERIC_WRITE,0,NULL,OPEN_EXISTING,0,NULL);

			if (hPipe == INVALID_HANDLE_VALUE)
			{
				DEBUGSTRPROC(L"All pipe instances closed?\n");
				break;
			}

			DEBUGSTRPROC(L"Waiting server pipe thread\n");
			dwWait = WaitForSingleObject(hServer, 200); // �������� ���������, ���� ���� ����������
			// ������ ������� ���� - ��� ����� ���� �����������
			CloseHandle(hPipe);
			hPipe = INVALID_HANDLE_VALUE;
		}

		// ������� ��������, ���� ��� ���� ����������
		DEBUGSTRPROC(L"Checking server pipe threads are closed\n");
		WaitForMultipleObjects(MAX_SERVER_THREADS, mh_RConServerThreads, TRUE, 500);

		for(int i=0; i<MAX_SERVER_THREADS; i++)
		{
			if (WaitForSingleObject(mh_RConServerThreads[i],0) != WAIT_OBJECT_0)
			{
				DEBUGSTRPROC(L"### Terminating mh_RConServerThreads\n");
				TerminateThread(mh_RConServerThreads[i],0);
			}

			CloseHandle(mh_RConServerThreads[i]);
			mh_RConServerThreads[i] = NULL;
		}

		ms_VConServer_Pipe[0] = 0;
	}

	//if (mh_InputThread) {
	//    if (WaitForSingleObject(mh_InputThread, 300) != WAIT_OBJECT_0) {
	//        DEBUGSTRPROC(L"### Input Thread wating timeout, terminating...\n");
	//        TerminateThread(mh_InputThread, 1);
	//    } else {
	//        DEBUGSTRPROC(L"Input Thread closed normally\n");
	//    }
	//    SafeCloseHandle(mh_InputThread);
	//}

	if (!abRecreating)
	{
		SafeCloseHandle(mh_TermEvent);
		SafeCloseHandle(mh_MonitorThreadEvent);
		//SafeCloseHandle(mh_PacketArrived);
	}

	if (abRecreating)
	{
		hConWnd = NULL;
		mn_ConEmuC_PID = 0;
		mn_FarPID = mn_ActivePID = mn_FarPID_PluginDetected = 0;
		mn_LastSetForegroundPID = 0;
		//mn_ConEmuC_Input_TID = 0;
		SafeCloseHandle(mh_ConEmuC);
		SafeCloseHandle(mh_ConEmuCInput);
		m_ConDataChanged.Close();
		m_GetDataPipe.Close();
		// ��� ����� ��� ���������� ConEmuC
		ms_ConEmuC_Pipe[0] = 0;
		ms_ConEmuCInput_Pipe[0] = 0;
		// ������� ��� ��������
		CloseMapHeader();
		CloseColorMapping();
		gpConEmu->Invalidate(mp_VCon);
	}

#ifdef _DEBUG
	/*
	    HeapValidate(mh_Heap, 0, NULL);
	*/
#endif
	DEBUGSTRPROC(L"Leaving StopThread\n");
}


void CRealConsole::Box(LPCTSTR szText)
{
#ifdef _DEBUG
	_ASSERTE(FALSE);
#endif
	MessageBox(NULL, szText, gpConEmu->GetDefaultTitle(), MB_ICONSTOP);
}

BOOL CRealConsole::isGuiVisible()
{
	if (!this)
		return FALSE;

	if (hGuiWnd)
	{
		//return !::IsWindowVisible(hGuiWnd);
		// IsWindowVisible �� ��������, �.�. ��������� ��������� � mh_WndDC
		DWORD_PTR nStyle = GetWindowLongPtr(hGuiWnd, GWL_STYLE);
		return (nStyle & WS_VISIBLE) != 0;
	}
	return FALSE;
}

BOOL CRealConsole::isGuiOverCon()
{
	if (!this)
		return FALSE;

	if (hGuiWnd && !mb_GuiExternMode)
	{
		return isGuiVisible();
	}

	return FALSE;
}

// ���������, ������� �� ����� (TRUE). ��� ������ ���� ����� ������ ������ (FALSE).
BOOL CRealConsole::isBufferHeight()
{
	if (!this)
		return FALSE;

	if (hGuiWnd)
	{
		return !isGuiVisible();
	}

	return mp_ABuf->isScroll();
}

BOOL CRealConsole::isConSelectMode()
{
	if (!this || !mp_ABuf) return false;
	
	// ����� ����� ���.������ - � ��� ����� ���������, ��� ������ �������� "����������", ����� ��� � ���.��������� �� ������.
	#ifdef _DEBUG
	if (mp_ABuf != mp_RBuf)
	{
		CONSOLE_CURSOR_INFO ci;
		mp_ABuf->GetCursorInfo(NULL, &ci);
		_ASSERTE(ci.dwSize < 40);
	}
	#endif

	return mp_ABuf->isConSelectMode();
}

BOOL CRealConsole::isDetached()
{
	if (this == NULL) return FALSE;

	if (!m_Args.bDetached) return FALSE;

	// ������ ������ �� ���������� - ������������� �� ������
	//_ASSERTE(!mb_Detached || (mb_Detached && (hConWnd==NULL)));
	return (mh_ConEmuC == NULL);
}

BOOL CRealConsole::isFar(BOOL abPluginRequired/*=FALSE*/)
{
	if (!this) return false;

	return GetFarPID(abPluginRequired)!=0;
}

BOOL CRealConsole::isWindowVisible()
{
	if (!this) return FALSE;

	if (!hConWnd) return FALSE;

	return IsWindowVisible(hConWnd);
}

LPCTSTR CRealConsole::GetTitle()
{
	// �� ������ mn_ProcessCount==0, � ������ � ������� ���������� ��� �����
	if (!this /*|| !mn_ProcessCount*/)
		return NULL;
		
	if (isAdministrator() && gpSet->szAdminTitleSuffix[0])
	{
		if (TitleAdmin[0] == 0)
		{
			TitleAdmin[countof(TitleAdmin)-1] = 0;
			wcscpy_c(TitleAdmin, TitleFull);
			wcscat_c(TitleAdmin, gpSet->szAdminTitleSuffix);
		}
		return TitleAdmin;
	}

	return TitleFull;
}

LRESULT CRealConsole::OnScroll(int nDirection)
{
	if (!this) return 0;

	return mp_ABuf->OnScroll(nDirection);
}

LRESULT CRealConsole::OnSetScrollPos(WPARAM wParam)
{
	if (!this) return 0;

	return mp_ABuf->OnSetScrollPos(wParam);
}

void CRealConsole::OnKeyboard(HWND hWnd, UINT messg, WPARAM wParam, LPARAM lParam, const wchar_t *pszChars)
{
	if (!this) return;

	//LRESULT result = 0;
	_ASSERTE(pszChars!=NULL);

	#ifdef _DEBUG
	if (wParam != VK_LCONTROL && wParam != VK_RCONTROL && wParam != VK_CONTROL &&
	        wParam != VK_LSHIFT && wParam != VK_RSHIFT && wParam != VK_SHIFT &&
	        wParam != VK_LMENU && wParam != VK_RMENU && wParam != VK_MENU &&
	        wParam != VK_LWIN && wParam != VK_RWIN)
	{
		wParam = wParam;
	}

	if (wParam == VK_CONTROL || wParam == VK_LCONTROL || wParam == VK_RCONTROL || wParam == 'C')
	{
		if (messg == WM_KEYDOWN || messg == WM_KEYUP /*|| messg == WM_CHAR*/)
		{
			wchar_t szDbg[128];

			if (messg == WM_KEYDOWN)
				_wsprintf(szDbg, SKIPLEN(countof(szDbg)) L"WM_KEYDOWN(%i,0x%08X)\n", (DWORD)wParam, (DWORD)lParam);
			else //if (messg == WM_KEYUP)
				_wsprintf(szDbg, SKIPLEN(countof(szDbg)) L"WM_KEYUP(%i,0x%08X)\n", (DWORD)wParam, (DWORD)lParam);

			DEBUGSTRINPUT(szDbg);
		}
	}
	#endif

	// ��������, ����� ������� ���������� ��� �����?
	if (mp_ABuf->OnKeyboard(hWnd, messg, wParam, lParam, pszChars))
		return;


	//// ��������� Left/Right/Up/Down ��� ���������
	//if (messg == WM_KEYDOWN
	//        && ((wParam == VK_ESCAPE) || (wParam == VK_RETURN)
	//            || (wParam == VK_LEFT) || (wParam == VK_RIGHT) || (wParam == VK_UP) || (wParam == VK_DOWN))
	//   )
	//{
	//	if (mp_ABuf->OnKeyboard(hWnd, messg, wParam, lParam, pszChars))
	//		return;
	//}
	
	WARNING("��� ���-��� ��������. ��������� ������ ����� ������������ ������.");
	// ��������, AltEnter ����� ���������� � �������, � ����� � "������ FullScreen" (� ��������� ������ ��� �������� ����� ����������)

	if (mp_ABuf->isSelfSelectMode())
	{
		return; // � ������ ��������� - � ������� ������ ���������� �� ��������!
	}

	// �������� ���������
	{
		if (wParam == VK_MENU && (messg == WM_KEYUP || messg == WM_SYSKEYUP) && gpSet->isFixAltOnAltTab)
		{
			// ��� ������� ������� Alt-Tab (������������ � ������ ����)
			// � ������� ������������� {press Alt/release Alt}
			// � ����������, ����� ����������� ������, ���������� �� Alt.
			if (GetForegroundWindow()!=ghWnd && GetFarPID())
			{
				if (/*isPressed(VK_MENU) &&*/ !isPressed(VK_CONTROL) && !isPressed(VK_SHIFT))
				{
					PostKeyPress(VK_CONTROL, LEFT_ALT_PRESSED, 0);
					//TODO("������� � ��������� ������� ���� SendKeyPress(VK_TAB,0x22,'\x09')");
					//INPUT_RECORD r = {KEY_EVENT};
					//
					////WARNING("�� ��������, ����� �� TAB ��������, � �� ���� ������ QuickSearch - �� ��������� � ��� ���������� �� ������ ������");
					//
					//r.Event.KeyEvent.bKeyDown = TRUE;
					//r.Event.KeyEvent.wVirtualKeyCode = VK_TAB;
					//r.Event.KeyEvent.wVirtualScanCode = MapVirtualKey(VK_TAB, 0/*MAPVK_VK_TO_VSC*/);
					//r.Event.KeyEvent.dwControlKeyState = 0x22;
					//r.Event.KeyEvent.uChar.UnicodeChar = 9;
					//PostConsoleEvent(&r);
					//
					////On Keyboard(hConWnd, WM_KEYUP, VK_TAB, 0);
					//r.Event.KeyEvent.bKeyDown = FALSE;
					//r.Event.KeyEvent.dwControlKeyState = 0x22; // ���� 0x20
					//PostConsoleEvent(&r);
				}
			}
		}

		//if (messg == WM_SYSKEYDOWN)
		//    if (wParam == VK_INSERT && lParam & (1<<29)/*����. ��� 29-� ���, � �� ����� 29*/)
		//        mb_ConsoleSelectMode = true;
		static bool isSkipNextAltUp = false;

		if (messg == WM_SYSKEYDOWN && wParam == VK_RETURN && lParam & (1<<29)
		        && !isPressed(VK_SHIFT))
		{
			if (gpSet->isSendAltEnter)
			{
				INPUT_RECORD r = {KEY_EVENT};
				//On Keyboard(hConWnd, WM_KEYDOWN, VK_MENU, 0); -- Alt ����� �� ����� - �� ��� ������
				WARNING("� ���� �� ��� ��������������?");
				//On Keyboard(hConWnd, WM_KEYDOWN, VK_RETURN, 0);
				r.Event.KeyEvent.bKeyDown = TRUE;
				r.Event.KeyEvent.wRepeatCount = 1;
				r.Event.KeyEvent.wVirtualKeyCode = VK_RETURN;
				r.Event.KeyEvent.wVirtualScanCode = /*28 �� ���� ����������*/MapVirtualKey(VK_RETURN, 0/*MAPVK_VK_TO_VSC*/);
				r.Event.KeyEvent.dwControlKeyState = NUMLOCK_ON|LEFT_ALT_PRESSED /*0x22*/;
				r.Event.KeyEvent.uChar.UnicodeChar = pszChars[0];
				PostConsoleEvent(&r);
				//On Keyboard(hConWnd, WM_KEYUP, VK_RETURN, 0);
				r.Event.KeyEvent.bKeyDown = FALSE;
				r.Event.KeyEvent.dwControlKeyState = NUMLOCK_ON;
				PostConsoleEvent(&r);
				//On Keyboard(hConWnd, WM_KEYUP, VK_MENU, 0); -- Alt ����� �� ����� - �� ����� ������ ��� �����
			}
			else
			{
				//if (isPressed(VK_SHIFT))
				//    return;
				gpConEmu->OnAltEnter();
				isSkipNextAltUp = true;
			}
		}
		//AltSpace - �������� ��������� ����
		else if (!gpSet->isSendAltSpace && (messg == WM_SYSKEYDOWN || messg == WM_SYSKEYUP)
		        && wParam == VK_SPACE && lParam & (1<<29) && !isPressed(VK_SHIFT))
		{
			// ����, ��� ��������� ���� ����� ����������
			if (messg == WM_SYSKEYUP)  // ������ �� UP, ����� �� "��������"
			{
				gpConEmu->ShowSysmenu();
			}
		}
		else if (messg == WM_KEYUP && wParam == VK_MENU && isSkipNextAltUp) isSkipNextAltUp = false;
		else if (messg == WM_SYSKEYDOWN && wParam == VK_F9 && lParam & (1<<29)
		        && !isPressed(VK_SHIFT))
		{
			// AltF9
			// ����� � ������� �� ������� ����� (FAR ����� ��������� ������ �� Alt)
			if (gpSet->isFixAltOnAltTab)
				PostKeyPress(VK_CONTROL, LEFT_ALT_PRESSED, 0);

			//INPUT_RECORD r = {KEY_EVENT};
			//r.Event.KeyEvent.bKeyDown = TRUE;
			//r.Event.KeyEvent.wVirtualKeyCode = VK_CONTROL;
			//r.Event.KeyEvent.wVirtualScanCode = MapVirtualKey(VK_CONTROL, 0/*MAPVK_VK_TO_VSC*/);
			//r.Event.KeyEvent.dwControlKeyState = 0x2A;
			//r.Event.KeyEvent.wRepeatCount = 1;
			//PostConsoleEvent(&r);
			//r.Event.KeyEvent.bKeyDown = FALSE;
			//r.Event.KeyEvent.dwControlKeyState = 0x22;
			//PostConsoleEvent(&r);
			//gpConEmu->SetWindowMode((gpConEmu->isZoomed()||(gpConEmu->mb_isFullScreen&&gpConEmu->isWndNotFSMaximized)) ? rNormal : rMaximized);
			gpConEmu->OnAltF9(TRUE);
		}
		else
		{
			if (gpConEmu->isInImeComposition())
			{
				// ������ ���� �������� �� ���� IME � �� ������ �������� � �������!
				return;
			}

			INPUT_RECORD r = {KEY_EVENT};
			WORD nCaps = 1 & (WORD)GetKeyState(VK_CAPITAL);
			WORD nNum = 1 & (WORD)GetKeyState(VK_NUMLOCK);
			WORD nScroll = 1 & (WORD)GetKeyState(VK_SCROLL);
			WORD nLAlt = 0x8000 & (WORD)GetKeyState(VK_LMENU);
			WORD nRAlt = 0x8000 & (WORD)GetKeyState(VK_RMENU);
			WORD nLCtrl = 0x8000 & (WORD)GetKeyState(VK_LCONTROL);
			WORD nRCtrl = 0x8000 & (WORD)GetKeyState(VK_RCONTROL);
			WORD nShift = 0x8000 & (WORD)GetKeyState(VK_SHIFT);
			//if (messg == WM_CHAR || messg == WM_SYSCHAR) {
			//    if (((WCHAR)wParam) <= 32 || mn_LastVKeyPressed == 0)
			//        return; // ��� ��� ����������
			//    r.Event.KeyEvent.bKeyDown = TRUE;
			//    r.Event.KeyEvent.uChar.UnicodeChar = (WCHAR)wParam;
			//    r.Event.KeyEvent.wRepeatCount = 1; TODO("0-15 ? Specifies the repeat count for the current message. The value is the number of times the keystroke is autorepeated as a result of the user holding down the key. If the keystroke is held long enough, multiple messages are sent. However, the repeat count is not cumulative.");
			//    r.Event.KeyEvent.wVirtualKeyCode = mn_LastVKeyPressed;
			//} else {
			mn_LastVKeyPressed = wParam & 0xFFFF;
			////PostConsoleMessage(hConWnd, messg, wParam, lParam, FALSE);
			//if ((wParam >= VK_F1 && wParam <= /*VK_F24*/ VK_SCROLL) || wParam <= 32 ||
			//    (wParam >= VK_LSHIFT/*0xA0*/ && wParam <= /*VK_RMENU=0xA5*/ 0xB7 /*=VK_LAUNCH_APP2*/) ||
			//    (wParam >= VK_LWIN/*0x5B*/ && wParam <= VK_APPS/*0x5D*/) ||
			//    /*(wParam >= VK_NUMPAD0 && wParam <= VK_DIVIDE) ||*/ //TODO:
			//    (wParam >= VK_PRIOR/*0x21*/ && wParam <= VK_HELP/*0x2F*/) ||
			//    nLCtrl || nRCtrl ||
			//    ((nLAlt || nRAlt) && !(nLCtrl || nRCtrl || nShift) && (wParam >= VK_NUMPAD0/*0x60*/ && wParam <= VK_NUMPAD9/*0x69*/)) || // ���� Alt-����� ��� ���������� NumLock
			//    FALSE)
			//{
			r.Event.KeyEvent.wRepeatCount = 1; TODO("0-15 ? Specifies the repeat count for the current message. The value is the number of times the keystroke is autorepeated as a result of the user holding down the key. If the keystroke is held long enough, multiple messages are sent. However, the repeat count is not cumulative.");
			r.Event.KeyEvent.wVirtualKeyCode = mn_LastVKeyPressed;
			r.Event.KeyEvent.uChar.UnicodeChar = pszChars[0];
			//if (!nLCtrl && !nRCtrl) {
			//    if (wParam == VK_ESCAPE || wParam == VK_RETURN || wParam == VK_BACK || wParam == VK_TAB || wParam == VK_SPACE
			//        || FALSE)
			//        r.Event.KeyEvent.uChar.UnicodeChar = wParam;
			//}
			//    mn_LastVKeyPressed = 0; // ����� �� ������������ WM_(SYS)CHAR
			//} else {
			//    return;
			//}
			r.Event.KeyEvent.bKeyDown = (messg == WM_KEYDOWN || messg == WM_SYSKEYDOWN);
			//}
			r.Event.KeyEvent.wVirtualScanCode = ((DWORD)lParam & 0xFF0000) >> 16; // 16-23 - Specifies the scan code. The value depends on the OEM.
			// 24 - Specifies whether the key is an extended key, such as the right-hand ALT and CTRL keys that appear on an enhanced 101- or 102-key keyboard. The value is 1 if it is an extended key; otherwise, it is 0.
			// 29 - Specifies the context code. The value is 1 if the ALT key is held down while the key is pressed; otherwise, the value is 0.
			// 30 - Specifies the previous key state. The value is 1 if the key is down before the message is sent, or it is 0 if the key is up.
			// 31 - Specifies the transition state. The value is 1 if the key is being released, or it is 0 if the key is being pressed.
			r.Event.KeyEvent.dwControlKeyState = 0;

			if (((DWORD)lParam & (DWORD)(1 << 24)) != 0)
				r.Event.KeyEvent.dwControlKeyState |= ENHANCED_KEY;

			if ((nCaps & 1) == 1)
				r.Event.KeyEvent.dwControlKeyState |= CAPSLOCK_ON;

			if ((nNum & 1) == 1)
				r.Event.KeyEvent.dwControlKeyState |= NUMLOCK_ON;

			if ((nScroll & 1) == 1)
				r.Event.KeyEvent.dwControlKeyState |= SCROLLLOCK_ON;

			if (nLAlt & 0x8000)
				r.Event.KeyEvent.dwControlKeyState |= LEFT_ALT_PRESSED;

			if (nRAlt & 0x8000)
				r.Event.KeyEvent.dwControlKeyState |= RIGHT_ALT_PRESSED;

			if (nLCtrl & 0x8000)
				r.Event.KeyEvent.dwControlKeyState |= LEFT_CTRL_PRESSED;

			if (nRCtrl & 0x8000)
				r.Event.KeyEvent.dwControlKeyState |= RIGHT_CTRL_PRESSED;

			if (nShift & 0x8000)
				r.Event.KeyEvent.dwControlKeyState |= SHIFT_PRESSED;

#ifdef _DEBUG

			if (r.EventType == KEY_EVENT && r.Event.KeyEvent.bKeyDown &&
			        r.Event.KeyEvent.wVirtualKeyCode == VK_F11)
			{
				DEBUGSTRINPUT(L"  ---  F11 sending\n");
			}

#endif

			// -- �������� �� �������� ������� ScreenToClient
			//// ������� (����) ������ ����� ��������� EMenu �������������� �� ������ �������,
			//// � �� � ��������� ���� (��� ��������� �������� - ��� ����� ���������� �� 2-3 �����).
			//// ������ ��� ������� �������.
			//RemoveFromCursor();

			if (mn_FarPID && mn_FarPID != mn_LastSetForegroundPID)
			{
				//DWORD dwFarPID = GetFarPID();
				//if (dwFarPID)
				AllowSetForegroundWindow(mn_FarPID);
				mn_LastSetForegroundPID = mn_FarPID;
			}

			PostConsoleEvent(&r);

			// ������� ������� ����� ������������������ � ������������������ ���������� ��������...
			/*
			The expected behaviour would be (as it is in a cmd.exe session):
			- hit "^" -> see nothing
			- hit "^" again -> see ^^
			- hit "^" again -> see nothing
			- hit "^" again -> see ^^

			Alternatively:
			- hit "^" -> see nothing
			- hit any other alpha-numeric key, e.g. "k" -> see "^k"
			*/
			for(int i = 1; pszChars[i]; i++)
			{
				r.Event.KeyEvent.uChar.UnicodeChar = pszChars[i];
				PostConsoleEvent(&r);
			}

			//if (messg == WM_CHAR || messg == WM_SYSCHAR) {
			//    // � ����� �������� ����������
			//    r.Event.KeyEvent.bKeyDown = FALSE;
			//    PostConsoleEvent(&r);
			//}
		}
	}
	/*if (IsDebuggerPresent()) {
	    if (hWnd ==ghWnd)
	        DEBUGSTRINPUT(L"   focused ghWnd\n"); else
	    if (hWnd ==hConWnd)
	        DEBUGSTRINPUT(L"   focused hConWnd\n"); else
	    if (hWnd =='ghWnd DC')
	        DEBUGSTRINPUT(L"   focused 'ghWnd DC'\n");
	    else
	        DEBUGSTRINPUT(L"   focused UNKNOWN\n");
	}*/
	return;
}

void CRealConsole::OnKeyboardIme(HWND hWnd, UINT messg, WPARAM wParam, LPARAM lParam)
{
	if (messg != WM_IME_CHAR)
		return;

	INPUT_RECORD r = {KEY_EVENT};
	WORD nCaps = 1 & (WORD)GetKeyState(VK_CAPITAL);
	WORD nNum = 1 & (WORD)GetKeyState(VK_NUMLOCK);
	WORD nScroll = 1 & (WORD)GetKeyState(VK_SCROLL);
	WORD nLAlt = 0x8000 & (WORD)GetKeyState(VK_LMENU);
	WORD nRAlt = 0x8000 & (WORD)GetKeyState(VK_RMENU);
	WORD nLCtrl = 0x8000 & (WORD)GetKeyState(VK_LCONTROL);
	WORD nRCtrl = 0x8000 & (WORD)GetKeyState(VK_RCONTROL);
	WORD nShift = 0x8000 & (WORD)GetKeyState(VK_SHIFT);

	r.Event.KeyEvent.wRepeatCount = 1; // Repeat count. Since the first byte and second byte is continuous, this is always 1.
	r.Event.KeyEvent.wVirtualKeyCode = VK_PROCESSKEY; // � RealConsole ������� VK=0, �� ��� ��� �������
	r.Event.KeyEvent.uChar.UnicodeChar = (wchar_t)wParam;
	r.Event.KeyEvent.bKeyDown = TRUE; //(messg == WM_KEYDOWN || messg == WM_SYSKEYDOWN);
	r.Event.KeyEvent.wVirtualScanCode = ((DWORD)lParam & 0xFF0000) >> 16; // 16-23 - Specifies the scan code. The value depends on the OEM.
	// 24 - Specifies whether the key is an extended key, such as the right-hand ALT and CTRL keys that appear on an enhanced 101- or 102-key keyboard. The value is 1 if it is an extended key; otherwise, it is 0.
	// 29 - Specifies the context code. The value is 1 if the ALT key is held down while the key is pressed; otherwise, the value is 0.
	// 30 - Specifies the previous key state. The value is 1 if the key is down before the message is sent, or it is 0 if the key is up.
	// 31 - Specifies the transition state. The value is 1 if the key is being released, or it is 0 if the key is being pressed.
	r.Event.KeyEvent.dwControlKeyState = 0;

	if (((DWORD)lParam & (DWORD)(1 << 24)) != 0)
		r.Event.KeyEvent.dwControlKeyState |= ENHANCED_KEY;

	if ((nCaps & 1) == 1)
		r.Event.KeyEvent.dwControlKeyState |= CAPSLOCK_ON;

	if ((nNum & 1) == 1)
		r.Event.KeyEvent.dwControlKeyState |= NUMLOCK_ON;

	if ((nScroll & 1) == 1)
		r.Event.KeyEvent.dwControlKeyState |= SCROLLLOCK_ON;

	if (nLAlt & 0x8000)
		r.Event.KeyEvent.dwControlKeyState |= LEFT_ALT_PRESSED;

	if (nRAlt & 0x8000)
		r.Event.KeyEvent.dwControlKeyState |= RIGHT_ALT_PRESSED;

	if (nLCtrl & 0x8000)
		r.Event.KeyEvent.dwControlKeyState |= LEFT_CTRL_PRESSED;

	if (nRCtrl & 0x8000)
		r.Event.KeyEvent.dwControlKeyState |= RIGHT_CTRL_PRESSED;

	if (nShift & 0x8000)
		r.Event.KeyEvent.dwControlKeyState |= SHIFT_PRESSED;

	PostConsoleEvent(&r);
}

void CRealConsole::OnDosAppStartStop(enum StartStopType sst, DWORD anPID)
{
	if (sst == sst_App16Start)
	{
		DEBUGSTRPROC(L"16 bit application STARTED\n");

		if (mn_Comspec4Ntvdm == 0)
		{
			// mn_Comspec4Ntvdm ����� ���� ��� �� ��������, ���� 16��� ������ �� �������
			WARNING("###: ��� ������� vc.com - ntvdm.exe ����������� � ����� ConEmuC.exe, ��� �������� ��������");
			_ASSERTE(mn_Comspec4Ntvdm != 0);
		}

		if (!(mn_ProgramStatus & CES_NTVDM))
			mn_ProgramStatus |= CES_NTVDM;

		// -- � cmdStartStop - mb_IgnoreCmdStop �������������� ������, ������� ������� �����
		//if (gOSVer.dwMajorVersion>5 || (gOSVer.dwMajorVersion==5 && gOSVer.dwMinorVersion>=1))
		mb_IgnoreCmdStop = TRUE;

		SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
	}
	else if (sst == sst_App16Stop)
	{
		//gpConEmu->gbPostUpdateWindowSize = true;
		DEBUGSTRPROC(L"16 bit application TERMINATED\n");
		WARNING("�� ���������� CES_NTVDM �����. ��� �� ��������� ������� ������� �������!");

		if (mn_Comspec4Ntvdm == 0)
		{
			mn_ProgramStatus &= ~CES_NTVDM;
		}

		//2010-02-26 �����. ����� ������ � ��������� � ������ ������� ��������
		//SyncConsole2Window(); // ����� ������ �� 16bit ������ ������ �� ����������� ������� �� ������� GUI
		// ������ �� ���������, ��� 16��� �� �������� � ������ ��������
		if (!gpConEmu->isNtvdm(TRUE))
			SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS);
	}
}

void CRealConsole::OnServerStarted(HWND ahConWnd, DWORD anServerPID)
{
	if (!this)
	{
		_ASSERTE(this);
		return;
	}
	if ((ahConWnd == NULL) || (hConWnd && (ahConWnd != hConWnd)) || (anServerPID != mn_ConEmuC_PID))
	{
		MBoxAssert(ahConWnd!=NULL);
		MBoxAssert((hConWnd==NULL) || (ahConWnd==hConWnd));
		MBoxAssert(anServerPID==mn_ConEmuC_PID);
		return;
	}

	// ������ ������� ������ ����� ��� �� ����������������
	if (hConWnd == NULL)
	{
		SetConStatus(L"Waiting for console server...");
		SetHwnd(ahConWnd);
	}
}

//void CRealConsole::OnWinEvent(DWORD anEvent, HWND hwnd, LONG idObject, LONG idChild, DWORD dwEventThread, DWORD dwmsEventTime)
//{
//	_ASSERTE(hwnd!=NULL);
//
//	if (hConWnd == NULL && anEvent == EVENT_CONSOLE_START_APPLICATION && idObject == (LONG)mn_ConEmuC_PID)
//	{
//		SetConStatus(L"Waiting for console server...");
//		SetHwnd(hwnd);
//	}
//
//	_ASSERTE(hConWnd!=NULL && hwnd==hConWnd);
//	//TODO("!!! ������� ��������� ������� � ��������� m_Processes");
//	//
//	//AddProcess(idobject), � �������� idObject �� ������ ���������
//	// �� ������, ��� ��������� ������ Ntvdm
//	TODO("��� ���������� �� ������� NTVDM - ����� ������� ���� �������");
//
//	switch(anEvent)
//	{
//		case EVENT_CONSOLE_START_APPLICATION:
//			//A new console process has started.
//			//The idObject parameter contains the process identifier of the newly created process.
//			//If the application is a 16-bit application, the idChild parameter is CONSOLE_APPLICATION_16BIT and idObject is the process identifier of the NTVDM session associated with the console.
//		{
//			if (mn_InRecreate>=1)
//				mn_InRecreate = 0; // �������� ������� ������� ������������
//
//			//WARNING("��� ����� ���������, ���� ��������� �� ����������: ������� ����� ���� �������� � ����� ������");
//			//Process Add(idObject);
//			// ���� �������� 16������ ���������� - ���������� �������� ��������� ������ ��������, ����� ����� �������
//#ifndef WIN64
//			_ASSERTE(CONSOLE_APPLICATION_16BIT==1);
//
//			if (idChild == CONSOLE_APPLICATION_16BIT)
//			{
//				OnDosAppStartStop(sst_App16Start, idObject);
//
//				//if (mn_Comspec4Ntvdm == 0)
//				//{
//				//	// mn_Comspec4Ntvdm ����� ���� ��� �� ��������, ���� 16��� ������ �� �������
//				//	_ASSERTE(mn_Comspec4Ntvdm != 0);
//				//}
//
//				//if (!(mn_ProgramStatus & CES_NTVDM))
//				//	mn_ProgramStatus |= CES_NTVDM;
//
//				//if (gOSVer.dwMajorVersion>5 || (gOSVer.dwMajorVersion==5 && gOSVer.dwMinorVersion>=1))
//				//	mb_IgnoreCmdStop = TRUE;
//
//				//SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
//			}
//
//#endif
//		} break;
//		case EVENT_CONSOLE_END_APPLICATION:
//			//A console process has exited.
//			//The idObject parameter contains the process identifier of the terminated process.
//		{
//			//WARNING("��� ����� ���������, ���� ��������� �� ����������: ������� ����� ���� ������ � ����� ������");
//			//Process Delete(idObject);
//			//
//#ifndef WIN64
//			if (idChild == CONSOLE_APPLICATION_16BIT)
//			{
//				OnDosAppStartStop(sst_App16Stop, idObject);
//			}
//
//#endif
//		} break;
//	}
//}


DWORD CRealConsole::RConServerThread(LPVOID lpvParam)
{
	CRealConsole *pRCon = (CRealConsole*)lpvParam;
	CVirtualConsole *pVCon = pRCon->mp_VCon;
	BOOL fConnected = FALSE;
	DWORD dwErr = 0;
	HANDLE hPipe = NULL;
	HANDLE hWait[2] = {NULL,NULL};
	DWORD dwTID = GetCurrentThreadId();
	MCHKHEAP
	_ASSERTE(pVCon!=NULL);
	_ASSERTE(pRCon->hConWnd!=NULL);
	_ASSERTE(pRCon->ms_VConServer_Pipe[0]!=0);
	_ASSERTE(pRCon->mh_ServerSemaphore!=NULL);
	_ASSERTE(pRCon->mh_TermEvent!=NULL);
	//swprintf_c(pRCon->ms_VConServer_Pipe, CEGUIPIPENAME, L".", (DWORD)pRCon->hConWnd); //��� mn_ConEmuC_PID
	// The main loop creates an instance of the named pipe and
	// then waits for a client to connect to it. When the client
	// connects, a thread is created to handle communications
	// with that client, and the loop is repeated.
	hWait[0] = pRCon->mh_TermEvent;
	hWait[1] = pRCon->mh_ServerSemaphore;
	MCHKHEAP

	// ���� �� ����������� ���������� �������
	do
	{
		while(!fConnected)
		{
			_ASSERTE(hPipe == NULL);
			// !!! ���������� �������� �������� ����� CreateNamedPipe ������, �.�. � ���� ������
			//     ���� ������� � ������ �� ����� ����������� � �������
			// ��������� ���������� ��������, ��� �������� �������
			dwErr = WaitForMultipleObjects(2, hWait, FALSE, INFINITE);

			if (dwErr == WAIT_OBJECT_0)
			{
				return 0; // ������� �����������
			}

			MCHKHEAP

			for(int i=0; i<MAX_SERVER_THREADS; i++)
			{
				if (pRCon->mn_RConServerThreadsId[i] == dwTID)
				{
					pRCon->mh_ActiveRConServerThread = pRCon->mh_RConServerThreads[i]; break;
				}
			}

			_ASSERTE(gpLocalSecurity);
			hPipe = CreateNamedPipe(
			            pRCon->ms_VConServer_Pipe, // pipe name
			            PIPE_ACCESS_DUPLEX,       // read/write access
			            PIPE_TYPE_MESSAGE |       // message type pipe
			            PIPE_READMODE_MESSAGE |   // message-read mode
			            PIPE_WAIT,                // blocking mode
			            PIPE_UNLIMITED_INSTANCES, // max. instances
			            PIPEBUFSIZE,              // output buffer size
			            PIPEBUFSIZE,              // input buffer size
			            0,                        // client time-out
			            gpLocalSecurity);          // default security attribute
			MCHKHEAP

			if (hPipe == INVALID_HANDLE_VALUE)
			{
				dwErr = GetLastError();
				_ASSERTE(hPipe != INVALID_HANDLE_VALUE);
				//DisplayLastError(L"CreateNamedPipe failed");
				hPipe = NULL;
				// ��������� ���� ��� ������ ���� ������� ��������� ����
				ReleaseSemaphore(hWait[1], 1, NULL);
				//Sleep(50);
				continue;
			}

			MCHKHEAP

			// ����� ConEmuC ����, ��� ��������� ���� �����
			if (pRCon->mh_GuiAttached)
			{
				SetEvent(pRCon->mh_GuiAttached);
				SafeCloseHandle(pRCon->mh_GuiAttached);
			}

			// Wait for the client to connect; if it succeeds,
			// the function returns a nonzero value. If the function
			// returns zero, GetLastError returns ERROR_PIPE_CONNECTED.
			fConnected = ConnectNamedPipe(hPipe, NULL) ? TRUE : ((dwErr = GetLastError()) == ERROR_PIPE_CONNECTED);
			MCHKHEAP
			// ����� ��������� ������ ���� ������� �����
			ReleaseSemaphore(hWait[1], 1, NULL);

			// ������� �����������!
			if (WaitForSingleObject(hWait[0], 0) == WAIT_OBJECT_0)
			{
				//FlushFileBuffers(hPipe); -- ��� �� �����, �� ������ �� ����������
				//DisconnectNamedPipe(hPipe);
				SafeCloseHandle(hPipe);
				return 0;
			}

			MCHKHEAP

			if (fConnected)
				break;
			else
				SafeCloseHandle(hPipe);
		}

		if (fConnected)
		{
			// ����� �������, ����� �� ������
			fConnected = FALSE;
			//// ��������� ������ ���� ������� ����� //2009-08-28 ���������� ����� ����� ConnectNamedPipe
			//ReleaseSemaphore(hWait[1], 1, NULL);
			MCHKHEAP

			if (gpConEmu->isValid(pVCon))
			{
				_ASSERTE(pVCon==pRCon->mp_VCon);
				pRCon->ServerThreadCommand(hPipe);    // ��� ������������� - ���������� � ���� ��������� ����
			}
			else
			{
				_ASSERTE(FALSE);
				// ��������. VirtualConsole �������, � ���� ��� �� �����������! ���� ������ ����...
				SafeCloseHandle(hPipe);
				return 1;
			}
		}

		MCHKHEAP
		FlushFileBuffers(hPipe);
		//DisconnectNamedPipe(hPipe);
		SafeCloseHandle(hPipe);
	} // ������� � �������� ������ instance �����
	while(WaitForSingleObject(pRCon->mh_TermEvent, 0) != WAIT_OBJECT_0);

	return 0;
}

CESERVER_REQ* CRealConsole::cmdStartStop(HANDLE hPipe, CESERVER_REQ* pIn, UINT nDataSize)
{
	CESERVER_REQ* pOut = ExecuteNewCmd(pIn->hdr.nCmd, sizeof(CESERVER_REQ_HDR) + sizeof(CESERVER_REQ_STARTSTOPRET));
	
	//
	DWORD nStarted = pIn->StartStop.nStarted;
	HWND  hWnd     = (HWND)pIn->StartStop.hWnd;
#ifdef _DEBUG
	wchar_t szDbg[128];

	switch(nStarted)
	{
		case sst_ServerStart:
			_wsprintf(szDbg, SKIPLEN(countof(szDbg)) L"GUI received CECMD_CMDSTARTSTOP(ServerStart,%i,PID=%u)\n", pIn->hdr.nCreateTick, pIn->StartStop.dwPID);
			break;
		case sst_ServerStop:
			_wsprintf(szDbg, SKIPLEN(countof(szDbg)) L"GUI received CECMD_CMDSTARTSTOP(ServerStop,%i,PID=%u)\n", pIn->hdr.nCreateTick, pIn->StartStop.dwPID);
			break;
		case sst_ComspecStart:
			_wsprintf(szDbg, SKIPLEN(countof(szDbg)) L"GUI received CECMD_CMDSTARTSTOP(ComspecStart,%i,PID=%u)\n", pIn->hdr.nCreateTick, pIn->StartStop.dwPID);
			break;
		case sst_ComspecStop:
			_wsprintf(szDbg, SKIPLEN(countof(szDbg)) L"GUI received CECMD_CMDSTARTSTOP(ComspecStop,%i,PID=%u)\n", pIn->hdr.nCreateTick, pIn->StartStop.dwPID);
			break;
		case sst_AppStart:
			_wsprintf(szDbg, SKIPLEN(countof(szDbg)) L"GUI received CECMD_CMDSTARTSTOP(AppStart,%i,PID=%u)\n", pIn->hdr.nCreateTick, pIn->StartStop.dwPID);
			break;
		case sst_AppStop:
			_wsprintf(szDbg, SKIPLEN(countof(szDbg)) L"GUI received CECMD_CMDSTARTSTOP(AppStop,%i,PID=%u)\n", pIn->hdr.nCreateTick, pIn->StartStop.dwPID);
			break;
		case sst_App16Start:
			_wsprintf(szDbg, SKIPLEN(countof(szDbg)) L"GUI received CECMD_CMDSTARTSTOP(App16Start,%i,PID=%u)\n", pIn->hdr.nCreateTick, pIn->StartStop.dwPID);
			break;
		case sst_App16Stop:
			_wsprintf(szDbg, SKIPLEN(countof(szDbg)) L"GUI received CECMD_CMDSTARTSTOP(App16Stop,%i,PID=%u)\n", pIn->hdr.nCreateTick, pIn->StartStop.dwPID);
			break;
	}

	DEBUGSTRCMD(szDbg);
#endif
	_ASSERTE(pIn->StartStop.dwPID!=0);
	DWORD nPID     = pIn->StartStop.dwPID;
	DWORD nSubSystem = pIn->StartStop.nSubSystem;
	BOOL bRunViaCmdExe = pIn->StartStop.bRootIsCmdExe;
	BOOL bUserIsAdmin = pIn->StartStop.bUserIsAdmin;
	BOOL lbWasBuffer = pIn->StartStop.bWasBufferHeight;
	//DWORD nInputTID = pIn->StartStop.dwInputTID;
	_ASSERTE(sizeof(CESERVER_REQ_STARTSTOPRET) <= sizeof(CESERVER_REQ_STARTSTOP));
	//pIn->hdr.cbSize = sizeof(CESERVER_REQ_HDR) + sizeof(CESERVER_REQ_STARTSTOPRET);

	// ���� ������� ������������ (����� ��������� �������) - �������� ��� � m_TerminatedPIDs
	TODO("���������, ����� �� ��������� �������������� sst_App16Stop? �� ����, ��� ����� ����� ���� ������ sst_ComspecStop ������?");
	if (nStarted == sst_ComspecStop || nStarted == sst_AppStop /*|| nStarted == sst_App16Stop*/)
	{
		bool lbPushed = false;
		// ����� �� ��� �������� � m_TerminatedPIDs (���� �� ������ �� ����)
		for (size_t i = 0; i < countof(m_TerminatedPIDs); i++)
		{
			if (m_TerminatedPIDs[i] == nPID)
			{
				lbPushed = true;
				break;
			}
		}
		// ����� ������ ������ � ��������
		for (UINT k = 0; !lbPushed && k <= 1; k++)
		{
			UINT iStart = !k ? mn_TerminatedIdx : 0;
			UINT iEnd = !k ? countof(m_TerminatedPIDs) : min(mn_TerminatedIdx,countof(m_TerminatedPIDs));
			// ����������� �����
			for (UINT i = iStart; i < iEnd; i++)
			{
				if (!m_TerminatedPIDs[i])
				{
					m_TerminatedPIDs[i] = nPID;
					mn_TerminatedIdx = ((i + 1) < countof(m_TerminatedPIDs)) ? (i + 1) : 0;
					lbPushed = true;
					break;
				}
			}
		}
	}


	if (nStarted == sst_ServerStart || nStarted == sst_ComspecStart)
	{
		if (nStarted == sst_ServerStart)
		{
			SetConStatus(L"Waiting for console server...");

			// �������� ������ ���� �������� �����
			_ASSERTE(mp_ABuf==mp_RBuf);
			mp_RBuf->InitMaxSize(pIn->StartStop.crMaxSize);
		}

		// ����� �������� ���������
		pOut->StartStopRet.bWasBufferHeight = isBufferHeight(); // ����� comspec ����, ��� ����� ����� ����� ���������
		//DWORD nParentPID = 0;
		//if (nStarted == 2)
		//{
		//	ConProcess* pPrc = NULL;
		//	int i, nProcCount = GetProcesses(&pPrc);
		//	if (pPrc != NULL)
		//	{
		//		for (i = 0; i < nProcCount; i++) {
		//			if (pPrc[i].ProcessID == nPID) {
		//				nParentPID = pPrc[i].ParentPID; break;
		//			}
		//		}
		//		if (nParentPID == 0) {
		//			_ASSERTE(nParentPID != 0);
		//		} else {
		//			BOOL lbFar = FALSE;
		//			for (i = 0; i < nProcCount; i++) {
		//				if (pPrc[i].ProcessID == nParentPID) {
		//					lbFar = pPrc[i].IsFar; break;
		//				}
		//			}
		//			if (!lbFar) {
		//				_ASSERTE(lbFar);
		//				nParentPID = 0;
		//			}
		//		}
		//		free(pPrc);
		//	}
		//}
		//pOut->StartStopRet.bWasBufferHeight = FALSE;// (nStarted == 2) && (nParentPID == 0); // comspec ������ ��������� � ����������
		pOut->StartStopRet.hWnd = ghWnd;
		pOut->StartStopRet.hWndDC = mp_VCon->GetView();
		pOut->StartStopRet.dwPID = GetCurrentProcessId();
		pOut->StartStopRet.dwSrvPID = mn_ConEmuC_PID;
		pOut->StartStopRet.bNeedLangChange = FALSE;

		if (nStarted == sst_ServerStart)
		{
			//_ASSERTE(nInputTID);
			//_ASSERTE(mn_ConEmuC_Input_TID==0 || mn_ConEmuC_Input_TID==nInputTID);
			//mn_ConEmuC_Input_TID = nInputTID;
			//
			if (!m_Args.bRunAsAdministrator && bUserIsAdmin)
				m_Args.bRunAsAdministrator = TRUE;

			if (mn_InRecreate>=1)
				mn_InRecreate = 0; // �������� ������� ������� ������������

			// ���� ���� Layout �� ��� �������
			if ((gpSet->isMonitorConsoleLang & 2) == 2)
			{
				// ������� - ����, ���� ��� �� �������������
				//	SwitchKeyboardLayout(INPUTLANGCHANGE_SYSCHARSET,gpConEmu->GetActiveKeyboardLayout());
				pOut->StartStopRet.bNeedLangChange = TRUE;
				TODO("��������� �� x64, �� ����� �� ������� � 0xFFFFFFFFFFFFFFFFFFFFF");
				pOut->StartStopRet.NewConsoleLang = gpConEmu->GetActiveKeyboardLayout();
			}

			// ������ �� �������������� ����� ���������� ���� �������
			SetHwnd(hWnd, TRUE);
			// ������� ��������, ��������� KeyboardLayout, � �.�.
			OnServerStarted();
		}

		AllowSetForegroundWindow(nPID);
		
		COORD cr16bit = mp_RBuf->GetDefaultNtvdmHeight();

		/*
		#define IMAGE_SUBSYSTEM_DOS_EXECUTABLE  255
		#define IMAGE_SUBSYSTEM_BATCH_FILE  254
		*/
		
		// ComSpec started
		if (nStarted == sst_ComspecStart)
		{
			// ��������������� � TRUE ���� ����� �������� 16������ ����������
			if (nSubSystem == IMAGE_SUBSYSTEM_DOS_EXECUTABLE/*255*/)
			{
				DEBUGSTRCMD(L"16 bit application STARTED, aquired from CECMD_CMDSTARTSTOP\n");

				//if (!(mn_ProgramStatus & CES_NTVDM))
				//	mn_ProgramStatus |= CES_NTVDM; -- � OnDosAppStartStop

				mn_Comspec4Ntvdm = nPID;
				OnDosAppStartStop(sst_App16Start, nPID);
				//mb_IgnoreCmdStop = TRUE; -- ���, � OnDosAppStartStop

				mp_RBuf->SetConsoleSize(cr16bit.X, cr16bit.Y, 0, CECMD_CMDSTARTED);
				pOut->StartStopRet.nBufferHeight = 0;
				pOut->StartStopRet.nWidth = cr16bit.X;
				pOut->StartStopRet.nHeight = cr16bit.Y;
			}
			else
			{
				BOOL bAllowBufferHeight = (gpSet->AutoBufferHeight || isBufferHeight());
				if (pIn->StartStop.bForceBufferHeight)
					bAllowBufferHeight = (pIn->StartStop.nForceBufferHeight != 0);
				
				// �� ���� ��� ����� ��������
				mb_IgnoreCmdStop = FALSE;
				// ���� ������������ ������ ��������� � ������ ������� ����������
				if (pIn->StartStop.bForceBufferHeight)
				{
					pOut->StartStopRet.nBufferHeight = pIn->StartStop.nForceBufferHeight;
				}
				else
				{
					// � ComSpec �������� ������ ��, ��� ������ � gpSet
					pOut->StartStopRet.nBufferHeight = bAllowBufferHeight ? gpSet->DefaultBufferHeight : 0;
				}
				// 111125 ���� "con.m_sbi.dwSize.X" � "con.m_sbi.dwSize.X"
				pOut->StartStopRet.nWidth = mp_RBuf->GetBufferWidth()/*con.m_sbi.dwSize.X*/;
				pOut->StartStopRet.nHeight = mp_RBuf->GetBufferHeight()/*con.m_sbi.dwSize.Y*/;

				if ((pOut->StartStopRet.nBufferHeight == 0) != (isBufferHeight() == FALSE))
				{
					WARNING("��� �������� ����� �� ������������� ����� ������� ����� ������� �� ������� ConEmuC");
					//con.m_sbi.dwSize.Y = gpSet->DefaultBufferHeight; -- �� ����� ������ �����, � �� SetConsoleSize ������ skip
					mp_RBuf->BuferModeChangeLock();
					mp_RBuf->SetBufferHeightMode((pOut->StartStopRet.nBufferHeight != 0), TRUE); // ����� ������, ����� ������� ����������� ������������
					mp_RBuf->SetConsoleSize(mp_RBuf->GetTextWidth()/*con.m_sbi.dwSize.X*/, mp_RBuf->TextHeight(), pOut->StartStopRet.nBufferHeight, CECMD_CMDSTARTED);
					mp_RBuf->BuferModeChangeUnlock();
				}
			}
		}
		else if (nStarted == sst_ServerStart)
		{
			BOOL b = mp_RBuf->BuferModeChangeLock();
		
			// Server
			if (nSubSystem == IMAGE_SUBSYSTEM_DOS_EXECUTABLE/*255*/)
			{
				pOut->StartStopRet.nBufferHeight = 0;
				pOut->StartStopRet.nWidth = cr16bit.X;
				pOut->StartStopRet.nHeight = cr16bit.Y;
			}
			else
			{
				pOut->StartStopRet.nWidth = mp_RBuf->GetBufferWidth()/*con.m_sbi.dwSize.X*/;

				//0x101 - ������ ���������
				if (nSubSystem != 0x100   // 0x100 - ����� �� ���-�������
				        && (mp_RBuf->isScroll()
				            || (mn_DefaultBufferHeight && bRunViaCmdExe)))
				{
					_ASSERTE(m_Args.bDetached || mn_DefaultBufferHeight == mp_RBuf->GetBufferHeight()/*con.m_sbi.dwSize.Y*/ || mp_RBuf->GetBufferHeight()/*con.m_sbi.dwSize.Y*/ == TextHeight());
					pOut->StartStopRet.nBufferHeight = max(mp_RBuf->GetBufferHeight()/*con.m_sbi.dwSize.Y*/,mn_DefaultBufferHeight);
					_ASSERTE(mp_RBuf->TextHeight()/*con.nTextHeight*/ > 5);
					pOut->StartStopRet.nHeight = mp_RBuf->TextHeight()/*con.nTextHeight*/;
					//111126 - �����. ���� ����� �����������
					//con.m_sbi.dwSize.Y = pOut->StartStopRet.nBufferHeight; // ����� ��������, ����� ����� ����� ���������� ���������������
				}
				else
				{
					_ASSERTE(!mp_RBuf->isScroll());
					pOut->StartStopRet.nBufferHeight = 0;
					pOut->StartStopRet.nHeight = mp_RBuf->TextHeight()/*con.m_sbi.dwSize.Y*/;
				}
				
			}

			mp_RBuf->SetBufferHeightMode((pOut->StartStopRet.nBufferHeight != 0), TRUE);
			mp_RBuf->SetChange2Size(pOut->StartStopRet.nWidth, pOut->StartStopRet.nHeight);
			
			if (b) mp_RBuf->BuferModeChangeUnlock();
		}

		// 23.06.2009 Maks - ������ ����. ������ �������� � ApplyConsoleInfo
		//Process Add(nPID);

	} // (nStarted == sst_ServerStart || nStarted == sst_ComspecStart)
	else if (nStarted == sst_ServerStop || nStarted == sst_ComspecStop)
	{
		// ServerStop ����� �� �������� - ���������� CECMD_SRVSTARTSTOP � ConEmuWnd
		_ASSERTE(nStarted != sst_ServerStop);

		// 23.06.2009 Maks - ������ ����. ������ �������� � ApplyConsoleInfo
		//Process Delete(nPID);

		// ComSpec stopped
		if (nStarted == sst_ComspecStop)
		{
			BOOL lbNeedResizeWnd = FALSE;
			BOOL lbNeedResizeGui = FALSE;
			COORD crNewSize = {TextWidth(),TextHeight()};
			int nNewWidth=0, nNewHeight=0;

			if ((mn_ProgramStatus & CES_NTVDM) == 0
			        && !(gpConEmu->mb_isFullScreen || gpConEmu->isZoomed()))
			{
				pOut->StartStopRet.bWasBufferHeight = FALSE;

				// � ��������� ������� (comspec ��� �������?) GetConsoleScreenBufferInfo ������������
				if (pOut->StartStop.sbi.dwSize.X && pOut->StartStop.sbi.dwSize.Y)
				{
					DWORD nScroll = 0;
					
					// ���������� �������� ������� �������, ������������ ��� �������� SBI
					// 111125 - bBufferHeight ������� �� nScroll (������� ��������� � ������� �������������� ���������)
					if (CRealBuffer::GetConWindowSize(pOut->StartStop.sbi, &nNewWidth, &nNewHeight, &nScroll))
					{
						lbNeedResizeGui = (crNewSize.X != nNewWidth || crNewSize.Y != nNewHeight);

						WARNING("ConResize: �����������, ��� ��� ����� ��������� GUI");
						if (nScroll || crNewSize.X != nNewWidth || crNewSize.Y != nNewHeight)
						{
							// ��� � ������� � ���, ����� �� ���������� ���������� ������ ������ ����,
							// ��� ��� ����� ������ ������ ����, ����� ������ ���� ConEmu
							// ����� ������ ����� (�� � �� �����������), � ��� ������ ������� �������...
							// ����, ����� ����, ��������, ������� "mode con lines=25 cols=80"
							_ASSERTE(crNewSize.X == nNewWidth && crNewSize.Y == nNewHeight);
							
							//gpConEmu->SyncWindowToConsole(); - ��� ������������ ������. �� ������ ��� �� ������� ����, �� ������ - ������ pVCon ����� ���� ��� �� �������
							lbNeedResizeWnd = TRUE;
							
							crNewSize.X = nNewWidth;
							crNewSize.Y = nNewHeight;
							
							//pOut->StartStopRet.bWasBufferHeight = TRUE; -- 111124 ������ ��������� pOut->StartStopRet.bWasBufferHeight=TRUE;
							_ASSERTE(nScroll!=0);
							pOut->StartStopRet.bWasBufferHeight = (nScroll!=0);
						}
					}
				}
			}

			if (mb_IgnoreCmdStop || (mn_ProgramStatus & CES_NTVDM) == CES_NTVDM)
			{
				// ����� ������������ ������ � WinXP � ����
				// ���� �������� 16������ ����������, ������ ����������� ������ ������� ������ ����� 80x25
				// ��� �� ������������� ��������� ������� ��� ������ �� 16���. ������� ����� ������������
				// ��� ������ ����. ��� ������� OnWinEvent.
				//SetBufferHeightMode(FALSE, TRUE);
				//PRAGMA_ERROR("���� �� ������� CECMD_CMDFINISHED �� ��������� WinEvents");
				mb_IgnoreCmdStop = FALSE; // �������� ����� �������, ��� ������ ������ �� �����
				DEBUGSTRCMD(L"16 bit application TERMINATED (aquired from CECMD_CMDFINISHED)\n");

				//mn_ProgramStatus &= ~CES_NTVDM; -- ������� ����� ������������� ������� �������, ����� �� ������
				if (lbWasBuffer)
				{
					mp_RBuf->SetBufferHeightMode(TRUE, TRUE); // ����� ���������, ����� ������� ����������� ������������
				}

				SyncConsole2Window(TRUE); // ����� ������ �� 16bit ������ ������ �� ����������� ������� �� ������� GUI

				if (mn_Comspec4Ntvdm)
				{
					#ifdef _DEBUG
					if (mn_Comspec4Ntvdm != nPID)
					{
						_ASSERTE(mn_Comspec4Ntvdm == nPID);
					}
					#endif
					if ((nSubSystem == IMAGE_SUBSYSTEM_DOS_EXECUTABLE) || (mn_Comspec4Ntvdm == nPID))
						mn_Comspec4Ntvdm = 0;
				}

				// mn_ProgramStatus &= ~CES_NTVDM;
				if ((nSubSystem == IMAGE_SUBSYSTEM_DOS_EXECUTABLE)
					|| ((mn_ProgramStatus & CES_NTVDM) == CES_NTVDM))
				{
					_ASSERTE(nSubSystem == IMAGE_SUBSYSTEM_DOS_EXECUTABLE)
					OnDosAppStartStop(sst_App16Stop, nPID);
				}

				lbNeedResizeWnd = FALSE;
				crNewSize.X = TextWidth();
				crNewSize.Y = TextHeight();
			} //else {

			// ������������ ������ ����� ��������� ConEmuC
			mp_RBuf->BuferModeChangeLock();
			
			//111126 - �����, ���� ������� SetConsoleSize
			//con.m_sbi.dwSize.Y = crNewSize.Y;

			if (!lbWasBuffer)
			{
				mp_RBuf->SetBufferHeightMode(FALSE, TRUE); // ����� ���������, ����� ������� ����������� ������������
			}

			#ifdef _DEBUG
			_wsprintf(szDbg, SKIPLEN(countof(szDbg)) L"Returns normal window size begin at %i\n", GetTickCount());
			DEBUGSTRCMD(szDbg);
			#endif
			
			// �����������. ����� ������ �� ������, ��� ������� ���������
			mp_RBuf->SetConsoleSize(crNewSize.X, crNewSize.Y, 0, CECMD_CMDFINISHED);
			
			#ifdef _DEBUG
			_wsprintf(szDbg, SKIPLEN(countof(szDbg)) L"Finished returns normal window size begin at %i\n", GetTickCount());
			DEBUGSTRCMD(szDbg);
			#endif
			
			#ifdef _DEBUG
			#ifdef WIN64
			//				PRAGMA_ERROR("���� ����������, ��� ����� ����� �� Win7 x64 �������� ������ ����� � �������� ������� � ��������� ��� ������������ ������������� �������!");
			#endif
			#endif
			// ����� nChange2TextWidth, nChange2TextHeight ����� ������������?

			if (lbNeedResizeGui)
			{
				RECT rcCon = MakeRect(nNewWidth, nNewHeight);
				RECT rcNew = gpConEmu->CalcRect(CER_MAIN, rcCon, CER_CONSOLE);
				RECT rcWnd; GetWindowRect(ghWnd, &rcWnd);

				if (gpSet->isDesktopMode)
				{
					MapWindowPoints(NULL, gpConEmu->mh_ShellWindow, (LPPOINT)&rcWnd, 2);
				}

				MOVEWINDOW(ghWnd, rcWnd.left, rcWnd.top, rcNew.right, rcNew.bottom, 1);
			}

			mp_RBuf->BuferModeChangeUnlock();
			//}
		}
		else
		{
			// ���� �� �������� �� ������!
			_ASSERTE(FALSE);
		}
	} // (nStarted == sst_ServerStop || nStarted == sst_ComspecStop)
	else if (nStarted == sst_App16Start || nStarted == sst_App16Stop)
	{
		OnDosAppStartStop((enum StartStopType)nStarted, nPID);
	}
	
	// ������� ��������� � ��������

	//pOut = ExecuteNewCmd(pIn->hdr.nCmd, pIn->hdr.cbSize);
	//if (pIn->hdr.cbSize > sizeof(CESERVER_REQ_HDR))
	//	memmove(pOut->Data, pIn->Data, pIn->hdr.cbSize - (int)sizeof(CESERVER_REQ_HDR));
		
	return pOut;
}

//CESERVER_REQ* CRealConsole::cmdGetGuiHwnd(HANDLE hPipe, CESERVER_REQ* pIn, UINT nDataSize)
//{
//	CESERVER_REQ* pOut = NULL;
//	
//	DEBUGSTRCMD(L"GUI recieved CECMD_GETGUIHWND\n");
//	pOut = ExecuteNewCmd(pIn->hdr.nCmd, sizeof(CESERVER_REQ_HDR) + 2*sizeof(DWORD));
//	pOut->dwData[0] = (DWORD)ghWnd; //-V205
//	pOut->dwData[1] = (DWORD)mp_VCon->GetView(); //-V205
//	return pOut;
//}

CESERVER_REQ* CRealConsole::cmdTabsChanged(HANDLE hPipe, CESERVER_REQ* pIn, UINT nDataSize)
{
	CESERVER_REQ* pOut = NULL;
	
	DEBUGSTRCMD(L"GUI recieved CECMD_TABSCHANGED\n");
	
	BOOL fSuccess = FALSE;
	DWORD cbWritten = 0;

	if (nDataSize == 0)
	{
		// ��� �����������
		if (pIn->hdr.nSrcPID == mn_FarPID)
		{
			mn_ProgramStatus &= ~CES_FARACTIVE;

			for (UINT i = 0; i < mn_FarPlugPIDsCount; i++)  // �������� �� ������ ��������
			{
				if (m_FarPlugPIDs[i] == mn_FarPID)
					m_FarPlugPIDs[i] = 0;
			}

			mn_FarPID_PluginDetected = mn_FarPID = 0;
			CloseFarMapData();

			if (isActive()) gpConEmu->UpdateProcessDisplay(FALSE);  // �������� PID � ���� ���������
		}

		SetTabs(NULL, 1);
	}
	else
	{
		_ASSERTE(nDataSize>=4); //-V112
		_ASSERTE(((pIn->Tabs.nTabCount-1)*sizeof(ConEmuTab))==(nDataSize-sizeof(CESERVER_REQ_CONEMUTAB)));
		BOOL lbCanUpdate = TRUE;

		// ���� ����������� ������ - ��������� �������� ���� FAR ��� ����-����� ������������
		if (pIn->Tabs.bMacroActive)
		{
			if (gpSet->isTabs == 2)
			{
				lbCanUpdate = FALSE;
				// ����� ������� � ������ ����������, ��� ����� ���������� ����������
				CESERVER_REQ *pRet = ExecuteNewCmd(CECMD_TABSCHANGED, sizeof(CESERVER_REQ_HDR) + sizeof(CESERVER_REQ_CONEMUTAB_RET));

				if (pRet)
				{
					pRet->TabsRet.bNeedPostTabSend = TRUE;
					// ����������
					fSuccess = WriteFile(
					               hPipe,        // handle to pipe
					               pRet,         // buffer to write from
					               pRet->hdr.cbSize,  // number of bytes to write
					               &cbWritten,   // number of bytes written
					               NULL);        // not overlapped I/O
					ExecuteFreeResult(pRet);
					
					// ����� � ����� ������ �� ���������
					pOut = (CESERVER_REQ*)INVALID_HANDLE_VALUE;
				}
			}
		}

		// ���� �������� �������� - ���������� �������� ������ ������� ����� (� ����� FAR)
		// �� ������ ���� ����� SendTabs ��� ������ �� �������� ���� ���� (����� ������ ����������� � Synchro)
		if (pIn->Tabs.bMainThread && lbCanUpdate && gpSet->isTabs == 2)
		{
			TODO("��������� ����� ������, ���� ��������� ��������� �����");
			bool lbCurrentActive = gpConEmu->mp_TabBar->IsTabsActive();
			bool lbNewActive = lbCurrentActive;

			// ���� �������� ����� ����� - ��������� ����� �� ���������
			if (gpConEmu->GetVCon(1) == NULL)
			{
				lbNewActive = (pIn->Tabs.nTabCount > 1);
			}

			if (lbCurrentActive != lbNewActive)
			{
				enum ConEmuMargins tTabAction = lbNewActive ? CEM_TABACTIVATE : CEM_TABDEACTIVATE;
				RECT rcConsole = gpConEmu->CalcRect(CER_CONSOLE, gpConEmu->GetIdealRect(), CER_MAIN, NULL, NULL, tTabAction);
				
				mp_RBuf->SetChange2Size(rcConsole.right, rcConsole.bottom);

				TODO("DoubleView: ��� �������");
				gpConEmu->ActiveCon()->SetRedraw(FALSE);

				gpConEmu->mp_TabBar->SetRedraw(FALSE);
				fSuccess = FALSE;
				// ����� ������� � ������ ����������, ��� ����� ������ �������
				CESERVER_REQ *pTmp = ExecuteNewCmd(CECMD_TABSCHANGED, sizeof(CESERVER_REQ_HDR) + sizeof(CESERVER_REQ_CONEMUTAB_RET));

				if (pTmp)
				{
					pTmp->TabsRet.bNeedResize = TRUE;
					pTmp->TabsRet.crNewSize.X = rcConsole.right;
					pTmp->TabsRet.crNewSize.Y = rcConsole.bottom;
					// ����������
					fSuccess = WriteFile(
					               hPipe,        // handle to pipe
					               pTmp,         // buffer to write from
					               pTmp->hdr.cbSize,  // number of bytes to write
					               &cbWritten,   // number of bytes written
					               NULL);        // not overlapped I/O
					ExecuteFreeResult(pTmp);
					
					// ����� � ����� ������ �� ���������
					pOut = (CESERVER_REQ*)INVALID_HANDLE_VALUE;
				}

				if (fSuccess)    // ���������, ���� �� ������� ������ ��������� �������
				{
					WaitConsoleSize(rcConsole.bottom, 500);
				}

				TODO("DoubleView: ��� �������");
				gpConEmu->ActiveCon()->SetRedraw(TRUE);
			}
		}

		if (lbCanUpdate)
		{
			TODO("DoubleView: ��� �������");
			gpConEmu->ActiveCon()->Invalidate();
			SetTabs(pIn->Tabs.tabs, pIn->Tabs.nTabCount);
			gpConEmu->mp_TabBar->SetRedraw(TRUE);
			gpConEmu->ActiveCon()->Redraw();
		}
	}

	// ���� ��� �� �������� �������
	if (pOut == NULL)
		pOut = ExecuteNewCmd(pIn->hdr.nCmd, sizeof(CESERVER_REQ_HDR));

	return pOut;
}

CESERVER_REQ* CRealConsole::cmdGetOutputFile(HANDLE hPipe, CESERVER_REQ* pIn, UINT nDataSize)
{
	CESERVER_REQ* pOut = NULL;
	
	DEBUGSTRCMD(L"GUI recieved CECMD_GETOUTPUTFILE\n");
	_ASSERTE(nDataSize>=4); //-V112
	BOOL lbUnicode = pIn->OutputFile.bUnicode;
	pOut = ExecuteNewCmd(pIn->hdr.nCmd, sizeof(CESERVER_REQ_HDR) + sizeof(CESERVER_REQ_OUTPUTFILE));
	pOut->OutputFile.bUnicode = lbUnicode;
	pOut->OutputFile.szFilePathName[0] = 0; // ���������� PrepareOutputFile

	if (!PrepareOutputFile(lbUnicode, pOut->OutputFile.szFilePathName))
	{
		pOut->OutputFile.szFilePathName[0] = 0;
	}

	return pOut;
}

CESERVER_REQ* CRealConsole::cmdGuiMacro(HANDLE hPipe, CESERVER_REQ* pIn, UINT nDataSize)
{
	CESERVER_REQ* pOut = NULL;

	DEBUGSTRCMD(L"GUI recieved CECMD_GUIMACRO\n");	
	LPWSTR pszResult = CConEmuMacro::ExecuteMacro(pIn->GuiMacro.sMacro, this);
	int nLen = pszResult ? _tcslen(pszResult) : 0;
	pOut = ExecuteNewCmd(pIn->hdr.nCmd, sizeof(CESERVER_REQ_HDR)+sizeof(CESERVER_REQ_GUIMACRO)+nLen*sizeof(wchar_t));

	if (pszResult)
	{
		lstrcpy(pOut->GuiMacro.sMacro, pszResult);
		pOut->GuiMacro.nSucceeded = 1;
		free(pszResult);
	}
	else
	{
		pOut->GuiMacro.sMacro[0] = 0;
		pOut->GuiMacro.nSucceeded = 0;
	}
	
	return pOut;
}

CESERVER_REQ* CRealConsole::cmdLangChange(HANDLE hPipe, CESERVER_REQ* pIn, UINT nDataSize)
{
	CESERVER_REQ* pOut = NULL;
	
	DEBUGSTRLANG(L"GUI recieved CECMD_LANGCHANGE\n");
	_ASSERTE(nDataSize>=4); //-V112
	// LayoutName: "00000409", "00010409", ...
	// � HKL �� ���� ����������, ��� ��� �������� DWORD
	// HKL � x64 �������� ���: "0x0000000000020409", "0xFFFFFFFFF0010409"
	DWORD dwName = pIn->dwData[0];
	gpConEmu->OnLangChangeConsole(mp_VCon, dwName);

	pOut = ExecuteNewCmd(pIn->hdr.nCmd, sizeof(CESERVER_REQ_HDR));

	//#ifdef _DEBUG
	////Sleep(2000);
	//WCHAR szMsg[255];
	//// --> ������ ������ ��� �� "��������" �������. ����� ����������� Post'�� � �������� ����
	//HKL hkl = GetKeyboardLayout(0);
	//swprintf_c(szMsg, L"ConEmu: GetKeyboardLayout(0) on CECMD_LANGCHANGE after GetKeyboardLayout(0) = 0x%08I64X\n",
	//	(unsigned __int64)(DWORD_PTR)hkl);
	//DEBUGSTRLANG(szMsg);
	////Sleep(2000);
	//#endif
	//
	//wchar_t szName[10]; swprintf_c(szName, L"%08X", dwName);
	//DWORD_PTR dwNewKeybLayout = (DWORD_PTR)LoadKeyboardLayout(szName, 0);
	//
	//#ifdef _DEBUG
	//DEBUGSTRLANG(L"ConEmu: Calling GetKeyboardLayout(0)\n");
	////Sleep(2000);
	//hkl = GetKeyboardLayout(0);
	//swprintf_c(szMsg, L"ConEmu: GetKeyboardLayout(0) after LoadKeyboardLayout = 0x%08I64X\n",
	//	(unsigned __int64)(DWORD_PTR)hkl);
	//DEBUGSTRLANG(szMsg);
	////Sleep(2000);
	//#endif
	//
	//if ((gpSet->isMonitorConsoleLang & 1) == 1) {
	//    if (con.dwKeybLayout != dwNewKeybLayout) {
	//        con.dwKeybLayout = dwNewKeybLayout;
	//		if (isActive()) {
	//            gpConEmu->SwitchKeyboardLayout(dwNewKeybLayout);
	//
	//			#ifdef _DEBUG
	//			hkl = GetKeyboardLayout(0);
	//			swprintf_c(szMsg, L"ConEmu: GetKeyboardLayout(0) after SwitchKeyboardLayout = 0x%08I64X\n",
	//				(unsigned __int64)(DWORD_PTR)hkl);
	//			DEBUGSTRLANG(szMsg);
	//			//Sleep(2000);
	//			#endif
	//		}
	//    }
	//}
	return pOut;
}

CESERVER_REQ* CRealConsole::cmdTabsCmd(HANDLE hPipe, CESERVER_REQ* pIn, UINT nDataSize)
{
	CESERVER_REQ* pOut = NULL;
	
	// 0: ��������/�������� ����, 1: ������� �� ���������, 2: ������� �� ����������, 3: commit switch
	DEBUGSTRCMD(L"GUI recieved CECMD_TABSCMD\n");
	_ASSERTE(nDataSize>=1);
	DWORD nTabCmd = pIn->Data[0];
	gpConEmu->TabCommand(nTabCmd);
	pOut = ExecuteNewCmd(pIn->hdr.nCmd, sizeof(CESERVER_REQ_HDR));

	return pOut;
}

CESERVER_REQ* CRealConsole::cmdResources(HANDLE hPipe, CESERVER_REQ* pIn, UINT nDataSize)
{
	CESERVER_REQ* pOut = NULL;
	
	DEBUGSTRCMD(L"GUI recieved CECMD_RESOURCES\n");
	_ASSERTE(nDataSize>=6);
	//mb_PluginDetected = TRUE; // ��������, ��� � ���� ���� ������ (���� ��� ����� ���� ������)
	DWORD nPID = pIn->dwData[0]; // ��������, ��� � ���� ���� ������
	mb_SkipFarPidChange = TRUE;
	// ��������� ���� PID � ������ �����
	bool bAlreadyExist = false;
	int j = -1;

	for (UINT i = 0; i < mn_FarPlugPIDsCount; i++)
	{
		if (m_FarPlugPIDs[i] == nPID)
		{
			bAlreadyExist = true; break;
		}
		else if (m_FarPlugPIDs[i] == 0)
		{
			j = i;
		}
	}

	if (!bAlreadyExist)
	{
		// ���� � ������ ����� ����� PID� ��� ��� - �� ����������� ���������
		if ((j == -1) && (mn_FarPlugPIDsCount < countof(m_FarPlugPIDs)))
			j = mn_FarPlugPIDsCount++;

		if (j >= 0)
			m_FarPlugPIDs[j] = nPID;
	}

	// ��������, ��� � ���� ���� ������
	mn_FarPID_PluginDetected = nPID;
	OpenFarMapData(); // ����������� ������� � ����������� � ����
	// ��������� ���������� PID ���� � MonitorThread (��� ����� ������������� OpenFarMapData)
	mb_SkipFarPidChange = FALSE;

	if (isActive()) gpConEmu->UpdateProcessDisplay(FALSE);  // �������� PID � ���� ���������

	//mn_Far_PluginInputThreadId      = pIn->dwData[1];
	//CheckColorMapping(mn_FarPID_PluginDetected);
	// 23.06.2009 Maks - ������ ����. ������ �������� � ApplyConsoleInfo
	//Process Add(mn_FarPID_PluginDetected); // �� ������ ������, ����� �� ��� �� � ����� ������?
	wchar_t* pszRes = (wchar_t*)(&(pIn->dwData[1])), *pszNext;

	if (*pszRes)
	{
		//EnableComSpec(mn_FarPID_PluginDetected, TRUE);
		//UpdateFarSettings(mn_FarPID_PluginDetected);
		wchar_t* pszItems[] = {ms_EditorRus,ms_ViewerRus,ms_TempPanelRus/*,ms_NameTitle*/};

		for (UINT i = 0; i < countof(pszItems); i++)
		{
			pszNext = pszRes + _tcslen(pszRes)+1;

			if (_tcslen(pszRes)>=30) pszRes[30] = 0;

			lstrcpy(pszItems[i], pszRes);

			if (i < 2) lstrcat(pszItems[i], L" ");

			pszRes = pszNext;

			if (*pszRes == 0)
				break;
		}

		//pszNext = pszRes + _tcslen(pszRes)+1;
		//if (_tcslen(pszRes)>=30) pszRes[30] = 0;
		//lstrcpy(ms_EditorRus, pszRes); lstrcat(ms_EditorRus, L" ");
		//pszRes = pszNext;
		//if (*pszRes) {
		//    pszNext = pszRes + _tcslen(pszRes)+1;
		//    if (_tcslen(pszRes)>=30) pszRes[30] = 0;
		//    lstrcpy(ms_ViewerRus, pszRes); lstrcat(ms_ViewerRus, L" ");
		//    pszRes = pszNext;
		//    if (*pszRes) {
		//        pszNext = pszRes + _tcslen(pszRes)+1;
		//        if (_tcslen(pszRes)>=31) pszRes[31] = 0;
		//        lstrcpy(ms_TempPanelRus, pszRes);
		//        pszRes = pszNext;
		//    }
		//}
	}

	UpdateFarSettings(mn_FarPID_PluginDetected);

	pOut = ExecuteNewCmd(pIn->hdr.nCmd, sizeof(CESERVER_REQ_HDR));

	return pOut;
}

CESERVER_REQ* CRealConsole::cmdSetForeground(HANDLE hPipe, CESERVER_REQ* pIn, UINT nDataSize)
{
	CESERVER_REQ* pOut = NULL;
	
	DEBUGSTRCMD(L"GUI recieved CECMD_SETFOREGROUNDWND\n");
	AllowSetForegroundWindow(pIn->hdr.nSrcPID);

	HWND hWnd = (HWND)pIn->qwData[0];
	DWORD nWndPID = 0; GetWindowThreadProcessId(hWnd, &nWndPID);
	if (nWndPID == GetCurrentProcessId())
	{
		// ���� ��� ���� �� hWndDC - ��������� ������� ����
		if (hWnd != ghWnd && GetParent(hWnd) == ghWnd)
			hWnd = ghWnd;
	}
	
	BOOL lbRc = apiSetForegroundWindow(hWnd);
	pOut = ExecuteNewCmd(pIn->hdr.nCmd, sizeof(CESERVER_REQ_HDR)+sizeof(DWORD));
	if (pOut)
		pOut->dwData[0] = lbRc;

	return pOut;
}

CESERVER_REQ* CRealConsole::cmdFlashWindow(HANDLE hPipe, CESERVER_REQ* pIn, UINT nDataSize)
{
	CESERVER_REQ* pOut = NULL;

	DEBUGSTRCMD(L"GUI recieved CECMD_FLASHWINDOW\n");	
	UINT nFlash = RegisterWindowMessage(CONEMUMSG_FLASHWINDOW);
	WPARAM wParam = 0;

	if (pIn->Flash.bSimple)
	{
		wParam = (pIn->Flash.bInvert ? 2 : 1) << 25;
	}
	else
	{
		wParam = ((pIn->Flash.dwFlags & 0xF) << 24) | (pIn->Flash.uCount & 0xFFFFFF);
	}

	PostMessage(ghWnd, nFlash, wParam, (LPARAM)pIn->Flash.hWnd.u);
	pOut = ExecuteNewCmd(pIn->hdr.nCmd, sizeof(CESERVER_REQ_HDR));

	return pOut;
}

CESERVER_REQ* CRealConsole::cmdRegPanelView(HANDLE hPipe, CESERVER_REQ* pIn, UINT nDataSize)
{
	CESERVER_REQ* pOut = NULL;
	
	DEBUGSTRCMD(L"GUI recieved CECMD_REGPANELVIEW\n");
	pOut = ExecuteNewCmd(pIn->hdr.nCmd, sizeof(CESERVER_REQ_HDR)+sizeof(pIn->PVI));
	pOut->PVI = pIn->PVI;

	if (pOut->PVI.cbSize != sizeof(pOut->PVI))
	{
		pOut->PVI.cbSize = 0; // ������ ������?
	}
	else if (!mp_VCon->RegisterPanelView(&(pOut->PVI)))
	{
		pOut->PVI.cbSize = 0; // ������
	}

	return pOut;
}

CESERVER_REQ* CRealConsole::cmdSetBackground(HANDLE hPipe, CESERVER_REQ* pIn, UINT nDataSize)
{
	CESERVER_REQ* pOut = NULL;

	DEBUGSTRCMD(L"GUI recieved CECMD_SETBACKGROUND\n");	
	pOut = ExecuteNewCmd(pIn->hdr.nCmd, sizeof(CESERVER_REQ_HDR)+sizeof(CESERVER_REQ_SETBACKGROUNDRET));
	// Set background Image
	UINT nCalcSize = pIn->hdr.cbSize - sizeof(pIn->hdr);

	if (nCalcSize < sizeof(CESERVER_REQ_SETBACKGROUND))
	{
		_ASSERTE(nCalcSize >= sizeof(CESERVER_REQ_SETBACKGROUND));
		pOut->BackgroundRet.nResult = esbr_InvalidArg;
	}
	else
	{
		UINT nCalcBmSize = nCalcSize - (((LPBYTE)&pIn->Background.bmp) - ((LPBYTE)&pIn->Background));

		if (pIn->Background.bEnabled && nCalcSize < nCalcBmSize)
		{
			_ASSERTE(nCalcSize >= nCalcBmSize);
			pOut->BackgroundRet.nResult = esbr_InvalidArg;
		}
		else
		{
			pOut->BackgroundRet.nResult = mp_VCon->SetBackgroundImageData(&pIn->Background);
		}
	}

	return pOut;
}

CESERVER_REQ* CRealConsole::cmdActivateCon(HANDLE hPipe, CESERVER_REQ* pIn, UINT nDataSize)
{
	CESERVER_REQ* pOut = NULL;
	
	DEBUGSTRCMD(L"GUI recieved CECMD_ACTIVATECON\n");
	pOut = ExecuteNewCmd(pIn->hdr.nCmd, sizeof(CESERVER_REQ_HDR)+sizeof(CESERVER_REQ_ACTIVATECONSOLE));
	// Activate current console
	_ASSERTE(hConWnd == (HWND)pIn->ActivateCon.hConWnd);

	if (gpConEmu->Activate(mp_VCon))
		pOut->ActivateCon.hConWnd = hConWnd;
	else
		pOut->ActivateCon.hConWnd = NULL;

	return pOut;
}

CESERVER_REQ* CRealConsole::cmdOnCreateProc(HANDLE hPipe, CESERVER_REQ* pIn, UINT nDataSize)
{
	CESERVER_REQ* pOut = NULL;

	DEBUGSTRCMD(L"GUI recieved CECMD_ONCREATEPROC\n");	
	pOut = ExecuteNewCmd(pIn->hdr.nCmd, 
		sizeof(CESERVER_REQ_HDR)+sizeof(CESERVER_REQ_ONCREATEPROCESSRET)
		/*+MAX_PATH*6*/);
	
	BOOL lbDos = (pIn->OnCreateProc.nImageBits == 16)
		&& (pIn->OnCreateProc.nImageSubsystem == IMAGE_SUBSYSTEM_DOS_EXECUTABLE);

	if (ghOpWnd && gpSetCls->hDebug)
	{
		DebugLogShellActivity *shl = (DebugLogShellActivity*)calloc(sizeof(DebugLogShellActivity),1);
		shl->nParentPID = pIn->hdr.nSrcPID;
		shl->nParentBits = pIn->OnCreateProc.nSourceBits;
		wcscpy_c(shl->szFunction, pIn->OnCreateProc.sFunction);
		shl->pszAction = lstrdup(pIn->OnCreateProc.wsValue);
		shl->pszFile   = lstrdup(pIn->OnCreateProc.wsValue+pIn->OnCreateProc.nActionLen);
		shl->pszParam  = lstrdup(pIn->OnCreateProc.wsValue+pIn->OnCreateProc.nActionLen+pIn->OnCreateProc.nFileLen);
		shl->bDos = lbDos;
		shl->nImageBits = pIn->OnCreateProc.nImageBits;
		shl->nImageSubsystem = pIn->OnCreateProc.nImageSubsystem;
		shl->nShellFlags = pIn->OnCreateProc.nShellFlags;
		shl->nCreateFlags = pIn->OnCreateProc.nCreateFlags;
		shl->nStartFlags = pIn->OnCreateProc.nStartFlags;
		shl->nShowCmd = pIn->OnCreateProc.nShowCmd;
		shl->hStdIn = (DWORD)pIn->OnCreateProc.hStdIn;
		shl->hStdOut = (DWORD)pIn->OnCreateProc.hStdOut;
		shl->hStdErr = (DWORD)pIn->OnCreateProc.hStdErr;

		PostMessage(gpSetCls->hDebug, DBGMSG_LOG_ID, DBGMSG_LOG_SHELL_MAGIC, (LPARAM)shl);
	}
	
	if (pIn->OnCreateProc.nImageBits > 0)
	{
		TODO("!!! DosBox allowed?");
		_ASSERTE(lbDos==FALSE); //PRAGMA_ERROR("����� (lbDos && FALSE)?");
		
		if (gpSet->AutoBufferHeight // LongConsoleOutput
			|| (lbDos && FALSE)) // DosBox!!!
		{
			pOut->OnCreateProcRet.bContinue = TRUE;
			//pOut->OnCreateProcRet.bUnicode = TRUE;
			pOut->OnCreateProcRet.bForceBufferHeight = gpSet->AutoBufferHeight;
			
			TODO("!!! DosBox allowed?");
			pOut->OnCreateProcRet.bAllowDosbox = FALSE;
			//pOut->OnCreateProcRet.nFileLen = pIn->OnCreateProc.nFileLen;
			//pOut->OnCreateProcRet.nBaseLen = _tcslen(gpConEmu->ms_ConEmuBaseDir)+2; // +����+\0
			
			////_wcscpy_c(pOut->OnCreateProcRet.wsValue, MAX_PATH+1, pszFile);
			//_wcscpy_c(pOut->OnCreateProcRet.wsValue, MAX_PATH+1, gpConEmu->ms_ConEmuBaseDir);
			//_wcscat_c(pOut->OnCreateProcRet.wsValue, MAX_PATH+1, L"\\");
		}
	}
	
	return pOut;
}

//CESERVER_REQ* CRealConsole::cmdNewConsole(HANDLE hPipe, CESERVER_REQ* pIn, UINT nDataSize)
//{
//	CESERVER_REQ* pOut = NULL;
//
//	DEBUGSTRCMD(L"GUI recieved CECMD_NEWCONSOLE\n");		
//	pOut = ExecuteNewCmd(pIn->hdr.nCmd, sizeof(CESERVER_REQ_HDR)+sizeof(wchar_t));
//	pOut->wData[0] = 0;
//	
//	return pOut;
//}

CESERVER_REQ* CRealConsole::cmdOnPeekReadInput(HANDLE hPipe, CESERVER_REQ* pIn, UINT nDataSize)
{
	CESERVER_REQ* pOut = NULL;

	DEBUGSTRCMD(L"GUI recieved CECMD_PEEKREADINFO\n");
	
	if (ghOpWnd && gpSetCls->hDebug && gpSetCls->m_ActivityLoggingType == glt_Input)
	{
		if (nDataSize >= sizeof(CESERVER_REQ_PEEKREADINFO))
		{
			CESERVER_REQ_PEEKREADINFO* pCopy = (CESERVER_REQ_PEEKREADINFO*)malloc(nDataSize);
			if (pCopy)
			{
				memmove(pCopy, &pIn->PeekReadInfo, nDataSize);
				PostMessage(gpSetCls->hDebug, DBGMSG_LOG_ID, DBGMSG_LOG_INPUT_MAGIC, (LPARAM)pCopy);
			}
		}
	}
	
	pOut = ExecuteNewCmd(pIn->hdr.nCmd, sizeof(CESERVER_REQ_HDR));	
	return pOut;
}

CESERVER_REQ* CRealConsole::cmdOnSetConsoleKeyShortcuts(HANDLE hPipe, CESERVER_REQ* pIn, UINT nDataSize)
{
	CESERVER_REQ* pOut = NULL;

	DEBUGSTRCMD(L"GUI recieved CECMD_KEYSHORTCUTS\n");

	m_ConsoleKeyShortcuts = pIn->Data[0] ? pIn->Data[1] : 0;
	gpConEmu->UpdateWinHookSettings();

	pOut = ExecuteNewCmd(pIn->hdr.nCmd, sizeof(CESERVER_REQ_HDR));	
	return pOut;
}

CESERVER_REQ* CRealConsole::cmdLockDc(HANDLE hPipe, CESERVER_REQ* pIn, UINT nDataSize)
{
	CESERVER_REQ* pOut = NULL;

	DEBUGSTRCMD(L"GUI recieved CECMD_LOCKDC\n");
	
	_ASSERTE(pIn->LockDc.hDcWnd == mp_VCon->GetView());
	
	mp_VCon->LockDcRect(pIn->LockDc.bLock, &pIn->LockDc.Rect);
	
	pOut = ExecuteNewCmd(pIn->hdr.nCmd, sizeof(CESERVER_REQ_HDR));	
	return pOut;
}

//CESERVER_REQ* CRealConsole::cmdAssert(HANDLE hPipe, CESERVER_REQ* pIn, UINT nDataSize)
//{
//	CESERVER_REQ* pOut = NULL;
//
//	DEBUGSTRCMD(L"GUI recieved CECMD_ASSERT\n");		
//	pOut = ExecuteNewCmd(pIn->hdr.nCmd, sizeof(CESERVER_REQ_HDR)+sizeof(wchar_t));
//	pOut->wData[0] = MessageBox(NULL, pIn->AssertInfo.szDebugInfo, pIn->AssertInfo.szTitle, MB_SETFOREGROUND|MB_SYSTEMMODAL|MB_RETRYCANCEL);
//	
//	return pOut;
//}

// ��� ������� ���� �� ���������!
void CRealConsole::ServerThreadCommand(HANDLE hPipe)
{
	CESERVER_REQ in= {{0}}, *pIn=NULL;
	CESERVER_REQ *pOut = NULL;
	DWORD cbRead = 0, cbWritten = 0, dwErr = 0;
	BOOL fSuccess = FALSE;
#ifdef _DEBUG
	HANDLE lhConEmuC = mh_ConEmuC;
#endif
	MCHKHEAP;
	// Send a message to the pipe server and read the response.
	fSuccess = ReadFile(
	               hPipe,            // pipe handle
	               &in,              // buffer to receive reply
	               sizeof(in),       // size of read buffer
	               &cbRead,          // bytes read
	               NULL);            // not overlapped

	if (!fSuccess && ((dwErr = GetLastError()) != ERROR_MORE_DATA))
	{
#ifdef _DEBUG
		// ���� ������� ����������� - MonitorThread � ��������� ����� ��� ������
		DEBUGSTRPROC(L"!!! ReadFile(pipe) failed - console in close?\n");
		//DWORD dwWait = WaitForSingleObject ( mh_TermEvent, 0 );
		//if (dwWait == WAIT_OBJECT_0) return;
		//Sleep(1000);
		//if (lhConEmuC != mh_ConEmuC)
		//	dwWait = WAIT_OBJECT_0;
		//else
		//	dwWait = WaitForSingleObject ( mh_ConEmuC, 0 );
		//if (dwWait == WAIT_OBJECT_0) return;
		//_ASSERTE("ReadFile(pipe) failed"==NULL);
#endif
		//CloseHandle(hPipe);
		return;
	}

	if (in.hdr.nVersion != CESERVER_REQ_VER)
	{
		gpConEmu->ReportOldCmdVersion(in.hdr.nCmd, in.hdr.nVersion, in.hdr.nSrcPID==GetServerPID() ? 1 : 0, in.hdr.nSrcPID, in.hdr.hModule, in.hdr.nBits);
		return;
	}

	_ASSERTE(in.hdr.cbSize>=sizeof(CESERVER_REQ_HDR) && cbRead>=sizeof(CESERVER_REQ_HDR));

	if (cbRead < sizeof(CESERVER_REQ_HDR) || /*in.hdr.cbSize < cbRead ||*/ in.hdr.nVersion != CESERVER_REQ_VER)
	{
		//CloseHandle(hPipe);
		return;
	}
	
	gpSetCls->debugLogCommand(&in, TRUE, timeGetTime(), 0, ms_VConServer_Pipe, pOut);

	if (in.hdr.cbSize <= cbRead)
	{
		pIn = &in; // ��������� ������ �� ���������
	}
	else
	{
		int nAllSize = in.hdr.cbSize;
		pIn = (CESERVER_REQ*)calloc(nAllSize,1);
		_ASSERTE(pIn!=NULL);
		memmove(pIn, &in, cbRead);
		_ASSERTE(pIn->hdr.nVersion==CESERVER_REQ_VER);
		LPBYTE ptrData = ((LPBYTE)pIn)+cbRead;
		nAllSize -= cbRead;

		while(nAllSize>0)
		{
			//_tprintf(TEXT("%s\n"), chReadBuf);

			// Break if TransactNamedPipe or ReadFile is successful
			if (fSuccess)
				break;

			// Read from the pipe if there is more data in the message.
			fSuccess = ReadFile(
			               hPipe,      // pipe handle
			               ptrData,    // buffer to receive reply
			               nAllSize,   // size of buffer
			               &cbRead,    // number of bytes read
			               NULL);      // not overlapped

			// Exit if an error other than ERROR_MORE_DATA occurs.
			if (!fSuccess && ((dwErr = GetLastError()) != ERROR_MORE_DATA))
				break;

			ptrData += cbRead;
			nAllSize -= cbRead;
		}

		TODO("����� ���������� ASSERT, ���� ������� ���� ������� � �������� ������");
		_ASSERTE(nAllSize==0);

		if (nAllSize>0)
		{
			//CloseHandle(hPipe);
			return; // ������� ������� �� ��� ������
		}
	}

	UINT nDataSize = pIn->hdr.cbSize - sizeof(CESERVER_REQ_HDR);

	// ��� ������ �� ����� ��������, ������������ ������� � ���������� (���� �����) ���������

	//  //if (pIn->hdr.nCmd == CECMD_GETFULLINFO /*|| pIn->hdr.nCmd == CECMD_GETSHORTINFO*/) {
	//  if (pIn->hdr.nCmd == CECMD_GETCONSOLEINFO) {
	//  	_ASSERTE(pIn->hdr.nCmd != CECMD_GETCONSOLEINFO);
	//// ������ ���� �� �� � ����� �������. ����� �� �������, ���������� ������ ��������� PopPacket
	////if (!con.bInSetSize && !con.bBufferHeight && pIn->ConInfo.inf.sbi.dwSize.Y > 200) {
	////	_ASSERTE(con.bBufferHeight || pIn->ConInfo.inf.sbi.dwSize.Y <= 200);
	////}
	////#ifdef _DEBUG
	////wchar_t szDbg[255]; swprintf_c(szDbg, L"GUI recieved %s, PktID=%i, Tick=%i\n",
	////	(pIn->hdr.nCmd == CECMD_GETFULLINFO) ? L"CECMD_GETFULLINFO" : L"CECMD_GETSHORTINFO",
	////	pIn->ConInfo.inf.nPacketId, pIn->hdr.nCreateTick);
	//      //DEBUGSTRCMD(szDbg);
	////#endif
	//      ////ApplyConsoleInfo(pIn);
	//      //if (((LPVOID)&in)==((LPVOID)pIn)) {
	//      //    // ��� ������������� ������ - ���������� (in)
	//      //    _ASSERTE(in.hdr.cbSize>0);
	//      //    // ��� ��� ��������� ����� ������� ����� ������, ������� ��������� PopPacket
	//      //    pIn = (CESERVER_REQ*)calloc(in.hdr.cbSize,1);
	//      //    memmove(pIn, &in, in.hdr.cbSize);
	//      //}
	//      //PushPacket(pIn);
	//      //pIn = NULL;

	//  } else

	if (pIn->hdr.nCmd == CECMD_CMDSTARTSTOP)
		pOut = cmdStartStop(hPipe, pIn, nDataSize);
	//else if (pIn->hdr.nCmd == CECMD_GETGUIHWND)
	//	pOut = cmdGetGuiHwnd(hPipe, pIn, nDataSize);
	else if (pIn->hdr.nCmd == CECMD_TABSCHANGED)
		pOut = cmdTabsChanged(hPipe, pIn, nDataSize);
	else if (pIn->hdr.nCmd == CECMD_GETOUTPUTFILE)
		pOut = cmdGetOutputFile(hPipe, pIn, nDataSize);
	else if (pIn->hdr.nCmd == CECMD_GUIMACRO)
		pOut = cmdGuiMacro(hPipe, pIn, nDataSize);
	else if (pIn->hdr.nCmd == CECMD_LANGCHANGE)
		pOut = cmdLangChange(hPipe, pIn, nDataSize);
	else if (pIn->hdr.nCmd == CECMD_TABSCMD)
		pOut = cmdTabsCmd(hPipe, pIn, nDataSize);
	else if (pIn->hdr.nCmd == CECMD_RESOURCES)
		pOut = cmdResources(hPipe, pIn, nDataSize);
	else if (pIn->hdr.nCmd == CECMD_SETFOREGROUND)
		pOut = cmdSetForeground(hPipe, pIn, nDataSize);
	else if (pIn->hdr.nCmd == CECMD_FLASHWINDOW)
		pOut = cmdFlashWindow(hPipe, pIn, nDataSize);
	else if (pIn->hdr.nCmd == CECMD_REGPANELVIEW)
		pOut = cmdRegPanelView(hPipe, pIn, nDataSize);
	else if (pIn->hdr.nCmd == CECMD_SETBACKGROUND)
		pOut = cmdSetBackground(hPipe, pIn, nDataSize);
	else if (pIn->hdr.nCmd == CECMD_ACTIVATECON)
		pOut = cmdActivateCon(hPipe, pIn, nDataSize);
	else if (pIn->hdr.nCmd == CECMD_ONCREATEPROC)
		pOut = cmdOnCreateProc(hPipe, pIn, nDataSize);
	//else if (pIn->hdr.nCmd == CECMD_NEWCONSOLE)
	//	pOut = cmdNewConsole(hPipe, pIn, nDataSize);
	else if (pIn->hdr.nCmd == CECMD_PEEKREADINFO)
		pOut = cmdOnPeekReadInput(hPipe, pIn, nDataSize);
	else if (pIn->hdr.nCmd == CECMD_KEYSHORTCUTS)
		pOut = cmdOnSetConsoleKeyShortcuts(hPipe, pIn, nDataSize);
	else if (pIn->hdr.nCmd == CECMD_ALIVE)
		pOut = ExecuteNewCmd(CECMD_ALIVE, sizeof(CESERVER_REQ_HDR));
	//else if (pIn->hdr.nCmd == CECMD_ASSERT)
	else if (pIn->hdr.nCmd == CECMD_LOCKDC)
		pOut = cmdLockDc(hPipe, pIn, nDataSize);
	//else if (pIn->hdr.nCmd == CECMD_ASSERT)
	//	pOut = cmdAssert(hPipe, pIn, nDataSize);
	else
	{
		// 0 - ����� assert-��� ������ �������������� �������
		pOut = ExecuteNewCmd(0/*pIn->hdr.nCmd*/, sizeof(CESERVER_REQ_HDR));
	}


	
	if (pOut != (CESERVER_REQ*)INVALID_HANDLE_VALUE)
	{
		if (pOut == NULL)
		{
			// ���� �� "������" ������� � ����� ������
			// 0 - ����� assert-��� ������ �������������� �������
			pOut = ExecuteNewCmd(pIn->hdr.nCmd, sizeof(CESERVER_REQ_HDR));
		}
		
		// ������ ����-������ �������� � ����, � �� ������ (Pipe was closed) � ������� ���������
		fSuccess = WriteFile(hPipe, pOut, pOut->hdr.cbSize, &cbWritten, NULL);

		ExecuteFreeResult(pOut);
	}
	

	// ���������� ������
	if (pIn && (LPVOID)pIn != (LPVOID)&in)
	{
		free(pIn); pIn = NULL;
	}

	MCHKHEAP;
	//CloseHandle(hPipe);
	return;
}

void CRealConsole::OnServerClosing(DWORD anSrvPID)
{
	if (anSrvPID == mn_ConEmuC_PID && mh_ConEmuC)
	{
		//int nCurProcessCount = m_Processes.size();
		//_ASSERTE(nCurProcessCount <= 1);
		m_ServerClosing.nRecieveTick = GetTickCount();
		m_ServerClosing.hServerProcess = mh_ConEmuC;
		m_ServerClosing.nServerPID = anSrvPID;
		// ��������� ������ �����������, ���� ����� �� ��������
		ms_ConEmuC_Pipe[0] = 0;
	}
	else
	{
		_ASSERTE(anSrvPID == mn_ConEmuC_PID);
	}
}

int CRealConsole::GetProcesses(ConProcess** ppPrc)
{
	if (mn_InRecreate)
	{
		DWORD dwCurTick = GetTickCount();
		DWORD dwDelta = dwCurTick - mn_InRecreate;

		if (dwDelta > CON_RECREATE_TIMEOUT)
		{
			mn_InRecreate = 0;
		}
		else if (ppPrc == NULL)
		{
			if (mn_InRecreate && !mb_ProcessRestarted && mh_ConEmuC)
			{
				DWORD dwExitCode = 0;

				if (GetExitCodeProcess(mh_ConEmuC, &dwExitCode) && dwExitCode != STILL_ACTIVE)
				{
					RecreateProcessStart();
				}
			}

			return 1; // ����� �� ����� Recreate GUI �� �����������
		}
	}

	// ���� ����� ������ ������ ���������� ���������
	if (ppPrc == NULL || mn_ProcessCount == 0)
	{
		if (ppPrc) *ppPrc = NULL;

		return mn_ProcessCount;
	}

	MSectionLock SPRC; SPRC.Lock(&csPRC);
	int dwProcCount = m_Processes.size();

	if (dwProcCount > 0)
	{
		*ppPrc = (ConProcess*)calloc(dwProcCount, sizeof(ConProcess));
		if (*ppPrc == NULL)
		{
			_ASSERTE((*ppPrc)!=NULL);
			return dwProcCount;
		}
		
		std::vector<ConProcess>::iterator end = m_Processes.end();
		int i = 0;
		for (std::vector<ConProcess>::iterator iter = m_Processes.begin(); iter != end; ++iter, ++i)
		{
			(*ppPrc)[i] = *iter;
		}
	}
	else
	{
		*ppPrc = NULL;
	}

	SPRC.Unlock();
	return dwProcCount;
}

DWORD CRealConsole::GetProgramStatus()
{
	if (!this)
		return 0;

	return mn_ProgramStatus;
}

DWORD CRealConsole::GetFarStatus()
{
	if (!this)
		return 0;

	if ((mn_ProgramStatus & CES_FARACTIVE) == 0)
		return 0;

	return mn_FarStatus;
}

DWORD CRealConsole::GetServerPID()
{
	if (!this)
		return 0;

	if (mb_InCreateRoot && !mn_ConEmuC_PID)
	{
		Sleep(500);
		_ASSERTE(!mb_InCreateRoot || mn_ConEmuC_PID);
		//if (GetCurrentThreadId() != mn_MonitorThreadID) {
		//	WaitForSingleObject(mh_CreateRootEvent, 30000); -- ���� - DeadLock
		//}
	}

	return mn_ConEmuC_PID;
}

bool CRealConsole::isServerCreated()
{
	return (mn_ConEmuC_PID!=0);
}

DWORD CRealConsole::GetFarPID(BOOL abPluginRequired/*=FALSE*/)
{
	if (!this)
		return 0;

	if (!mn_FarPID  // ������ ���� �������� ��� PID
	        || ((mn_ProgramStatus & CES_FARACTIVE) == 0) // ��������� ����
	        || ((mn_ProgramStatus & CES_NTVDM) == CES_NTVDM)) // ���� ������� 16��� ��������� - ������ ��� ����� �� ��������
		return 0;

	if (abPluginRequired)
	{
		if (mn_FarPID_PluginDetected && mn_FarPID_PluginDetected == mn_FarPID)
			return mn_FarPID_PluginDetected;
		else
			return 0;
	}

	return mn_FarPID;
}

// ������� PID "������� ���������" �������� � �������
DWORD CRealConsole::GetActivePID()
{
	if (!this)
		return 0;

	if (hGuiWnd)
		return mn_GuiWndPID;

	DWORD nPID = GetFarPID();
	if (nPID)
		return nPID;
	return mn_ActivePID;
}

// �������� ������ ���������� ��������
// ���������� TRUE ���� �������� ������ (Far/�� Far)
BOOL CRealConsole::ProcessUpdateFlags(BOOL abProcessChanged)
{
	BOOL lbChanged = FALSE;
	//Warning: ������ ���������� ������ �� ProcessAdd/ProcessDelete, �.�. ��� ������ �� ���������
	bool bIsFar = false, bIsTelnet = false, bIsCmd = false;
	DWORD dwFarPID = 0, dwActivePID = 0;
	// ������� 16bit ���������� ������ �� WinEvent. ����� �� ��������� ������ ��� ����������,
	// �.�. ������� ntvdm.exe �� �����������, � �������� � ������.
	bool bIsNtvdm = (mn_ProgramStatus & CES_NTVDM) == CES_NTVDM;
	
	if (bIsNtvdm && mn_Comspec4Ntvdm)
		bIsCmd = true;

	std::vector<ConProcess>::reverse_iterator iter = m_Processes.rbegin();
	std::vector<ConProcess>::reverse_iterator rend = m_Processes.rend();
	
	while (iter != rend)
	{
		ConProcess cp = *iter;
		// �������� ������� ConEmuC �� ���������!
		if (cp.ProcessID != mn_ConEmuC_PID)
		{
			if (!bIsFar)
			{
				if (cp.IsFar)
				{
					bIsFar = true;
				}
				else if (cp.ProcessID == mn_FarPID_PluginDetected)
				{
					bIsFar = true;
					iter->IsFar = iter->IsFarPlugin = true;
				}
			}

			if (!bIsTelnet && cp.IsTelnet)
				bIsTelnet = true;

			//if (!bIsNtvdm && cp.IsNtvdm) bIsNtvdm = true;
			if (!bIsFar && !bIsCmd && cp.IsCmd)
				bIsCmd = true;

			//if (!bIsCmd && mn_Comspec4Ntvdm && cp.ProcessID == mn_Comspec4Ntvdm)
			//	bIsCmd = bIsNtvdm = true;

			//
			if (!dwFarPID && cp.IsFar)
			{
				dwFarPID = cp.ProcessID;
				//dwInputTID = cp.InputTID;
			}
			
			// "������� �������� �������"
			if (!dwActivePID)
				dwActivePID = cp.ProcessID;
			else if (dwActivePID == cp.ParentPID)
				dwActivePID = cp.ProcessID;
		}

		++iter;
	}

	TODO("������, �������� cmd.exe ����� ���� ������� � � '����'? �������� �� Update");

	if (bIsCmd && bIsFar)  // ���� � ������� ������� cmd.exe - ������ (������ �����?) ��� ��������� �������
	{
		bIsFar = false; dwFarPID = 0;
	}

	DWORD nNewProgramStatus = 0;

	if (bIsFar)
		nNewProgramStatus |= CES_FARACTIVE;

	if (bIsFar && bIsNtvdm)
		// 100627 -- �������, ��� ��� �� ��������� 16��� ����������� ��� cmd (%comspec%)
		bIsNtvdm = false;

	//#ifdef _DEBUG
	//else
	//	nNewProgramStatus = nNewProgramStatus;
	//#endif
	if (bIsTelnet)
		nNewProgramStatus |= CES_TELNETACTIVE;

	if (bIsNtvdm)  // ������������ ���� ��� "(mn_ProgramStatus & CES_NTVDM) == CES_NTVDM"
		nNewProgramStatus |= CES_NTVDM;

	if (mn_ProgramStatus != nNewProgramStatus)
		mn_ProgramStatus = nNewProgramStatus;

	mn_ProcessCount = m_Processes.size();

	if (dwFarPID && mn_FarPID != dwFarPID)
		AllowSetForegroundWindow(dwFarPID);

	if (!mn_FarPID && mn_FarPID != dwFarPID)
	{
		// ���� �� ����� ��� �� ���, � ������ �������� ��� - ������������ ������.
		// ��� ����� �� ������ ������, ����� �������������� ������������ ���������� ������ � �������.
		// ���� ��� ���, � ���� �� ��� - ����������� �� ������, ����� �� ������
		// �� �������� ������� ��������� (������ - ����� ActiveHelp �� ���������)
		lbChanged = TRUE;
	}
	mn_FarPID = dwFarPID;
	
	if (mn_ActivePID != dwActivePID)
		mn_ActivePID = dwActivePID;

	//if (!dwFarPID)
	//	mn_FarPID_PluginDetected = 0;

	//TODO("���� �������� FAR - ����������� ������ ��� Colorer - CheckColorMapping();");
	//mn_FarInputTID = dwInputTID;

	if (mn_ProcessCount == 0)
	{
		if (mn_InRecreate == 0)
		{
			StopSignal();
		}
		else if (mn_InRecreate == 1)
		{
			mn_InRecreate = 2;
		}
	}

	// �������� ������ ��������� � ���� �������� � ��������
	if (abProcessChanged)
	{
		gpConEmu->UpdateProcessDisplay(abProcessChanged);
		//2009-09-10
		//gpConEmu->mp_TabBar->Refresh(mn_ProgramStatus & CES_FARACTIVE);
		gpConEmu->mp_TabBar->Update();
	}

	return lbChanged;
}

// ���������� TRUE ���� �������� ������ (Far/�� Far)
BOOL CRealConsole::ProcessUpdate(const DWORD *apPID, UINT anCount)
{
	BOOL lbChanged = FALSE;
	TODO("OPTIMIZE: ������ �� �� ������ ������ ����������, �� � �� ������ ��������� �����...");
	MSectionLock SPRC; SPRC.Lock(&csPRC);
	BOOL lbRecreateOk = FALSE;

	if (mn_InRecreate && mn_ProcessCount == 0)
	{
		// ��� ���� ���-�� ������, ������ �� �������� �����, ������ ������� �����������
		lbRecreateOk = TRUE;
	}

	_ASSERTE(anCount<40);

	if (anCount>40) anCount = 40;

	DWORD PID[40]; memmove(PID, apPID, anCount*sizeof(DWORD));
	UINT i = 0;
	std::vector<ConProcess>::iterator iter, end;
	//BOOL bAlive = FALSE;
	BOOL bProcessChanged = FALSE, bProcessNew = FALSE, bProcessDel = FALSE;

	// ���������, ����� �����-�� �������� ��� �������� ��� ������������� - �� �� ���������
	for (UINT j = 0; j < anCount; j++)
	{
		for (UINT k = 0; k < countof(m_TerminatedPIDs); k++)
		{
			if (m_TerminatedPIDs[k] == PID[j])
			{
				PID[j] = 0; break;
			}
		}
	}

	// ���������, ����� �����-�� �� ����������� � m_FarPlugPIDs ��������� ���������� �� �������
	for (UINT j = 0; j < mn_FarPlugPIDsCount; j++)
	{
		if (m_FarPlugPIDs[j] == 0)
			continue;

		bool bFound = false;

		for(i = 0; i < anCount; i++)
		{
			if (PID[i] == m_FarPlugPIDs[j])
			{
				bFound = true; break;
			}
		}

		if (!bFound)
			m_FarPlugPIDs[j] = 0;
	}

	// ��������� ��������� �� ��� ��������, ����� ��� ��� ������
	iter = m_Processes.begin();
	end = m_Processes.end();
	while (iter != end)
	{
		iter->inConsole = false;
		++iter;
	}

	// ���������, ����� �������� ��� ���� � ����� ������
	iter = m_Processes.begin();
	while (iter != end)
	{
		for (i = 0; i < anCount; i++)
		{
			if (PID[i] && PID[i] == iter->ProcessID)
			{
				iter->inConsole = true;
				PID[i] = 0; // ��� ��������� ��� �� �����, �� � ��� �����
				break;
			}
		}

		++iter;
	}

	// ���������, ���� �� ���������
	for(i = 0; i < anCount; i++)
	{
		if (PID[i])
		{
			bProcessNew = TRUE; break;
		}
	}

	iter = m_Processes.begin();
	while (iter != end)
	{
		if (iter->inConsole == false)
		{
			bProcessDel = TRUE; break;
		}

		++iter;
	}

	// ������ ����� �������� ����� �������
	if (bProcessNew || bProcessDel)
	{
		ConProcess cp;
		HANDLE h = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
		_ASSERTE(h!=INVALID_HANDLE_VALUE);

		if (h==INVALID_HANDLE_VALUE)
		{
			DWORD dwErr = GetLastError();
			wchar_t szError[255];
			_wsprintf(szError, SKIPLEN(countof(szError)) L"Can't create process snapshoot, ErrCode=0x%08X", dwErr);
			gpConEmu->DebugStep(szError);
		}
		else
		{
			//Snapshoot ������, �������
			// ����� ����������� ������ - ��������� ��������� �� ��� ��������, ����� ��� ��� ������
			iter = m_Processes.begin();
			end = m_Processes.end();
			while (iter != end)
			{
				iter->Alive = false;
				++iter;
			}

			PROCESSENTRY32 p; memset(&p, 0, sizeof(p)); p.dwSize = sizeof(p);
			BOOL TerminatedPIDsExist[128] = {};
			_ASSERTE(countof(TerminatedPIDsExist)==countof(m_TerminatedPIDs));

			if (Process32First(h, &p))
			{
				do
				{
					DWORD th32ProcessID = p.th32ProcessID;
					// ���� ���� ������� ��� ������ ��� sst_ComspecStop/sst_AppStop/sst_App16Stop
					// - �������� ���.���������� � 0 � continue;
					// ����� � ������� ��������� ����� ���������� "����������", �� ��� �� ������������ �� �������
					// ��� � ���� �������, ����� ��������� � ������ ��������� � ����������� �������� ������� (Far/�� Far)
					for (UINT k = 0; k < countof(m_TerminatedPIDs); k++)
					{
						if (m_TerminatedPIDs[k] == th32ProcessID)
						{
							th32ProcessID = 0;
							TerminatedPIDsExist[k] = TRUE;
							break;
						}
					}
					if (!th32ProcessID)
						continue; // ��������� �������

					// ���� �� ���� � PID[] - �������� � m_Processes
					if (bProcessNew)
					{
						for (i = 0; i < anCount; i++)
						{
							if (PID[i] && PID[i] == p.th32ProcessID)
							{
								if (!bProcessChanged) bProcessChanged = TRUE;

								memset(&cp, 0, sizeof(cp));
								cp.ProcessID = PID[i]; cp.ParentPID = p.th32ParentProcessID;
								ProcessCheckName(cp, p.szExeFile); //far, telnet, cmd, conemuc, � ��.
								cp.Alive = true;
								cp.inConsole = true;
								SPRC.RelockExclusive(300); // �������������, ���� ��� ��� �� �������
								m_Processes.push_back(cp);
							}
						}
					}

					// ���������� ����������� �������� - ��������� ������ Alive
					// ��������� ��� ��� ��� ���������, ������� ����� ��� ������� �� �������
					iter = m_Processes.begin();
					end = m_Processes.end();
					while (iter != end)
					{
						if (iter->ProcessID == p.th32ProcessID)
						{
							iter->Alive = true;

							if (!iter->NameChecked)
							{
								// ��������, ��� �������� ������ (������������ ��� ��������)
								if (!bProcessChanged) bProcessChanged = TRUE;

								//far, telnet, cmd, conemuc, � ��.
								ProcessCheckName(*iter, p.szExeFile);
								// ��������� ��������
								iter->ParentPID = p.th32ParentProcessID;
							}
						}

						++iter;
					}

					// �������� �������
				}
				while(Process32Next(h, &p));

				// ������ �� ������� �� ��������, ������� ��� ���
				for (UINT k = 0; k < countof(m_TerminatedPIDs); k++)
				{
					if (m_TerminatedPIDs[k] && !TerminatedPIDsExist[k])
						m_TerminatedPIDs[k] = 0;
				}
			}

			// ������� shapshoot
			SafeCloseHandle(h);
		}
	}

	// ������ ��������, ������� ��� ���
	iter = m_Processes.begin();
	end = m_Processes.end();
	while (iter != end)
	{
		if (!iter->Alive || !iter->inConsole)
		{
			if (!bProcessChanged) bProcessChanged = TRUE;

			SPRC.RelockExclusive(300); // ���� ��� ���� ������������ - ������ ������ FALSE
			iter = m_Processes.erase(iter);
			end = m_Processes.end();
		}
		else
		{
			//if (mn_Far_PluginInputThreadId && mn_FarPID_PluginDetected
			//    && iter->ProcessID == mn_FarPID_PluginDetected
			//    && iter->InputTID == 0)
			//    iter->InputTID = mn_Far_PluginInputThreadId;
			++iter;
		}
	}

	// �������� ������ ���������� ��������, �������� PID FAR'�, ��������� ���������� ��������� � �������
	if (ProcessUpdateFlags(bProcessChanged))
		lbChanged = TRUE;

	//
	if (lbRecreateOk)
		mn_InRecreate = 0;

	return lbChanged;
}

void CRealConsole::ProcessCheckName(struct ConProcess &ConPrc, LPWSTR asFullFileName)
{
	wchar_t* pszSlash = _tcsrchr(asFullFileName, _T('\\'));
	if (pszSlash) pszSlash++; else pszSlash=asFullFileName;

	int nLen = _tcslen(pszSlash);

	if (nLen>=63) pszSlash[63]=0;

	lstrcpyW(ConPrc.Name, pszSlash);
	ConPrc.IsFar = lstrcmpi(ConPrc.Name, _T("Far.exe"))==0;
	ConPrc.IsNtvdm = lstrcmpi(ConPrc.Name, _T("ntvdm.exe"))==0;
	ConPrc.IsTelnet = lstrcmpi(ConPrc.Name, _T("telnet.exe"))==0;
	TODO("��� ������� �� ������������, � �� ��������� �������� conemuc, �� �������� ������� ��� FAR, ��� ������� �������� ������, ����� GUI ���������� � ���� �������");
	ConPrc.IsCmd = lstrcmpi(ConPrc.Name, _T("cmd.exe"))==0
	               || lstrcmpi(ConPrc.Name, _T("conemuc.exe"))==0
	               || lstrcmpi(ConPrc.Name, _T("conemuc64.exe"))==0;
	ConPrc.NameChecked = true;
}

BOOL CRealConsole::WaitConsoleSize(int anWaitSize, DWORD nTimeout)
{
	BOOL lbRc = FALSE;
	//CESERVER_REQ *pIn = NULL, *pOut = NULL;
	DWORD nStart = GetTickCount();
	DWORD nDelta = 0;
	//BOOL lbBufferHeight = FALSE;
	//int nNewWidth = 0, nNewHeight = 0;

	if (nTimeout > 10000) nTimeout = 10000;

	if (nTimeout == 0) nTimeout = 100;

	if (GetCurrentThreadId() == mn_MonitorThreadID)
	{
		_ASSERTE(GetCurrentThreadId() != mn_MonitorThreadID);
		return FALSE;
	}

#ifdef _DEBUG
	wchar_t szDbg[128]; _wsprintf(szDbg, SKIPLEN(countof(szDbg)) L"CRealConsole::WaitConsoleSize(H=%i, Timeout=%i)\n", anWaitSize, nTimeout);
	DEBUGSTRTABS(szDbg);
#endif
	WARNING("������, ������� � ������ ����� � �� ��������? ��� ���������? ������ ��������� �������� �� FileMap");
	
	// ����� �� ���������� �� ������� ������� � �������� �����, � �������� - � ��������
	_ASSERTE(mp_ABuf==mp_RBuf);

	while (nDelta < nTimeout)
	{
		// ���� - if (GetConWindowSize(con.m_sbi, nNewWidth, nNewHeight, &lbBufferHeight))
		if (anWaitSize == mp_RBuf->GetWindowHeight())
		{
			lbRc = TRUE;
			break;
		}

		SetEvent(mh_MonitorThreadEvent);
		Sleep(10);
		nDelta = GetTickCount() - nStart;
	}

	_ASSERTE(lbRc && "WaitConsoleSize");
	DEBUGSTRTABS(lbRc ? L"CRealConsole::WaitConsoleSize SUCCEEDED\n" : L"CRealConsole::WaitConsoleSize FAILED!!!\n");
	return lbRc;
}

// -- �������� �� �������� ������� ScreenToClient
//void CRealConsole::RemoveFromCursor()
//{
//	if (!this) return;
//	//
//	if (gpSet->isLockRealConsolePos) return;
//	// ������� (����) ������ ����� ��������� EMenu �������������� �� ������ �������,
//	// � �� � ��������� ���� (��� ��������� �������� - ��� ����� ���������� �� 2-3 �����).
//	// ������ ��� ������� �������.
//	if (!isWindowVisible())
//	{  // ������ �������� ������� ���� ������� ���, ����� ������ ��� ��� ����
//		RECT con; POINT ptCur;
//		GetWindowRect(hConWnd, &con);
//		GetCursorPos(&ptCur);
//		short x = ptCur.x + 1;
//		short y = ptCur.y + 1;
//		if (con.left != x || con.top != y)
//			MOVEWINDOW(hConWnd, x, y, con.right - con.left + 1, con.bottom - con.top + 1, TRUE);
//	}
//}

void CRealConsole::ShowConsoleOrGuiClient(int nMode) // -1 Toggle 0 - Hide 1 - Show
{
	if (this == NULL) return;

	// � GUI-������ (putty, notepad, ...) CtrlWinAltSpace "�����������" �������� (������ detach/attach)
	// �� ������ � ��� ������, ���� �� ������� "��������" ����� (GUI ������, ������� ����� ������� �������)
	if (hGuiWnd && isGuiVisible())
	{
		ShowGuiClient(nMode);
	}
	else
	{
		ShowConsole(nMode);
	}
}


void CRealConsole::ShowGuiClient(int nMode) // -1 Toggle 0 - Hide 1 - Show
{
	if (this == NULL) return;

	// � GUI-������ (putty, notepad, ...) CtrlWinAltSpace "�����������" �������� (������ detach/attach)
	// �� ������ � ��� ������, ���� �� ������� "��������" ����� (GUI ������, ������� ����� ������� �������)
	if (!hGuiWnd)
		return;

	if (nMode == -1)
	{
		nMode = mb_GuiExternMode ? 0 : 1;
	}

	gpConEmu->SetSkipOnFocus(TRUE);

	// ������� Gui ���������� �� ������� ConEmu (�� Detach �� ������)	
	CESERVER_REQ *pIn = ExecuteNewCmd(CECMD_SETGUIEXTERN, sizeof(CESERVER_REQ_HDR) + sizeof(DWORD));
	if (pIn)
	{
		pIn->dwData[0] = (nMode == 0) ? FALSE : TRUE;

		CESERVER_REQ *pOut = ExecuteHkCmd(mn_GuiWndPID, pIn, ghWnd);
		if (pOut && pOut->hdr.cbSize == pIn->hdr.cbSize)
		{
			mb_GuiExternMode = pOut->dwData[0];
			ExecuteFreeResult(pOut);
		}
		ExecuteFreeResult(pIn);

		SetOtherWindowPos(hGuiWnd, ghWnd, 0,0,0,0, SWP_NOMOVE|SWP_NOSIZE);
	}

	gpConEmu->SetSkipOnFocus(FALSE);
	
	mp_VCon->Invalidate();
}

void CRealConsole::ShowConsole(int nMode) // -1 Toggle 0 - Hide 1 - Show
{
	if (this == NULL) return;

	if (!hConWnd) return;

	if (nMode == -1)
	{
		//nMode = IsWindowVisible(hConWnd) ? 0 : 1;
		nMode = isShowConsole ? 0 : 1;
	}

	if (nMode == 1)
	{
		isShowConsole = true;
		//apiShowWindow(hConWnd, SW_SHOWNORMAL);
		//if (setParent) SetParent(hConWnd, 0);
		RECT rcCon, rcWnd; GetWindowRect(hConWnd, &rcCon); GetWindowRect(ghWnd, &rcWnd);
		//if (!IsDebuggerPresent())
		TODO("��������������� ������� ���, ����� �� ������� �� �����");

		if (SetOtherWindowPos(hConWnd, HWND_TOPMOST,
			rcWnd.right-rcCon.right+rcCon.left, rcWnd.bottom-rcCon.bottom+rcCon.top,
			0,0, SWP_NOSIZE|SWP_SHOWWINDOW))
		{
			if (!IsWindowEnabled(hConWnd))
				EnableWindow(hConWnd, true); // ��� ��������� ������� - �� ���������.

			DWORD dwExStyle = GetWindowLong(hConWnd, GWL_EXSTYLE);

			#if 0
			DWORD dw1, dwErr;

			if ((dwExStyle & WS_EX_TOPMOST) == 0)
			{
				dw1 = SetWindowLong(hConWnd, GWL_EXSTYLE, dwExStyle|WS_EX_TOPMOST);
				dwErr = GetLastError();
				dwExStyle = GetWindowLong(hConWnd, GWL_EXSTYLE);

				if ((dwExStyle & WS_EX_TOPMOST) == 0)
				{
					SetOtherWindowPos(hConWnd, HWND_TOPMOST,
						0,0,0,0, SWP_NOSIZE|SWP_NOMOVE);
				}
			}
			#endif

			// Issue 246. ���������� ����� � ConEmu ����� ������ ���� ������� ����������
			// "OnTop" ��� RealConsole, ����� - RealConsole "��������" �� ������ �����
			if ((dwExStyle & WS_EX_TOPMOST))
				gpConEmu->setFocus();

			//} else { //2010-06-05 �� ���������. SetOtherWindowPos �������� ������� � ������� ��� �������������
			//	if (isAdministrator() || (m_Args.pszUserName != NULL)) {
			//		// ���� ��� �������� � Win7 as admin
			//        CESERVER_REQ *pIn = ExecuteNewCmd(CECMD_SHOWCONSOLE, sizeof(CESERVER_REQ_HDR) + sizeof(DWORD));
			//		if (pIn) {
			//			pIn->dwData[0] = SW_SHOWNORMAL;
			//			CESERVER_REQ *pOut = ExecuteSrvCmd(GetServerPID(), pIn, ghWnd);
			//			if (pOut) ExecuteFreeResult(pOut);
			//			ExecuteFreeResult(pIn);
			//		}
			//	}
		}

		//if (setParent) SetParent(hConWnd, 0);
	}
	else
	{
		isShowConsole = false;
		ShowOtherWindow(hConWnd, SW_HIDE);
		////if (!gpSet->isConVisible)
		//if (!apiShowWindow(hConWnd, SW_HIDE)) {
		//	if (isAdministrator() || (m_Args.pszUserName != NULL)) {
		//		// ���� ��� �������� � Win7 as admin
		//        CESERVER_REQ *pIn = ExecuteNewCmd(CECMD_SHOWCONSOLE, sizeof(CESERVER_REQ_HDR) + sizeof(DWORD));
		//		if (pIn) {
		//			pIn->dwData[0] = SW_HIDE;
		//			CESERVER_REQ *pOut = ExecuteSrvCmd(GetServerPID(), pIn, ghWnd);
		//			if (pOut) ExecuteFreeResult(pOut);
		//			ExecuteFreeResult(pIn);
		//		}
		//	}
		//}
		////if (setParent) SetParent(hConWnd, setParent2 ? ghWnd : 'ghWnd DC');
		////if (!gpSet->isConVisible)
		////EnableWindow(hConWnd, false); -- �������� �� �����
		gpConEmu->setFocus();
	}
}

//void CRealConsole::CloseMapping()
//{
//	if (pConsoleData) {
//		UnmapViewOfFile(pConsoleData);
//		pConsoleData = NULL;
//	}
//	if (hFileMapping) {
//		CloseHandle(hFileMapping);
//		hFileMapping = NULL;
//	}
//}

void CRealConsole::OnServerStarted()
{
	//if (!mp_ConsoleInfo)
	if (!m_ConsoleMap.IsValid())
	{
		// ���������������� ����� ������, �������, ��������� � �.�.
		InitNames();
		// ������� map � �������, ������ �� ��� ������ ���� ������
		OpenMapHeader();
	}

	// � �������� Colorer
	// ��������� �� ����������� ���� ���������
	CreateColorMapping();

	// ������������ ����� CESERVER_REQ_STARTSTOPRET
	//if ((gpSet->isMonitorConsoleLang & 2) == 2) // ���� Layout �� ��� �������
	//	SwitchKeyboardLayout(INPUTLANGCHANGE_SYSCHARSET,gpConEmu->GetActiveKeyboardLayout());
}

// ���� ��� ������� ������� - �������, ��� ������� ����������� ���������
// � ��� �� �������� �� ����� ��������� ������� ���� ConEmu
void CRealConsole::OnRConStartedSuccess()
{
	if (this)
	{
		mb_RConStartedSuccess = TRUE;
		gpConEmu->OnRConStartedSuccess(this);
	}
}

void CRealConsole::SetHwnd(HWND ahConWnd, BOOL abForceApprove /*= FALSE*/)
{
	// ���� ���������? ������������ �������?
	if (hConWnd && !IsWindow(hConWnd))
	{
		_ASSERTE(IsWindow(hConWnd));
		hConWnd = NULL;
	}

	// ��� ���� ��� ������ (AttachGui/ConsoleEvent/CMD_START)
	if (hConWnd != NULL)
	{
		if (hConWnd != ahConWnd)
		{
			if (m_ConsoleMap.IsValid())
			{
				_ASSERTE(!m_ConsoleMap.IsValid());
				//CloseMapHeader(); // ����� ��� ��������� � ������� ����? ���� �� ������
				// OpenMapHeader() ���� �� �����, �.�. map ��� ���� ��� �� ������
			}

			Assert(hConWnd == ahConWnd);
			if (!abForceApprove)
				return;
		}
	}

	hConWnd = ahConWnd;
	SetWindowLongPtr(mp_VCon->GetView(), 0, (LONG_PTR)ahConWnd);
	//if (mb_Detached && ahConWnd) // �� ����������, � �� ���� ����� �� ������!
	//  mb_Detached = FALSE; // ����� ������, �� ��� ������������
	//OpenColorMapping();
	mb_ProcessRestarted = FALSE; // ������� ��������
	mb_InCloseConsole = FALSE;
	ZeroStruct(m_ServerClosing);
	if (mn_InRecreate>=1)
		mn_InRecreate = 0; // �������� ������� ������� ������������

	if (ms_VConServer_Pipe[0] == 0)
	{
		wchar_t szEvent[64];

		if (!mh_GuiAttached)
		{
			_wsprintf(szEvent, SKIPLEN(countof(szEvent)) CEGUIRCONSTARTED, (DWORD)hConWnd); //-V205
			//// ������ ����� ������� � ������� ��� �� �������
			//mh_GuiAttached = OpenEvent(EVENT_MODIFY_STATE, FALSE, ms_VConServer_Pipe);
			//// �����, ����� ������������ run as administrator - event ������� �� ����������?
			//if (!mh_GuiAttached) {
			mh_GuiAttached = CreateEvent(gpLocalSecurity, TRUE, FALSE, szEvent);
			_ASSERTE(mh_GuiAttached!=NULL);
			//}
		}

		// ��������� ��������� ����
		_wsprintf(ms_VConServer_Pipe, SKIPLEN(countof(ms_VConServer_Pipe)) CEGUIPIPENAME, L".", (DWORD)hConWnd); //��� mn_ConEmuC_PID //-V205

		if (!mh_ServerSemaphore)
			mh_ServerSemaphore = CreateSemaphore(NULL, 1, 1, NULL);

		for (int i=0; i<MAX_SERVER_THREADS; i++)
		{
			if (mh_RConServerThreads[i])
				continue;

			mn_RConServerThreadsId[i] = 0;
			mh_RConServerThreads[i] = CreateThread(NULL, 0, RConServerThread, (LPVOID)this, 0, &mn_RConServerThreadsId[i]);
			_ASSERTE(mh_RConServerThreads[i]!=NULL);
		}

		// ����� ConEmuC ����, ��� �� ������
		//     if (mh_GuiAttached) {
		//     	SetEvent(mh_GuiAttached);
		//Sleep(10);
		//     	SafeCloseHandle(mh_GuiAttached);
		//		}
	}

	ShowConsole(gpSet->isConVisible ? 1 : 0); // ���������� ����������� ���� ���� AlwaysOnTop ��� �������� ���

	//else if (isAdministrator())
	//	ShowConsole(0); // � Win7 ��� ���� ���������� ������� - �������� �������� � ConEmuC

	// ���������� � OnServerStarted
	//if ((gpSet->isMonitorConsoleLang & 2) == 2) // ���� Layout �� ��� �������
	//    SwitchKeyboardLayout(INPUTLANGCHANGE_SYSCHARSET,gpConEmu->GetActiveKeyboardLayout());

	if (isActive())
	{
		ghConWnd = hConWnd;
		// ����� ����� ���� ����� ����� ���� �� ������ �������
		SetWindowLongPtr(ghWnd, GWLP_USERDATA, (LONG_PTR)hConWnd);
	}
}

void CRealConsole::OnFocus(BOOL abFocused)
{
	if (!this) return;

	if ((mn_Focused == -1) ||
	        ((mn_Focused == 0) && abFocused) ||
	        ((mn_Focused == 1) && !abFocused))
	{
#ifdef _DEBUG

		if (abFocused)
		{
			DEBUGSTRINPUT(L"--Get focus\n")
		}
		else
		{
			DEBUGSTRINPUT(L"--Loose focus\n")
		}

#endif
		// �����, ����� �� ��������� PostConsoleEvent RCon ����� �����������?
		mn_Focused = abFocused ? 1 : 0;
		INPUT_RECORD r = {FOCUS_EVENT};
		r.Event.FocusEvent.bSetFocus = abFocused;
		PostConsoleEvent(&r);
	}
}

void CRealConsole::CreateLogFiles()
{
	if (!m_UseLogs || mh_LogInput) return;  // ���

	DWORD dwErr = 0;
	wchar_t szFile[MAX_PATH+64], *pszDot;
	_ASSERTE(gpConEmu->ms_ConEmuExe[0]);
	lstrcpyW(szFile, gpConEmu->ms_ConEmuExe);

	if ((pszDot = wcsrchr(szFile, L'\\')) == NULL)
	{
		DisplayLastError(L"wcsrchr failed!");
		return; // ������
	}

	*pszDot = 0;
	//mpsz_LogPackets = (wchar_t*)calloc(pszDot - szFile + 64, 2);
	//lstrcpyW(mpsz_LogPackets, szFile);
	//swprintf_c(mpsz_LogPackets+(pszDot-szFile), L"\\ConEmu-recv-%i-%%i.con", mn_ConEmuC_PID); // ConEmu-recv-<ConEmuC_PID>-<index>.con
	_wsprintf(pszDot, SKIPLEN(countof(szFile)-(pszDot-szFile)) L"\\ConEmu-input-%i.log", mn_ConEmuC_PID);
	mh_LogInput = CreateFileW(szFile, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

	if (mh_LogInput == INVALID_HANDLE_VALUE)
	{
		mh_LogInput = NULL;
		dwErr = GetLastError();
		wchar_t szError[MAX_PATH*2];
		_wsprintf(szError, SKIPLEN(countof(szError)) L"Create log file failed! ErrCode=0x%08X\n%s\n", dwErr, szFile);
		MBoxA(szError);
		return;
	}

	mpsz_LogInputFile = lstrdup(szFile);
	// OK, ��� �������
}

void CRealConsole::LogString(LPCWSTR asText, BOOL abShowTime /*= FALSE*/)
{
	if (!this) return;

	if (!asText || !mh_LogInput) return;

	char chAnsi[255];
	WideCharToMultiByte(CP_ACP, 0, asText, -1, chAnsi, 254, 0,0);
	chAnsi[254] = 0;
	LogString(chAnsi, abShowTime);
}

void CRealConsole::LogString(LPCSTR asText, BOOL abShowTime /*= FALSE*/)
{
	if (!this) return;

	if (!asText) return;

	if (mh_LogInput)
	{
		DWORD dwLen;

		if (abShowTime)
		{
			SYSTEMTIME st; GetLocalTime(&st);
			char szTime[32];
			_wsprintfA(szTime, SKIPLEN(countof(szTime)) "%i:%02i:%02i.%03i ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
			dwLen = strlen(szTime);
			WriteFile(mh_LogInput, szTime, dwLen, &dwLen, 0);
		}

		if ((dwLen = strlen(asText))>0)
			WriteFile(mh_LogInput, asText, dwLen, &dwLen, 0);

		WriteFile(mh_LogInput, "\r\n", 2, &dwLen, 0);
		FlushFileBuffers(mh_LogInput);
	}
	else
	{
#ifdef _DEBUG
		DEBUGSTRLOG(asText); DEBUGSTRLOG("\n");
#endif
	}
}

void CRealConsole::LogInput(INPUT_RECORD* pRec)
{
	if (!mh_LogInput || m_UseLogs<2) return;

	char szInfo[192] = {0};
	SYSTEMTIME st; GetLocalTime(&st);
	_wsprintfA(szInfo, SKIPLEN(countof(szInfo)) "%i:%02i:%02i.%03i ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
	char *pszAdd = szInfo+strlen(szInfo);

	switch(pRec->EventType)
	{
			/*case FOCUS_EVENT: _wsprintfA(pszAdd, countof(szInfo)-(pszAdd-szInfo), "FOCUS_EVENT(%i)\r\n", pRec->Event.FocusEvent.bSetFocus);
			    break;*/
		case MOUSE_EVENT: _wsprintfA(pszAdd, SKIPLEN(countof(szInfo)-(pszAdd-szInfo)) "MOUSE_EVENT\r\n");
			{
				_wsprintfA(pszAdd, SKIPLEN(countof(szInfo)-(pszAdd-szInfo))
				           "Mouse: {%ix%i} Btns:{", pRec->Event.MouseEvent.dwMousePosition.X, pRec->Event.MouseEvent.dwMousePosition.Y);
				pszAdd += strlen(pszAdd);

				if (pRec->Event.MouseEvent.dwButtonState & 1) strcat(pszAdd, "L");

				if (pRec->Event.MouseEvent.dwButtonState & 2) strcat(pszAdd, "R");

				if (pRec->Event.MouseEvent.dwButtonState & 4) strcat(pszAdd, "M1");

				if (pRec->Event.MouseEvent.dwButtonState & 8) strcat(pszAdd, "M2");

				if (pRec->Event.MouseEvent.dwButtonState & 0x10) strcat(pszAdd, "M3");

				pszAdd += strlen(pszAdd);

				if (pRec->Event.MouseEvent.dwButtonState & 0xFFFF0000)
				{
					_wsprintfA(pszAdd, SKIPLEN(countof(szInfo)-(pszAdd-szInfo))
					           "x%04X", (pRec->Event.MouseEvent.dwButtonState & 0xFFFF0000)>>16);
				}

				strcat(pszAdd, "} "); pszAdd += strlen(pszAdd);
				_wsprintfA(pszAdd, SKIPLEN(countof(szInfo)-(pszAdd-szInfo))
				           "KeyState: 0x%08X ", pRec->Event.MouseEvent.dwControlKeyState);

				if (pRec->Event.MouseEvent.dwEventFlags & 0x01) strcat(pszAdd, "|MOUSE_MOVED");

				if (pRec->Event.MouseEvent.dwEventFlags & 0x02) strcat(pszAdd, "|DOUBLE_CLICK");

				if (pRec->Event.MouseEvent.dwEventFlags & 0x04) strcat(pszAdd, "|MOUSE_WHEELED"); //-V112

				if (pRec->Event.MouseEvent.dwEventFlags & 0x08) strcat(pszAdd, "|MOUSE_HWHEELED");

				strcat(pszAdd, "\r\n");
			} break;
		case KEY_EVENT:
		{
			char chAcp; WideCharToMultiByte(CP_ACP, 0, &pRec->Event.KeyEvent.uChar.UnicodeChar, 1, &chAcp, 1, 0,0);
			/* */ _wsprintfA(pszAdd, SKIPLEN(countof(szInfo)-(pszAdd-szInfo))
			                 "%c %s count=%i, VK=%i, SC=%i, CH=%i, State=0x%08x %s\r\n",
			                 chAcp ? chAcp : ' ',
			                 pRec->Event.KeyEvent.bKeyDown ? "Down," : "Up,  ",
			                 pRec->Event.KeyEvent.wRepeatCount,
			                 pRec->Event.KeyEvent.wVirtualKeyCode,
			                 pRec->Event.KeyEvent.wVirtualScanCode,
			                 pRec->Event.KeyEvent.uChar.UnicodeChar,
			                 pRec->Event.KeyEvent.dwControlKeyState,
			                 (pRec->Event.KeyEvent.dwControlKeyState & ENHANCED_KEY) ?
			                 "<Enhanced>" : "");
		} break;
	}

	if (*pszAdd)
	{
		DWORD dwLen = 0;
		WriteFile(mh_LogInput, szInfo, strlen(szInfo), &dwLen, 0);
		FlushFileBuffers(mh_LogInput);
	}
}

void CRealConsole::CloseLogFiles()
{
	SafeCloseHandle(mh_LogInput);

	if (mpsz_LogInputFile)
	{
		//DeleteFile(mpsz_LogInputFile);
		free(mpsz_LogInputFile); mpsz_LogInputFile = NULL;
	}

	//if (mpsz_LogPackets) {
	//    //wchar_t szMask[MAX_PATH*2]; wcscpy(szMask, mpsz_LogPackets);
	//    //wchar_t *psz = wcsrchr(szMask, L'%');
	//    //if (psz) {
	//    //    wcscpy(psz, L"*.*");
	//    //    psz = wcsrchr(szMask, L'\\');
	//    //    if (psz) {
	//    //        psz++;
	//    //        WIN32_FIND_DATA fnd;
	//    //        HANDLE hFind = FindFirstFile(szMask, &fnd);
	//    //        if (hFind != INVALID_HANDLE_VALUE) {
	//    //            do {
	//    //                if ((fnd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)==0) {
	//    //                    wcscpy(psz, fnd.cFileName);
	//    //                    DeleteFile(szMask);
	//    //                }
	//    //            } while (FindNextFile(hFind, &fnd));
	//    //            FindClose(hFind);
	//    //        }
	//    //    }
	//    //}
	//    free(mpsz_LogPackets); mpsz_LogPackets = NULL;
	//}
}

//void CRealConsole::LogPacket(CESERVER_REQ* pInfo)
//{
//    if (!mpsz_LogPackets || m_UseLogs<3) return;
//
//    wchar_t szPath[MAX_PATH];
//    swprintf_c(szPath, mpsz_LogPackets, ++mn_LogPackets);
//
//    HANDLE hFile = CreateFile(szPath, GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
//    if (hFile != INVALID_HANDLE_VALUE) {
//        DWORD dwSize = 0;
//        WriteFile(hFile, pInfo, pInfo->hdr.cbSize, &dwSize, 0);
//        CloseHandle(hFile);
//    }
//}



// ������� � ������� ������ �� ��������
BOOL CRealConsole::RecreateProcess(RConStartArgs *args)
{
	if (!this)
		return false;

	_ASSERTE(hConWnd && mh_ConEmuC);

	if (!hConWnd || !mh_ConEmuC)
	{
		Box(_T("Console was not created (CRealConsole::SetConsoleSize)"));
		return false; // ������� ���� �� �������?
	}

	if (mn_InRecreate)
	{
		Box(_T("Console already in recreate..."));
		return false;
	}

	if (args->pszSpecialCmd && *args->pszSpecialCmd)
	{
		if (m_Args.pszSpecialCmd) Free(m_Args.pszSpecialCmd);

		int nLen = _tcslen(args->pszSpecialCmd);
		m_Args.pszSpecialCmd = (wchar_t*)Alloc(nLen+1,2);

		if (!m_Args.pszSpecialCmd)
		{
			Box(_T("Can't allocate memory..."));
			return FALSE;
		}

		lstrcpyW(m_Args.pszSpecialCmd, args->pszSpecialCmd);
	}

	if (args->pszStartupDir)
	{
		if (m_Args.pszStartupDir) Free(m_Args.pszStartupDir);

		int nLen = _tcslen(args->pszStartupDir);
		m_Args.pszStartupDir = (wchar_t*)Alloc(nLen+1,2);

		if (!m_Args.pszStartupDir)
			return FALSE;

		lstrcpyW(m_Args.pszStartupDir, args->pszStartupDir);
	}

	m_Args.bRunAsAdministrator = args->bRunAsAdministrator;
	//DWORD nWait = 0;
	mb_ProcessRestarted = FALSE;
	mn_InRecreate = GetTickCount();

	if (!mn_InRecreate)
	{
		DisplayLastError(L"GetTickCount failed");
		return false;
	}

	CloseConsole();
	// mb_InCloseConsole ������� ����� ����, ��� �������� ����� ����!
	//mb_InCloseConsole = FALSE;
	//if (con.pConChar && con.pConAttr)
	//{
	//	wmemset((wchar_t*)con.pConAttr, 7, con.nTextWidth * con.nTextHeight);
	//}
	SetConStatus(L"Restarting process...");
	return true;
}

// � ��������� �� ������
BOOL CRealConsole::RecreateProcessStart()
{
	bool lbRc = false;

	if (mn_InRecreate && mn_ProcessCount == 0 && !mb_ProcessRestarted)
	{
		mb_ProcessRestarted = TRUE;

		if ((mn_ProgramStatus & CES_NTVDM) == CES_NTVDM)
		{
			mn_ProgramStatus = 0; mb_IgnoreCmdStop = FALSE;

			// ��� ������������ ������������ 16������ �����, ����� ������������
			if (!PreInit())
				return FALSE;
		}
		else
		{
			mn_ProgramStatus = 0; mb_IgnoreCmdStop = FALSE;
		}

		StopThread(TRUE/*abRecreate*/);
		ResetEvent(mh_TermEvent);
		hConWnd = NULL;
		hGuiWnd = NULL;
		mn_GuiWndStyle = mn_GuiWndStylEx = mn_GuiWndPID;
		ms_VConServer_Pipe[0] = 0;
		SafeCloseHandle(mh_ServerSemaphore);
		SafeCloseHandle(mh_GuiAttached);

		if (!StartProcess())
		{
			mn_InRecreate = 0;
			mb_ProcessRestarted = FALSE;
		}
		else
		{
			ResetEvent(mh_TermEvent);
			lbRc = StartMonitorThread();
		}
	}

	return lbRc;
}

BOOL CRealConsole::IsConsoleDataChanged()
{
	if (!this) return FALSE;

	#ifdef _DEBUG
	if (mb_DebugLocked)
		return FALSE;
	#endif
	
	WARNING("����� ����� ������ - ���� ������� TRUE!");
	
	return mb_ABufChaged || mp_ABuf->isConsoleDataChanged();
}

bool CRealConsole::IsFarHyperlinkAllowed()
{
	if (!isFar(TRUE))
		return false;
	if (!gpSet->isFarGotoEditor)
		return false;
	if (gpSet->isFarGotoEditorVk && !isPressed(gpSet->isFarGotoEditorVk))
		return false;
	return true;
}

//CRealConsole::ExpandTextRangeType CRealConsole::ExpandTextRange(COORD& crFrom/*[In/Out]*/, COORD& crTo/*[Out]*/, CRealConsole::ExpandTextRangeType etr, wchar_t* pszText /*= NULL*/, size_t cchTextMax /*= 0*/)
//{
//}

BOOL CRealConsole::GetConsoleLine(int nLine, wchar_t** pChar, /*CharAttr** pAttr,*/ int* pLen, MSectionLock* pcsData)
{
	return mp_ABuf->GetConsoleLine(nLine, pChar, pLen, pcsData);
}

// nWidth � nHeight ��� �������, ������� ����� �������� VCon (��� ����� ��� �� ������������ �� ���������?
void CRealConsole::GetConsoleData(wchar_t* pChar, CharAttr* pAttr, int nWidth, int nHeight)
{
	if (!this) return;

	if (mb_ABufChaged)
		mb_ABufChaged = false; // �������

	mp_ABuf->GetConsoleData(pChar, pAttr, nWidth, nHeight);
}

//#define PICVIEWMSG_SHOWWINDOW (WM_APP + 6)
//#define PICVIEWMSG_SHOWWINDOW_KEY 0x0101
//#define PICVIEWMSG_SHOWWINDOW_ASC 0x56731469

BOOL CRealConsole::ShowOtherWindow(HWND hWnd, int swShow, BOOL abAsync/*=TRUE*/)
{
	if ((IsWindowVisible(hWnd) == FALSE) == (swShow == SW_HIDE))
		return TRUE; // ��� ��� �������

	BOOL lbRc = FALSE;

	// ����������� ���������, ������� ������ ������� � ����
	//lbRc = apiShowWindow(hWnd, swShow);
	//
	//lbRc = ((IsWindowVisible(hWnd) == FALSE) == (swShow == SW_HIDE));
	//
	////if (!lbRc)
	//{
	//	DWORD dwErr = GetLastError();
	//
	//	if (dwErr == 0)
	//	{
	//		if ((IsWindowVisible(hWnd) == FALSE) == (swShow == SW_HIDE))
	//			lbRc = TRUE;
	//		else
	//			dwErr = 5; // ����������� ����� ������
	//	}
	//
	//	if (dwErr == 5 /*E_access*/)
		{
			//PostConsoleMessage(hWnd, WM_SHOWWINDOW, SW_SHOWNA, 0);
			CESERVER_REQ in;
			ExecutePrepareCmd(&in, CECMD_POSTCONMSG, sizeof(CESERVER_REQ_HDR) + sizeof(CESERVER_REQ_POSTMSG));
			// ����������, ���������
			in.Msg.bPost = abAsync;
			in.Msg.hWnd = hWnd;
			in.Msg.nMsg = WM_SHOWWINDOW;
			in.Msg.wParam = swShow; //SW_SHOWNA;
			in.Msg.lParam = 0;
			
			DWORD dwTickStart = timeGetTime();
			
			CESERVER_REQ *pOut = ExecuteSrvCmd(GetServerPID(), &in, ghWnd);
			
			gpSetCls->debugLogCommand(&in, FALSE, dwTickStart, timeGetTime()-dwTickStart, L"ExecuteSrvCmd", pOut);

			if (pOut) ExecuteFreeResult(pOut);

			lbRc = TRUE;
		}
		//else if (!lbRc)
		//{
		//	wchar_t szClass[64], szMessage[255];
		//
		//	if (!GetClassName(hWnd, szClass, 63))
		//		_wsprintf(szClass, SKIPLEN(countof(szClass)) L"0x%08X", (DWORD)hWnd); else szClass[63] = 0;
		//
		//	_wsprintf(szMessage, SKIPLEN(countof(szMessage)) L"Can't %s %s window!",
		//	          (swShow == SW_HIDE) ? L"hide" : L"show",
		//	          szClass);
		//	DisplayLastError(szMessage, dwErr);
		//}
	//}

	return lbRc;
}

BOOL CRealConsole::SetOtherWindowPos(HWND hWnd, HWND hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags)
{
	BOOL lbRc = SetWindowPos(hWnd, hWndInsertAfter, X, Y, cx, cy, uFlags);

	if (!lbRc)
	{
		DWORD dwErr = GetLastError();

		if (dwErr == 5 /*E_access*/)
		{
			CESERVER_REQ in;
			ExecutePrepareCmd(&in, CECMD_SETWINDOWPOS, sizeof(CESERVER_REQ_HDR) + sizeof(CESERVER_REQ_SETWINDOWPOS));
			// ����������, ���������
			in.SetWndPos.hWnd = hWnd;
			in.SetWndPos.hWndInsertAfter = hWndInsertAfter;
			in.SetWndPos.X = X;
			in.SetWndPos.Y = Y;
			in.SetWndPos.cx = cx;
			in.SetWndPos.cy = cy;
			in.SetWndPos.uFlags = uFlags;
			
			DWORD dwTickStart = timeGetTime();
			
			CESERVER_REQ *pOut = ExecuteSrvCmd(GetServerPID(), &in, ghWnd);
			
			gpSetCls->debugLogCommand(&in, FALSE, dwTickStart, timeGetTime()-dwTickStart, L"ExecuteSrvCmd", pOut);

			if (pOut) ExecuteFreeResult(pOut);

			lbRc = TRUE;
		}
		else
		{
			wchar_t szClass[64], szMessage[128];

			if (!GetClassName(hWnd, szClass, 63))
				_wsprintf(szClass, SKIPLEN(countof(szClass)) L"0x%08X", (DWORD)hWnd); else szClass[63] = 0; //-V205

			_wsprintf(szMessage, SKIPLEN(countof(szMessage)) L"SetWindowPos(%s) failed!", szClass);
			DisplayLastError(szMessage, dwErr);
		}
	}

	return lbRc;
}

BOOL CRealConsole::SetOtherWindowFocus(HWND hWnd, BOOL abSetForeground)
{
	BOOL lbRc = FALSE;
	DWORD dwErr = 0;
	HWND hLastFocus = NULL;

	if (!(m_Args.bRunAsAdministrator || m_Args.pszUserName || m_Args.bRunAsRestricted/*?*/))
	{
		if (abSetForeground)
		{
			lbRc = apiSetForegroundWindow(hWnd);
		}
		else
		{
			SetLastError(0);
			hLastFocus = SetFocus(hWnd);
			dwErr = GetLastError();
			lbRc = (dwErr == 0 /* != ERROR_ACCESS_DENIED {5}*/);
		}
	}
	else
	{
		lbRc = apiSetForegroundWindow(hWnd);
	}

	// -- ������ ���, �� ��������
	//if (!lbRc)
	//{
	//	CESERVER_REQ in;
	//	ExecutePrepareCmd(&in, CECMD_SETFOCUS, sizeof(CESERVER_REQ_HDR) + sizeof(CESERVER_REQ_SETFOCUS));
	//	// ����������, ���������
	//	in.setFocus.bSetForeground = abSetForeground;
	//	in.setFocus.hWindow = hWnd;
	//	CESERVER_REQ *pOut = ExecuteSrvCmd(GetServerPID(), &in, ghWnd);
	//	if (pOut) ExecuteFreeResult(pOut);
	//	lbRc = TRUE;
	//}

	return lbRc;
}

HWND CRealConsole::SetOtherWindowParent(HWND hWnd, HWND hParent)
{
	HWND h = NULL;
	DWORD dwErr = 0;

	SetLastError(0);
	h = SetParent(hWnd, hParent);
	if (h == NULL)
		dwErr = GetLastError();

	if (dwErr)
	{
		CESERVER_REQ in;
		ExecutePrepareCmd(&in, CECMD_SETPARENT, sizeof(CESERVER_REQ_HDR) + sizeof(CESERVER_REQ_SETPARENT));
		// ����������, ���������
		in.setParent.hWnd = hWnd;
		in.setParent.hParent = hParent;
		
		DWORD dwTickStart = timeGetTime();
		
		CESERVER_REQ *pOut = ExecuteSrvCmd(GetServerPID(), &in, ghWnd);
		
		gpSetCls->debugLogCommand(&in, FALSE, dwTickStart, timeGetTime()-dwTickStart, L"ExecuteSrvCmd", pOut);
		
		if (pOut)
		{
			h = pOut->setParent.hParent;
			ExecuteFreeResult(pOut);
		}
	}

	return h;
}

BOOL CRealConsole::SetOtherWindowRgn(HWND hWnd, int nRects, LPRECT prcRects, BOOL bRedraw)
{
	BOOL lbRc = FALSE;
	CESERVER_REQ in;
	ExecutePrepareCmd(&in, CECMD_SETWINDOWRGN, sizeof(CESERVER_REQ_HDR) + sizeof(CESERVER_REQ_SETWINDOWRGN));
	// ����������, ���������
	in.SetWndRgn.hWnd = hWnd;

	if (nRects <= 0 || !prcRects)
	{
		_ASSERTE(nRects==0 || nRects==-1); // -1 means reset rgn and hide window
		in.SetWndRgn.nRectCount = nRects;
		in.SetWndRgn.bRedraw = bRedraw;
	}
	else
	{
		in.SetWndRgn.nRectCount = nRects;
		in.SetWndRgn.bRedraw = bRedraw;
		memmove(in.SetWndRgn.rcRects, prcRects, nRects*sizeof(RECT));
	}
	
	DWORD dwTickStart = timeGetTime();

	CESERVER_REQ *pOut = ExecuteSrvCmd(GetServerPID(), &in, ghWnd);
	
	gpSetCls->debugLogCommand(&in, FALSE, dwTickStart, timeGetTime()-dwTickStart, L"ExecuteSrvCmd", pOut);

	if (pOut) ExecuteFreeResult(pOut);

	lbRc = TRUE;
	return lbRc;
}

void CRealConsole::ShowHideViews(BOOL abShow)
{
	// ��� ���� ������ ��������� � �������� ����, � ���������� ������ ���, ����� ����������
#if 0
	// �.�. apiShowWindow ����������, ���� ���� ������� �� ����� ������� ������������ (��� Run as admin)
	// �� ������� � ����������� ���� ������ ������ ��������
	HWND hPic = isPictureView();

	if (hPic)
	{
		if (abShow)
		{
			if (mb_PicViewWasHidden && !IsWindowVisible(hPic))
				ShowOtherWindow(hPic, SW_SHOWNA);

			mb_PicViewWasHidden = FALSE;
		}
		else
		{
			mb_PicViewWasHidden = TRUE;
			ShowOtherWindow(hPic, SW_HIDE);
		}
	}

	for(int p = 0; p <= 1; p++)
	{
		const PanelViewInit* pv = mp_VCon->GetPanelView(p==0);

		if (pv)
		{
			if (abShow)
			{
				if (pv->bVisible && !IsWindowVisible(pv->hWnd))
					ShowOtherWindow(pv->hWnd, SW_SHOWNA);
			}
			else
			{
				if (IsWindowVisible(pv->hWnd))
					ShowOtherWindow(pv->hWnd, SW_HIDE);
			}
		}
	}
#endif
}

void CRealConsole::OnActivate(int nNewNum, int nOldNum)
{
	if (!this)
		return;

	_ASSERTE(isActive());
	// ����� ����� ���� ����� ����� ���� �� ������ �������
	SetWindowLongPtr(ghWnd, GWLP_USERDATA, (LONG_PTR)hConWnd);
	ghConWnd = hConWnd;
	// ���������
	mp_VCon->OnAlwaysShowScrollbar();
	// ����� ��� � ����� ����� ����
	OnGuiFocused(TRUE, TRUE);
	//if (mp_ConsoleInfo) {
	//	PRAGMA_ERROR("����� ������ ����� ������ ����� ������� �������");
	//	mp_ConsoleInfo->bConsoleActive = TRUE;
	//}

	if ((gOSVer.dwMajorVersion > 6) || ((gOSVer.dwMajorVersion == 6) && (gOSVer.dwMinorVersion >= 1)))
		gpConEmu->Taskbar_SetShield(isAdministrator());

	//if (hGuiWnd)
	//{
	//	HWND hFore = GetForegroundWindow();
	//	DWORD nForePID = 0;
	//	if (hFore) GetWindowThreadProcessId(hFore, &nForePID);
	//	if (mn_GuiWndPID != nForePID)
	//	{
	//		//SetOtherWindowFocus(hGuiWnd, FALSE/*use SetFocus*/);
	//		SetFocus(hGuiWnd);
	//		//PostConsoleMessage(hConWnd, WM_SETFOCUS, NULL, NULL);
	//	}
	//}


	//if (mh_MonitorThread) SetThreadPriority(mh_MonitorThread, THREAD_PRIORITY_ABOVE_NORMAL);

	if ((gpSet->isMonitorConsoleLang & 2) == 2)  // ���� Layout �� ��� �������
		SwitchKeyboardLayout(INPUTLANGCHANGE_SYSCHARSET,gpConEmu->GetActiveKeyboardLayout());
	else if (mp_RBuf->GetKeybLayout() && (gpSet->isMonitorConsoleLang & 1) == 1)  // ������� �� Layout'�� � �������
		gpConEmu->SwitchKeyboardLayout(mp_RBuf->GetKeybLayout());

	WARNING("�� �������� ���������� ���������");
	gpConEmu->UpdateTitle();
	UpdateScrollInfo();
	gpConEmu->mp_TabBar->OnConsoleActivated(nNewNum+1/*, isBufferHeight()*/);
	gpConEmu->mp_TabBar->Update();
	gpConEmu->OnBufferHeight();
	gpConEmu->UpdateProcessDisplay(TRUE);
	//gpSet->NeedBackgroundUpdate(); -- 111105 ���������� �������� ������ � VCon, � �������� - ��� ����� �����, ������� �� �����
	ShowHideViews(TRUE);
	//HWND hPic = isPictureView();
	//if (hPic && mb_PicViewWasHidden) {
	//	if (!IsWindowVisible(hPic)) {
	//		if (!apiShowWindow(hPic, SW_SHOWNA)) {
	//			DisplayLastError(L"Can't show PictireView window!");
	//		}
	//	}
	//}
	//mb_PicViewWasHidden = FALSE;

	if (ghOpWnd && isActive())
		gpSetCls->UpdateConsoleMode(mp_RBuf->GetConMode());

	if (isActive())
	{
		gpConEmu->UpdateActiveGhost(mp_VCon);
		gpConEmu->OnSetCursor(-1,-1);
		gpConEmu->UpdateWindowRgn();
	}
}

void CRealConsole::OnDeactivate(int nNewNum)
{
	if (!this) return;
	
	mp_VCon->SavePaneSnapshoot();

	ShowHideViews(FALSE);
	//HWND hPic = isPictureView();
	//if (hPic && IsWindowVisible(hPic)) {
	//    mb_PicViewWasHidden = TRUE;
	//	if (!apiShowWindow(hPic, SW_HIDE)) {
	//		DisplayLastError(L"Can't hide PictuteView window!");
	//	}
	//}

	// 111125 � ����� ��������� ���������� ��� �����������?
	//if (con.m_sel.dwFlags & CONSOLE_MOUSE_SELECTION)
	//{
	//	con.m_sel.dwFlags &= ~CONSOLE_MOUSE_SELECTION;
	//	//ReleaseCapture();
	//}

	// ����� ��� � ����� ����� ����
	OnGuiFocused(FALSE);
	//if (mp_ConsoleInfo) {
	//	PRAGMA_ERROR("����� ������ ����� ������ ����� ������� �������");
	//	mp_ConsoleInfo->bConsoleActive = FALSE;
	//}
	//if (mh_MonitorThread) SetThreadPriority(mh_MonitorThread, THREAD_PRIORITY_NORMAL);

	gpConEmu->setFocus();
}

void CRealConsole::OnGuiFocused(BOOL abFocus, BOOL abForceChild /*= FALSE*/)
{
	if (!this)
		return;
	if (mb_InSetFocus)
		return;

	mb_InSetFocus = TRUE;

	if (abFocus)
	{
		mp_VCon->OnTaskbarFocus();

		if (hGuiWnd)
		{
			if (abForceChild)
			{
				HWND hFore = GetForegroundWindow();
				DWORD nForePID = 0;
				if (hFore) GetWindowThreadProcessId(hFore, &nForePID);
				if (mn_GuiWndPID != nForePID /*|| abForceChild*/)
				{
					//SetOtherWindowFocus(hGuiWnd, FALSE/*use SetFocus*/);
					//--SetFocus(hGuiWnd);
					//PostConsoleMessage(hConWnd, WM_SETFOCUS, NULL, NULL);
				}
			}
			SendMessage(ghWnd, WM_NCACTIVATE, TRUE, 0);
		}
		else
		{
			// -- �� ����� ���� ���� �������� - ��������, �� ������������ ������ Settings ������� �� TaskBar-�
			// gpConEmu->setFocus();
		}
	}

	// ���� FALSE - ������ ����������� �������� ������ ������� (GUI ������ �����)
	mb_ThawRefreshThread = abFocus || !gpSet->isSleepInBackground;

	// �������� "���������" ��������� ���� � ������� hdr.bConsoleActive � ��������
	if (m_ConsoleMap.IsValid() && ms_ConEmuC_Pipe[0])
	{
		BOOL lbNeedChange = FALSE;
		BOOL lbActive = isActive();

		if ((BOOL)m_ConsoleMap.Ptr()->bConsoleActive == lbActive
		     && (BOOL)m_ConsoleMap.Ptr()->bThawRefreshThread == mb_ThawRefreshThread)
		{
			lbNeedChange = FALSE;
		}
		else
		{
			lbNeedChange = TRUE;
		}

		if (lbNeedChange)
			UpdateServerActive(lbActive);
	}

	mb_InSetFocus = FALSE;
}

// �������� � ������� ����� Active & ThawRefreshThread,
// � ������ ��������� ���������� ���������� ������� (���� abActive == TRUE)
void CRealConsole::UpdateServerActive(BOOL abActive)
{
	if (!this) return;

	BOOL fSuccess = FALSE;

	if (ms_ConEmuC_Pipe[0])
	{
		size_t nInSize = sizeof(CESERVER_REQ_HDR)+sizeof(DWORD)*2;
		DWORD dwRead = 0;
		//CESERVER_REQ lIn = {{nInSize}}, lOut = {};
		CESERVER_REQ* pIn = ExecuteNewCmd(CECMD_ONACTIVATION, nInSize);
		CESERVER_REQ* pOut = ExecuteNewCmd(CECMD_ONACTIVATION, sizeof(CESERVER_REQ));
		if (pIn && pOut)
		{
			pIn->dwData[0] = abActive;
			pIn->dwData[1] = mb_ThawRefreshThread;
			//ExecutePrepareCmd(&lIn.hdr, CECMD_ONACTIVATION, lIn.hdr.cbSize);
			DWORD dwTickStart = timeGetTime();
			// �������, ����� �� ���������
			fSuccess = CallNamedPipe(ms_ConEmuC_Pipe, pIn, pIn->hdr.cbSize, pOut, pOut->hdr.cbSize, &dwRead, 500);
			gpSetCls->debugLogCommand(pIn, FALSE, dwTickStart, timeGetTime()-dwTickStart, ms_ConEmuC_Pipe, pOut);
		}
		ExecuteFreeResult(pIn);
		ExecuteFreeResult(pOut);
	}
}

void CRealConsole::UpdateScrollInfo()
{
	if (!isActive())
		return;

	if (!gpConEmu->isMainThread())
	{
		gpConEmu->OnUpdateScrollInfo(FALSE/*abPosted*/);
		return;
	}

	CVConGuard guard(mp_VCon);

	WARNING("DoubleView: �������� static �� member");
	static SHORT nLastHeight = 0, nLastWndHeight = 0, nLastTop = 0;

	if (nLastHeight == mp_ABuf->GetBufferHeight()/*con.m_sbi.dwSize.Y*/
	        && nLastWndHeight == mp_ABuf->GetTextHeight()/*(con.m_sbi.srWindow.Bottom - con.m_sbi.srWindow.Top + 1)*/
	        && nLastTop == mp_ABuf->GetBufferPosY()/*con.m_sbi.srWindow.Top*/)
		return; // �� ��������

	nLastHeight = mp_ABuf->GetBufferHeight()/*con.m_sbi.dwSize.Y*/;
	nLastWndHeight = mp_ABuf->GetTextHeight()/*(con.m_sbi.srWindow.Bottom - con.m_sbi.srWindow.Top + 1)*/;
	nLastTop = mp_ABuf->GetBufferPosY()/*con.m_sbi.srWindow.Top*/;
	mp_VCon->SetScroll(mp_ABuf->isScroll()/*con.bBufferHeight*/, nLastTop, nLastWndHeight, nLastHeight);
}

void CRealConsole::SetTabs(ConEmuTab* tabs, int tabsCount)
{
#ifdef _DEBUG
	wchar_t szDbg[128];
	_wsprintf(szDbg, SKIPLEN(countof(szDbg)) L"CRealConsole::SetTabs.  ItemCount=%i, PrevItemCount=%i\n", tabsCount, mn_tabsCount);
	DEBUGSTRTABS(szDbg);
#endif
	ConEmuTab* lpTmpTabs = NULL;
	const size_t nMaxTabName = countof(tabs->Name);
	// ���� ����� ��������� � �����������
	int nActiveTab = 0, i;

	if (tabs && tabsCount)
	{
		_ASSERTE(tabs->Type>1 || !tabs->Modified);

		//int nNewSize = sizeof(ConEmuTab)*tabsCount;
		//lpNewTabs = (ConEmuTab*)Alloc(nNewSize, 1);
		//if (!lpNewTabs)
		//    return;
		//memmove ( lpNewTabs, tabs, nNewSize );

		// ����� ������ "Panels" ����� �� ��� � ��������� �������. �������� "edit - doc1.doc"
		// ��� ��������, � �������� ������������ ��������
		if (tabsCount > 1 && tabs[0].Type == 1/*WTYPE_PANELS*/ && tabs[0].Current)
			tabs[0].Name[0] = 0;

		if (tabsCount == 1)  // �������������. ����� ������ �� ����������
			tabs[0].Current = 1;

		// ����� �������� ��������
		for (i = (tabsCount-1); i >= 0; i--)
		{
			if (tabs[i].Current)
			{
				nActiveTab = i; break;
			}
		}

#ifdef _DEBUG

		// ���������: ����� �������� (����������/��������) ������ ���� ������!
		for(i=1; i<tabsCount; i++)
		{
			if (tabs[i].Name[0] == 0)
			{
				_ASSERTE(tabs[i].Name[0]!=0);
				//wcscpy(tabs[i].Name, gpConEmu->GetDefaultTitle());
			}
		}

#endif
	}
	else if (tabsCount == 1 && tabs == NULL)
	{
		lpTmpTabs = (ConEmuTab*)Alloc(sizeof(ConEmuTab),1);

		if (!lpTmpTabs)
			return;

		tabs = lpTmpTabs;
		tabs->Current = 1;
		tabs->Type = 1;
	}

	// ���� �������� ��� "���������������"
	if (tabsCount && isAdministrator() && (gpSet->bAdminShield || gpSet->szAdminTitleSuffix[0]))
	{
		// � ������ - ������� �� �������� (���� ������������ ��� ������) ��� ��������� (����������� � GetTab)
		//if (gpSet->bAdminShield)
		{
			for (i = 0; i < tabsCount; i++)
			{
				tabs[i].Type |= 0x100;
			}
		}
		//else
		//{
		//	// ����� - ��������� (���� �� �����)
		//	size_t nAddLen = _tcslen(gpSet->szAdminTitleSuffix) + 1;
		//	for(i=0; i<tabsCount; i++)
		//	{
		//		if (tabs[i].Name[0])
		//		{
		//			// ���� ���� �����
		//			if (_tcslen(tabs[i].Name) < (nMaxTabName + nAddLen))
		//				lstrcat(tabs[i].Name, gpSet->szAdminTitleSuffix);
		//		}
		//	}
		//}
	}

	if (tabsCount != mn_tabsCount)
		mb_TabsWasChanged = TRUE;

	MSectionLock SC;

	if (tabsCount > mn_MaxTabs)
	{
		SC.Lock(&msc_Tabs, TRUE);
		mn_tabsCount = 0; Free(mp_tabs); mp_tabs = NULL;
		mn_MaxTabs = tabsCount*2+10;
		mp_tabs = (ConEmuTab*)Alloc(mn_MaxTabs,sizeof(ConEmuTab));
		mb_TabsWasChanged = TRUE;
	}
	else
	{
		SC.Lock(&msc_Tabs, FALSE);
	}

	for (i = 0; i < tabsCount; i++)
	{
		if (!mb_TabsWasChanged)
		{
			if (mp_tabs[i].Current != tabs[i].Current
			        || mp_tabs[i].Type != tabs[i].Type
			        || mp_tabs[i].Modified != tabs[i].Modified
			  )
				mb_TabsWasChanged = TRUE;
			else if (wcscmp(mp_tabs[i].Name, tabs[i].Name)!=0)
				mb_TabsWasChanged = TRUE;
		}

		mp_tabs[i] = tabs[i];
	}

	mn_tabsCount = tabsCount; mn_ActiveTab = nActiveTab;
	SC.Unlock(); // ������ �� �����

	//if (mb_TabsWasChanged && mp_tabs && mn_tabsCount) {
	//    CheckPanelTitle();
	//    CheckFarStates();
	//    FindPanels();
	//}

	// ����������� gpConEmu->mp_TabBar->..
	if (gpConEmu->isValid(mp_VCon))    // �� ����� �������� ������� ��� ��� �� ��������� � ������...
	{
		// �� ����� ��������� ��������� - �����������
		gpConEmu->mp_TabBar->SetRedraw(TRUE);
		gpConEmu->mp_TabBar->Update();
	}
}

int CRealConsole::GetTabCount(BOOL abVisibleOnly /*= FALSE*/)
{
	if (!this)
		return 0;

	#ifdef HT_CONEMUTAB
	WARNING("����� �������� �� ����� ���� �������� � ��, ������� ������ ����������");
	if (gpSet->isTabsInCaption)
	{
		_ASSERTE(FALSE);
	}
	#endif
	
	if (abVisibleOnly)
	{
		// ���� �� ����� ���������� ��� ��������� ���������/�������, � ������ �������� ����
		if (!gpSet->bShowFarWindows)
			return 1;
	}
	
	if (((mn_ProgramStatus & CES_FARACTIVE) == 0))
		return 1; // �� ����� ���������� ������ - ������ ���� ��������

	TODO("���������� gpSet->bHideDisabledTabs, ����� �����-�� ���� ��������");
	//if (mn_tabsCount > 1 && gpSet->bHideDisabledConsoleTabs)
	//{
	//	int nCount = 0;
	//	for (int i = 0; i < mn_tabsCount; i++)
	//	{
	//		if (CanActivateFarWindow(i))
	//			nCount++;
	//	}
	//	if (nCount == 0)
	//	{
	//		_ASSERTE(nCount>0);
	//		nCount = 1;
	//	}
	//	return nCount;
	//}

	return max(mn_tabsCount,1);
}

int CRealConsole::GetActiveTab()
{
	if (!this)
		return 0;

	int nCount = GetTabCount();
	return (mn_ActiveTab < nCount) ? mn_ActiveTab : 0;
}

void CRealConsole::UpdateTabFlags(/*IN|OUT*/ ConEmuTab* pTab)
{
	if (isAdministrator() && (gpSet->bAdminShield || gpSet->szAdminTitleSuffix[0]))
	{
		if (gpSet->bAdminShield)
		{
			pTab->Type |= 0x100;
		}
		else
		{
			INT_PTR nMaxLen = min(countof(TitleFull), countof(pTab->Name));
			if ((INT_PTR)(_tcslen(pTab->Name) + _tcslen(gpSet->szAdminTitleSuffix)) < nMaxLen)
				_wcscat_c(pTab->Name, nMaxLen, gpSet->szAdminTitleSuffix);
		}
	}
}

// ���� ������ ���� ��� - pTab �� ��������!!!
bool CRealConsole::GetTab(int tabIdx, /*OUT*/ ConEmuTab* pTab)
{
	if (!this)
		return false;

	//if (hGuiWnd)
	//{
	//	if (tabIdx == 0)
	//	{
	//		pTab->Pos = 0; pTab->Current = 1; pTab->Type = 1; pTab->Modified = 0;
	//		int nMaxLen = min(countof(TitleFull) , countof(pTab->Name));
	//		GetWindowText(hGuiWnd, pTab->Name, nMaxLen);
	//		return true;
	//	}
	//	return false;
	//}

	if (!mp_tabs || tabIdx<0 || tabIdx>=mn_tabsCount || hGuiWnd)
	{
		// �� ������ ������, ���� ���� ���� �� ����������������, � ������ ������ -
		// ������ ������ ��������� �������
		if (tabIdx == 0)
		{
			pTab->Pos = 0; pTab->Current = 1; pTab->Type = 1; pTab->Modified = 0;
			int nMaxLen = min(countof(TitleFull) , countof(pTab->Name));
			lstrcpyn(pTab->Name, TitleFull, nMaxLen);
			UpdateTabFlags(pTab);
			return true;
		}

		return false;
	}

	// �� ����� ���������� DOS-������ - ������ ���� ���
	if ((mn_ProgramStatus & CES_FARACTIVE) == 0 && tabIdx > 0)
		return false;

	memmove(pTab, mp_tabs+tabIdx, sizeof(ConEmuTab));

	// ���� ������ ������������ - ����� ���������� ��������� �������
	if (((mn_tabsCount == 1) || (mn_ProgramStatus & CES_FARACTIVE) == 0) && pTab->Type == 1)
	{
		// � ���� ��������� �������
		if (TitleFull[0])  
		{
			int nMaxLen = min(countof(TitleFull) , countof(pTab->Name));
			lstrcpyn(pTab->Name, TitleFull, nMaxLen);
		}
	}

	if (pTab->Name[0] == 0)
	{
		if (ms_PanelTitle[0])  // ������ ����� - ��� Panels?
		{
			// 03.09.2009 Maks -> min
			int nMaxLen = min(countof(ms_PanelTitle) , countof(pTab->Name));
			lstrcpyn(pTab->Name, ms_PanelTitle, nMaxLen);

			//if (isAdministrator() && (gpSet->bAdminShield || gpSet->szAdminTitleSuffix[0]))
			//{
			//	if (gpSet->bAdminShield)
			//	{
			//		pTab->Type |= 0x100;
			//	}
			//	else
			//	{
			//		if ((INT_PTR)_tcslen(ms_PanelTitle) < (INT_PTR)(countof(pTab->Name) + _tcslen(gpSet->szAdminTitleSuffix) + 1))
			//			lstrcat(pTab->Name, gpSet->szAdminTitleSuffix);
			//	}
			//}
		}
		else if (mn_tabsCount == 1 && TitleFull[0])  // ���� ������ ������������ - ����� ���������� ��������� �������
		{
			// 03.09.2009 Maks -> min
			int nMaxLen = min(countof(TitleFull) , countof(pTab->Name));
			lstrcpyn(pTab->Name, TitleFull, nMaxLen);
		}
	}

	//if (tabIdx == 0 && isAdministrator() && gpSet->bAdminShield)
	//{
	//	pTab->Type |= 0x100;
	//}

	wchar_t* pszAmp = pTab->Name;
	int nCurLen = _tcslen(pTab->Name), nMaxLen = countof(pTab->Name)-1;
	
	#ifdef HT_CONEMUTAB
	WARNING("����� �������� ����� �� ������ ��������� - ��� ����� � ������������ ����� ����� ������");
	if (gpSet->isTabsInCaption)
	{
		_ASSERTE(FALSE);
	}
	#endif

	while((pszAmp = wcschr(pszAmp, L'&')) != NULL)
	{
		if (nCurLen >= (nMaxLen - 2))
		{
			*pszAmp = L'_';
			pszAmp ++;
		}
		else
		{
			size_t nMove = nCurLen-(pszAmp-pTab->Name)+1; // right part of string + \0
			wmemmove_s(pszAmp+1, nMove, pszAmp, nMove);
			nCurLen ++;
			pszAmp += 2;
		}
	}

	UpdateTabFlags(pTab);

	return true;
}

int CRealConsole::GetModifiedEditors()
{
	int nEditors = 0;
	
	if (mp_tabs && mn_tabsCount)
	{
		for(int j = 0; j < mn_tabsCount; j++)
		{
			if ((mp_tabs[j].Type == /*Editor*/3) && (mp_tabs[j].Modified != 0))
				nEditors++;
		}
	}

	return nEditors;
}

void CRealConsole::CheckPanelTitle()
{
#ifdef _DEBUG

	if (mb_DebugLocked)
		return;

#endif

	if (mp_tabs && mn_tabsCount)
	{
		if ((mn_ActiveTab >= 0 && mn_ActiveTab < mn_tabsCount) || mn_tabsCount == 1)
		{
			WCHAR szPanelTitle[CONEMUTABMAX];

			if (!GetWindowText(hConWnd, szPanelTitle, countof(ms_PanelTitle)-1))
				ms_PanelTitle[0] = 0;
			else if (szPanelTitle[0] == L'{' || szPanelTitle[0] == L'(')
				lstrcpy(ms_PanelTitle, szPanelTitle);
		}
	}
}

DWORD CRealConsole::CanActivateFarWindow(int anWndIndex)
{
	if (!this)
		return 0;

	DWORD dwPID = GetFarPID();

	if (!dwPID)
	{
		return -1; // ������� ������������ ��� ������� �� �������� (���� ���)
		//if (anWndIndex == mn_ActiveTab)
		//return 0;
	}

	// ���� ���� ����: ucBoxDblDownRight ucBoxDblHorz ucBoxDblHorz ucBoxDblHorz (������ ��� ������ ������ �������) - �������
	// ���� ���� ������� (� ��������� ������� {n%}) - �������
	// ���� ����� ������ - ������� (������ ���������� ��� ������)

	if (anWndIndex<0 || anWndIndex>=mn_tabsCount)
	{
		AssertCantActivate((anWndIndex>=0 && anWndIndex<mn_tabsCount));
		return 0;
	}

	// ������� ����� ����������. �� ����, � ��� ������ ������ ���� ���������� ����� �������� ����.
	if (mn_ActiveTab == anWndIndex)
		return (DWORD)-1; // ������ ���� ��� ��������, ����� �� ���������...

	if (isPictureView(TRUE))
	{
		AssertCantActivate("isPictureView"==NULL);
		return 0; // ��� ������� PictureView ������������� �� ������ ��� ���� ������� �� ���������
	}

	if (!GetWindowText(hConWnd, TitleCmp, countof(TitleCmp)-2))
		TitleCmp[0] = 0;

	// �������� ��� ����������� � ������ �����
	if (GetProgress(NULL)>=0)
	{
		AssertCantActivate("GetProgress>0"==NULL);
		return 0; // ���� ����������� ��� �����-�� ������ ��������
	}

	//// ����������� � FAR: "{33%}..."
	////2009-06-02: PPCBrowser ���������� ����������� ���: "(33% 00:02:20)..."
	//if ((TitleCmp[0] == L'{' || TitleCmp[0] == L'(')
	//    && isDigit(TitleCmp[1]) &&
	//    ((TitleCmp[2] == L'%' /*&& TitleCmp[3] == L'}'*/) ||
	//     (isDigit(TitleCmp[2]) && TitleCmp[3] == L'%' /*&& TitleCmp[4] == L'}'*/) ||
	//     (isDigit(TitleCmp[2]) && isDigit(TitleCmp[3]) && TitleCmp[4] == L'%' /*&& TitleCmp[5] == L'}'*/))
	//   )
	//{
	//    // ���� �����������
	//    return 0;
	//}

	if (!mp_RBuf->isInitialized())
	{
		AssertCantActivate("Buf.isInitiazed"==NULL);
		return 0; // ������� �� ����������������, ������ ������
	}
		
	if (mp_RBuf != mp_ABuf)
	{
		AssertCantActivate("mp_RBuf != mp_ABuf"==NULL);
		return 0; // ���� ����������� ���.����� - ������ ���� ������
	}

	BOOL lbMenuActive = FALSE;

	if (mp_tabs && mn_ActiveTab >= 0 && mn_ActiveTab < mn_tabsCount)
	{
		// ���� ����� ���� ������ � �������
		if (mp_tabs[mn_ActiveTab].Type == 1/*WTYPE_PANELS*/)
		{
			lbMenuActive = mp_RBuf->isFarMenuActive();
		}
	}

	// ���� ������ ���� ������������ ������:
	//  0-������ ���������� � "  " ��� � "R   " ���� ���� ������ �������
	//  1-� ������ ucBoxDblDownRight ucBoxDblHorz ucBoxDblHorz ��� "[0+1]" ucBoxDblHorz ucBoxDblHorz
	//  2-� ������ ucBoxDblVert
	// ������� ��������� ���� ���������� �� ���������� ������ � ������ ������.
	// ���������� ���� ������������ ������ ����� ������ - � �������� �������������� ������ � ��������� �����

	if (lbMenuActive)
	{
		AssertCantActivate(lbMenuActive==FALSE);
		return 0;
	}

	// ���� ����� ������ - �� ���� ������������� �� �����
	if (mp_ABuf && (mp_ABuf->GetDetector()->GetFlags() & FR_FREEDLG_MASK))
	{
		AssertCantActivate("FR_FREEDLG_MASK"==NULL);
		return 0;
	}

	AssertCantActivate(dwPID!=0);
	return dwPID;
}

BOOL CRealConsole::ActivateFarWindow(int anWndIndex)
{
	if (!this)
		return FALSE;

	DWORD dwPID = CanActivateFarWindow(anWndIndex);

	if (!dwPID)
	{
		return FALSE;
	}
	else if (dwPID == (DWORD)-1)
	{
		return TRUE; // ������ ���� ��� ��������, ����� �� ���������...
	}

	BOOL lbRc = FALSE;
	DWORD nWait = -1;
	CConEmuPipe pipe(dwPID, 100);

	if (pipe.Init(_T("CRealConsole::ActivateFarWindow")))
	{
		DWORD nData[2] = {anWndIndex,0};

		// ���� � ������� ����� QSearch - ��� ����� �������������� "�����"
		if (!mn_ActiveTab && (mp_ABuf && (mp_ABuf->GetDetector()->GetFlags() & FR_QSEARCH)))
			nData[1] = TRUE;

		DEBUGSTRCMD(L"GUI send CMD_SETWINDOW\n");	
		if (pipe.Execute(CMD_SETWINDOW, nData, 8))
		{
			DEBUGSTRCMD(L"CMD_SETWINDOW executed\n");	

			WARNING("CMD_SETWINDOW �� �������� ���������� ��������� ��������� ��������� ���� (gpTabs).");
			// �� ���� ���� ������������ ���� ����������� ������ 2� ��� - ����������� ���������� ���������
			DWORD cbBytesRead=0;
			//DWORD tabCount = 0, nInMacro = 0, nTemp = 0, nFromMainThread = 0;
			ConEmuTab* tabs = NULL;
			CESERVER_REQ_CONEMUTAB TabHdr;
			DWORD nHdrSize = sizeof(CESERVER_REQ_CONEMUTAB) - sizeof(TabHdr.tabs);

			//if (pipe.Read(&tabCount, sizeof(DWORD), &cbBytesRead) &&
			//	pipe.Read(&nInMacro, sizeof(DWORD), &nTemp) &&
			//	pipe.Read(&nFromMainThread, sizeof(DWORD), &nTemp)
			//	)
			if (pipe.Read(&TabHdr, nHdrSize, &cbBytesRead))
			{
				tabs = (ConEmuTab*)pipe.GetPtr(&cbBytesRead);
				_ASSERTE(cbBytesRead==(TabHdr.nTabCount*sizeof(ConEmuTab)));

				if (cbBytesRead == (TabHdr.nTabCount*sizeof(ConEmuTab)))
				{
					SetTabs(tabs, TabHdr.nTabCount);
					lbRc = TRUE;
				}

				MCHKHEAP;
			}

			pipe.Close();
			// ������ ����� ����������� ������, ����� �� ��������� ���������� �������
			UpdateServerActive(TRUE);
			// � MonitorThread, ����� �� ������� ����� ����������
			ResetEvent(mh_ApplyFinished);
			mn_LastConsolePacketIdx--;
			SetEvent(mh_MonitorThreadEvent);
			nWait = WaitForSingleObject(mh_ApplyFinished, SETSYNCSIZEAPPLYTIMEOUT);
		}
	}

	return lbRc;
}

BOOL CRealConsole::IsConsoleThread()
{
	if (!this) return FALSE;

	DWORD dwCurThreadId = GetCurrentThreadId();
	return dwCurThreadId == mn_MonitorThreadID;
}

void CRealConsole::SetForceRead()
{
	SetEvent(mh_MonitorThreadEvent);
}

// ���������� �� TabBar->ConEmu
void CRealConsole::ChangeBufferHeightMode(BOOL abBufferHeight)
{
	if (!this)
		return;
		
	TODO("��� �� �� ������ ������, � ��������� ������� ������ �� ������� ����� ��������� �������");
	// ����, ��� �������������, ������� ��� ����
	_ASSERTE(mp_ABuf==mp_RBuf);
	mp_ABuf->ChangeBufferHeightMode(abBufferHeight);
}

#if 0
void CRealConsole::SetBufferHeightMode(BOOL abBufferHeight, BOOL abIgnoreLock/*=FALSE*/)
{
	if (!this)
		return;
		
	if (hGuiWnd)
	{
		// ������ �� ������� ������ ������� ShowOtherWindow (CConEmuMain::AskChangeBufferHeight)
		// � ��� ������� ������������� ��� ��������� ���������� � ������!
		_ASSERTE(hGuiWnd==NULL);
		//ShowOtherWindow(hGuiWnd, abBufferHeight ? SW_HIDE : SW_SHOW);
		//mp_VCon->Invalidate();
		return;
	}

	_ASSERTE(mp_ABuf==mp_RBuf);
	mp_ABuf->SetBufferHeightMode(abBufferHeight, abIgnoreLock);
}
#endif

HANDLE CRealConsole::PrepareOutputFileCreate(wchar_t* pszFilePathName)
{
	wchar_t szTemp[200];
	HANDLE hFile = NULL;

	if (GetTempPath(200, szTemp))
	{
		if (GetTempFileName(szTemp, L"CEM", 0, pszFilePathName))
		{
			hFile = CreateFile(pszFilePathName, GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);

			if (hFile == INVALID_HANDLE_VALUE)
			{
				pszFilePathName[0] = 0;
				hFile = NULL;
			}
		}
	}

	return hFile;
}

BOOL CRealConsole::PrepareOutputFile(BOOL abUnicodeText, wchar_t* pszFilePathName)
{
	BOOL lbRc = FALSE;
	CESERVER_REQ_HDR In = {0};
	const CESERVER_REQ *pOut = NULL;
	MPipe<CESERVER_REQ_HDR,CESERVER_REQ> Pipe;
	_ASSERTE(sizeof(In)==sizeof(CESERVER_REQ_HDR));
	ExecutePrepareCmd(&In, CECMD_GETOUTPUT, sizeof(CESERVER_REQ_HDR));
	Pipe.InitName(gpConEmu->GetDefaultTitle(), L"%s", ms_ConEmuC_Pipe, 0);

	if (!Pipe.Transact(&In, In.cbSize, &pOut))
	{
		MBoxA(Pipe.GetErrorText());
		return FALSE;
	}

	//HANDLE hPipe = NULL;
	//
	//CESERVER_REQ *pOut = NULL;
	//BYTE cbReadBuf[512];
	//BOOL fSuccess;
	//DWORD cbRead, dwMode, dwErr;
	//
	//// Try to open a named pipe; wait for it, if necessary.
	//while (1)
	//{
	//    hPipe = CreateFile(
	//        ms_ConEmuC_Pipe,// pipe name
	//        GENERIC_READ |  // read and write access
	//        GENERIC_WRITE,
	//        0,              // no sharing
	//        NULL,           // default security attributes
	//        OPEN_EXISTING,  // opens existing pipe
	//        0,              // default attributes
	//        NULL);          // no template file
	//
	//    // Break if the pipe handle is valid.
	//    if (hPipe != INVALID_HANDLE_VALUE)
	//        break;
	//
	//    // Exit if an error other than ERROR_PIPE_BUSY occurs.
	//    dwErr = GetLastError();
	//    if (dwErr != ERROR_PIPE_BUSY)
	//    {
	//        TODO("���������, ���� �������� ���� � ����� ������, �� ������ ���� ��� mh_ConEmuC");
	//        dwErr = WaitForSingleObject(mh_ConEmuC, 100);
	//        if (dwErr == WAIT_OBJECT_0)
	//            return FALSE;
	//        continue;
	//        //DisplayLastError(L"Could not open pipe", dwErr);
	//        //return 0;
	//    }
	//
	//    // All pipe instances are busy, so wait for 1 second.
	//    if (!WaitNamedPipe(ms_ConEmuC_Pipe, 1000) )
	//    {
	//        dwErr = WaitForSingleObject(mh_ConEmuC, 100);
	//        if (dwErr == WAIT_OBJECT_0) {
	//            DEBUGSTRCMD(L" - FAILED!\n");
	//            return FALSE;
	//        }
	//        //DisplayLastError(L"WaitNamedPipe failed");
	//        //return 0;
	//    }
	//}
	//
	//// The pipe connected; change to message-read mode.
	//dwMode = PIPE_READMODE_MESSAGE;
	//fSuccess = SetNamedPipeHandleState(
	//    hPipe,    // pipe handle
	//    &dwMode,  // new pipe mode
	//    NULL,     // don't set maximum bytes
	//    NULL);    // don't set maximum time
	//if (!fSuccess)
	//{
	//    DEBUGSTRCMD(L" - FAILED!\n");
	//    DisplayLastError(L"SetNamedPipeHandleState failed");
	//    CloseHandle(hPipe);
	//    return 0;
	//}
	//
	//ExecutePrepareCmd((CESERVER_REQ*)&in.hdr, CECMD_GETOUTPUT, sizeof(CESERVER_REQ_HDR));
	//
	//// Send a message to the pipe server and read the response.
	//fSuccess = TransactNamedPipe(
	//    hPipe,                  // pipe handle
	//    &in,                    // message to server
	//    in.nSize,               // message length
	//    cbReadBuf,              // buffer to receive reply
	//    sizeof(cbReadBuf),      // size of read buffer
	//    &cbRead,                // bytes read
	//    NULL);                  // not overlapped
	//
	//if (!fSuccess && (GetLastError() != ERROR_MORE_DATA))
	//{
	//    DEBUGSTRCMD(L" - FAILED!\n");
	//    //DisplayLastError(L"TransactNamedPipe failed");
	//    CloseHandle(hPipe);
	//    HANDLE hFile = PrepareOutputFileCreate(pszFilePathName);
	//    if (hFile)
	//        CloseHandle(hFile);
	//    return (hFile!=NULL);
	//}
	//
	//// ���� �� ������� ������, ������ ��������� �� �����
	//pOut = (CESERVER_REQ*)cbReadBuf;
	//if (pOut->hdr.nVersion != CESERVER_REQ_VER) {
	//    gpConEmu->ShowOldCmdVersion(pOut->hdr.nCmd, pOut->hdr.nVersion, pOut->hdr.nSrcPID==GetServerPID() ? 1 : 0);
	//    CloseHandle(hPipe);
	//    return 0;
	//}
	//
	//int nAllSize = pOut->hdr.cbSize;
	//if (nAllSize==0) {
	//    DEBUGSTRCMD(L" - FAILED!\n");
	//    DisplayLastError(L"Empty data recieved from server", 0);
	//    CloseHandle(hPipe);
	//    return 0;
	//}
	//if (nAllSize > (int)sizeof(cbReadBuf))
	//{
	//    pOut = (CESERVER_REQ*)calloc(nAllSize,1);
	//    _ASSERTE(pOut!=NULL);
	//    memmove(pOut, cbReadBuf, cbRead);
	//    _ASSERTE(pOut->hdr.nVersion==CESERVER_REQ_VER);
	//
	//    LPBYTE ptrData = ((LPBYTE)pOut)+cbRead;
	//    nAllSize -= cbRead;
	//
	//    while(nAllSize>0)
	//    {
	//        //_tprintf(TEXT("%s\n"), chReadBuf);
	//
	//        // Break if TransactNamedPipe or ReadFile is successful
	//        if (fSuccess)
	//            break;
	//
	//        // Read from the pipe if there is more data in the message.
	//        fSuccess = ReadFile(
	//            hPipe,      // pipe handle
	//            ptrData,    // buffer to receive reply
	//            nAllSize,   // size of buffer
	//            &cbRead,    // number of bytes read
	//            NULL);      // not overlapped
	//
	//        // Exit if an error other than ERROR_MORE_DATA occurs.
	//        if ( !fSuccess && (GetLastError() != ERROR_MORE_DATA))
	//            break;
	//        ptrData += cbRead;
	//        nAllSize -= cbRead;
	//    }
	//
	//    TODO("����� ���������� ASSERT, ���� ������� ���� ������� � �������� ������");
	//    _ASSERTE(nAllSize==0);
	//}
	//
	//CloseHandle(hPipe);
	// ������ pOut �������� ������ ������ : CESERVER_CONSAVE
	HANDLE hFile = PrepareOutputFileCreate(pszFilePathName);
	lbRc = (hFile != NULL);

	if (pOut->hdr.nVersion == CESERVER_REQ_VER)
	{
		const CESERVER_CONSAVE* pSave = (CESERVER_CONSAVE*)pOut;
		UINT nWidth = pSave->hdr.sbi.dwSize.X;
		UINT nHeight = pSave->hdr.sbi.dwSize.Y;
		const wchar_t* pwszCur = pSave->Data;
		const wchar_t* pwszEnd = (const wchar_t*)(((LPBYTE)pOut)+pOut->hdr.cbSize);

		if (pOut->hdr.nVersion == CESERVER_REQ_VER && nWidth && nHeight && (pwszCur < pwszEnd))
		{
			DWORD dwWritten;
			char *pszAnsi = NULL;
			const wchar_t* pwszRn = NULL;

			if (!abUnicodeText)
			{
				pszAnsi = (char*)calloc(nWidth+1,1);
			}
			else
			{
				WORD dwTag = 0xFEFF; //BOM
				WriteFile(hFile, &dwTag, 2, &dwWritten, 0);
			}

			BOOL lbHeader = TRUE;

			for(UINT y = 0; y < nHeight && pwszCur < pwszEnd; y++)
			{
				UINT nCurLen = 0;
				pwszRn = pwszCur + nWidth - 1;

				while(pwszRn >= pwszCur && *pwszRn == L' ')
				{
					//*pwszRn = 0;
					pwszRn --;
				}

				nCurLen = pwszRn - pwszCur + 1;

				if (nCurLen > 0 || !lbHeader)    // ������ N ����� ���� ��� ������ - �� ����������
				{
					if (lbHeader)
					{
						lbHeader = FALSE;
					}
					else if (nCurLen == 0)
					{
						// ���� ���� ����� ��� - ������ ������ �� ������
						pwszRn = pwszCur + nWidth;

						while(pwszRn < pwszEnd && *pwszRn == L' ') pwszRn ++;

						if (pwszRn >= pwszEnd) break;  // ����������� ����� ������ ���
					}

					if (abUnicodeText)
					{
						if (nCurLen>0)
							WriteFile(hFile, pwszCur, nCurLen*2, &dwWritten, 0);

						WriteFile(hFile, L"\r\n", 2*sizeof(wchar_t), &dwWritten, 0); //-V112
					}
					else
					{
						nCurLen = WideCharToMultiByte(CP_ACP, 0, pwszCur, nCurLen, pszAnsi, nWidth, 0,0);

						if (nCurLen>0)
							WriteFile(hFile, pszAnsi, nCurLen, &dwWritten, 0);

						WriteFile(hFile, "\r\n", 2, &dwWritten, 0);
					}
				}

				pwszCur += nWidth;
			}
		}
	}

	if (hFile != NULL && hFile != INVALID_HANDLE_VALUE)
		CloseHandle(hFile);

	//if (pOut && (LPVOID)pOut != (LPVOID)cbReadBuf)
	//    free(pOut);
	return lbRc;
}

void CRealConsole::SwitchKeyboardLayout(WPARAM wParam, DWORD_PTR dwNewKeyboardLayout)
{
	if (!this) return;

	if (hGuiWnd && dwNewKeyboardLayout)
	{
		CESERVER_REQ *pIn = ExecuteNewCmd(CECMD_LANGCHANGE, sizeof(CESERVER_REQ_HDR) + sizeof(DWORD));
		if (pIn)
		{
			pIn->dwData[0] = (DWORD)dwNewKeyboardLayout;
			
			DWORD dwTickStart = timeGetTime();
			
			CESERVER_REQ *pOut = ExecuteHkCmd(mn_GuiWndPID, pIn, ghWnd);
			
			gpSetCls->debugLogCommand(pIn, FALSE, dwTickStart, timeGetTime()-dwTickStart, L"ExecuteHkCmd", pOut);
			
			ExecuteFreeResult(pOut);
			ExecuteFreeResult(pIn);
		}
	}

	if (ms_ConEmuC_Pipe[0] == 0) return;

	if (!hConWnd) return;

#ifdef _DEBUG
	WCHAR szMsg[255];
	_wsprintf(szMsg, SKIPLEN(countof(szMsg)) L"CRealConsole::SwitchKeyboardLayout(CP:%i, HKL:0x" WIN3264TEST(L"%08X",L"%X%08X") L")\n",
	          (DWORD)wParam, WIN3264WSPRINT(dwNewKeyboardLayout));
	DEBUGSTRLANG(szMsg);
#endif

	if (gpSetCls->isAdvLogging > 1)
	{
		WCHAR szInfo[255];
		_wsprintf(szInfo, SKIPLEN(countof(szInfo)) L"CRealConsole::SwitchKeyboardLayout(CP:%i, HKL:0x%08I64X)",
		          (DWORD)wParam, (unsigned __int64)(DWORD_PTR)dwNewKeyboardLayout);
		LogString(szInfo);
	}

	// ����� ��������� ����� ��������, ����� �� ���������
	mp_RBuf->SetKeybLayout(dwNewKeyboardLayout);
	
	// � FAR ��� XLat �������� ���:
	//PostConsoleMessageW(hFarWnd,WM_INPUTLANGCHANGEREQUEST, INPUTLANGCHANGE_FORWARD, 0);
	PostConsoleMessage(hConWnd, WM_INPUTLANGCHANGEREQUEST, wParam, (LPARAM)dwNewKeyboardLayout);
}

void CRealConsole::Paste()
{
	if (!this) return;

	if (!hConWnd) return;

#ifndef RCON_INTERNAL_PASTE
	// ����� ���
	PostConsoleMessage(hConWnd, WM_COMMAND, SC_PASTE_SECRET, 0);
#endif
#ifdef RCON_INTERNAL_PASTE

	// � ����� � ����� ��� ������
	if (!OpenClipboard(NULL))
	{
		MBox(_T("Can't open PC clipboard"));
		return;
	}

	HGLOBAL hglb = GetClipboardData(CF_UNICODETEXT);

	if (!hglb)
	{
		CloseClipboard();
		MBox(_T("Can't get CF_UNICODETEXT"));
		return;
	}

	wchar_t* lptstr = (wchar_t*)GlobalLock(hglb);

	if (!lptstr)
	{
		CloseClipboard();
		MBox(_T("Can't lock CF_UNICODETEXT"));
		return;
	}

	// ������ ���������� �����
	size_t nLen = _tcslen(lptstr);

	if (nLen>0)
	{
		if (nLen>127)
		{
			TCHAR szMsg[128]; _wsprintf(szMsg, SKIPLEN(countof(szMsg)) L"Clipboard length is %i chars!\nContinue?", nLen);

			if (MessageBox(ghWnd, szMsg, gpConEmu->GetTitle(), MB_OKCANCEL) != IDOK)
			{
				GlobalUnlock(hglb);
				CloseClipboard();
				return;
			}
		}

		INPUT_RECORD r = {KEY_EVENT};

		// ��������� � ������� ��� ������� ��: lptstr
		while(*lptstr)
		{
			r.Event.KeyEvent.bKeyDown = TRUE;
			r.Event.KeyEvent.uChar.UnicodeChar = *lptstr;
			r.Event.KeyEvent.wRepeatCount = 1; //TODO("0-15 ? Specifies the repeat count for the current message. The value is the number of times the keystroke is autorepeated as a result of the user holding down the key. If the keystroke is held long enough, multiple messages are sent. However, the repeat count is not cumulative.");
			r.Event.KeyEvent.wVirtualKeyCode = 0;
			//r.Event.KeyEvent.wVirtualScanCode = ((DWORD)lParam & 0xFF0000) >> 16; // 16-23 - Specifies the scan code. The value depends on the OEM.
			// 24 - Specifies whether the key is an extended key, such as the right-hand ALT and CTRL keys that appear on an enhanced 101- or 102-key keyboard. The value is 1 if it is an extended key; otherwise, it is 0.
			// 29 - Specifies the context code. The value is 1 if the ALT key is held down while the key is pressed; otherwise, the value is 0.
			// 30 - Specifies the previous key state. The value is 1 if the key is down before the message is sent, or it is 0 if the key is up.
			// 31 - Specifies the transition state. The value is 1 if the key is being released, or it is 0 if the key is being pressed.
			//r.Event.KeyEvent.dwControlKeyState = 0;
			PostConsoleEvent(&r);
			// � ����� �������� ����������
			r.Event.KeyEvent.bKeyDown = FALSE;
			PostConsoleEvent(&r);
		}
	}

	GlobalUnlock(hglb);
	CloseClipboard();
#endif
}

bool CRealConsole::isConsoleClosing()
{
	if (!gpConEmu->isValid(this))
		return true;

	if (m_ServerClosing.nServerPID
	        && m_ServerClosing.nServerPID == mn_ConEmuC_PID
	        && (GetTickCount() - m_ServerClosing.nRecieveTick) >= SERVERCLOSETIMEOUT)
	{
		// ������, ������ ����� �� ����� ������? �� ��������, ����� �� ���-���� ����� �����������?
		if (WaitForSingleObject(mh_ConEmuC, 0))
		{
#ifdef _DEBUG
			wchar_t szTitle[128], szText[255];
			_wsprintf(szTitle, SKIPLEN(countof(szTitle)) L"ConEmu, PID=%u", GetCurrentProcessId());
			_wsprintf(szText, SKIPLEN(countof(szText))
			          L"This is Debug message.\n\nServer hung. PID=%u\nm_ServerClosing.nServerPID=%u\n\nPress Ok to terminate server",
			          mn_ConEmuC_PID, m_ServerClosing.nServerPID);
			MessageBox(NULL, szText, szTitle, MB_ICONSTOP);
#else
			_ASSERTE(m_ServerClosing.nServerPID==0);
#endif
			TerminateProcess(mh_ConEmuC, 100);
		}

		return true;
	}

	if ((hConWnd == NULL) || mb_InCloseConsole)
		return true;

	return false;
}

void CRealConsole::CloseConsole(BOOL abForceTerminate /* = FALSE */)
{
	if (!this) return;

	_ASSERTE(!mb_ProcessRestarted);

	// ����� background
	CESERVER_REQ_SETBACKGROUND BackClear = {};
	mp_VCon->SetBackgroundImageData(&BackClear);

	if (hConWnd)
	{
		if (gpSet->isSafeFarClose && !abForceTerminate)
		{
			BOOL lbExecuted = FALSE;
			DWORD nFarPID = GetFarPID(TRUE/*abPluginRequired*/);

			if (nFarPID && isAlive())
			{
				CConEmuPipe pipe(nFarPID, CONEMUREADYTIMEOUT);

				//if (pipe.Init(_T("CRealConsole::CloseConsole"), TRUE))
				{
					mb_InCloseConsole = TRUE;
					//DWORD cbWritten=0;
					gpConEmu->DebugStep(_T("ConEmu: ACTL_QUIT"));
					//lbExecuted = pipe.Execute(CMD_QUITFAR);
					LPCWSTR pszMacro = gpSet->SafeFarCloseMacro();
					_ASSERTE(pszMacro && *pszMacro);

					// Async, ����� ConEmu ��������� �����
					PostMacro(pszMacro, TRUE);
					//lbExecuted = pipe.Execute(CMD_POSTMACRO, pszMacro, (_tcslen(pszMacro)+1)*2);
					lbExecuted = TRUE;

					gpConEmu->DebugStep(NULL);
				}

				if (lbExecuted)
					return;
			}
		}

		if (abForceTerminate && GetActivePID() && !mn_InRecreate)
		{
			// ��������, ���� �����:
			// a) ������ ������� � ���� �������
			// b) �������� �������� �������
			BOOL lbTerminateSucceeded = FALSE;
			ConProcess* pPrc = NULL;
			int nPrcCount = GetProcesses(&pPrc);
			if ((nPrcCount > 0) && pPrc)
			{
				DWORD nActivePID = GetActivePID();
				DWORD dwServerPID = GetServerPID();
				if (nActivePID && dwServerPID)
				{
					wchar_t szActive[64] = {};
					for (int i = 0; i < nPrcCount; i++)
					{
						if (pPrc[i].ProcessID == nActivePID)
						{
							lstrcpyn(szActive, pPrc[i].Name, countof(szActive));
							break;
						}
					}
					if (!*szActive)
						wcscpy_c(szActive, L"<Not found>");
					//	_wsprintf(szActive, SKIPLEN(countof(szActive)) L"PID=%u", nActivePID);

					// �������
					wchar_t szMsg[255];
					_wsprintf(szMsg, SKIPLEN(countof(szMsg)) 
						L"Do you want to close %s (Yes),\n"
						L"or terminate (kill) active process (No)?\n"
						L"\nActive process '%s' PID=%u",
						hGuiWnd ? L"active program" : L"RealConsole",
						szActive, nActivePID);
					BOOL b = gbDontEnable; gbDontEnable = TRUE;
					int nBtn = MessageBox(gbMessagingStarted ? ghWnd : NULL, szMsg, Title, MB_ICONEXCLAMATION|MB_YESNOCANCEL);
					gbDontEnable = b;

					if (nBtn == IDCANCEL)
					{
						return;
					}
					else if (nBtn == IDNO)
					{
						//Terminate
						CESERVER_REQ *pIn = ExecuteNewCmd(CECMD_TERMINATEPID, sizeof(CESERVER_REQ_HDR)+sizeof(DWORD));
						if (pIn)
						{
							pIn->dwData[0] = nActivePID;
							DWORD dwTickStart = timeGetTime();
							
							CESERVER_REQ *pOut = ExecuteSrvCmd(dwServerPID, pIn, ghWnd);
							
							gpSetCls->debugLogCommand(pIn, FALSE, dwTickStart, timeGetTime()-dwTickStart, L"ExecuteSrvCmd", pOut);
							
							if (pOut)
							{
								if (pOut->hdr.cbSize == sizeof(CESERVER_REQ_HDR) + 2*sizeof(DWORD))
								{
									// �����, ����� �� �������� ��� � ������� ������
									lbTerminateSucceeded = TRUE;
									// ���� ������ - �������
									if (pOut->dwData[0] == FALSE)
									{
										_wsprintf(szMsg, SKIPLEN(countof(szMsg)) 
											L"TerminateProcess(%u) failed, code=0x%08X",
											nActivePID, pOut->dwData[1]);
										MBox(szMsg);
									}
								}
								ExecuteFreeResult(pOut);
							}
							ExecuteFreeResult(pIn);
						}
					}
				}
			}
			if (pPrc)
				free(pPrc);
			if (lbTerminateSucceeded)
				return;
		}

		mb_InCloseConsole = TRUE;
		if (hGuiWnd)
		{
			PostConsoleMessage(hGuiWnd, WM_CLOSE, 0, 0);
		}
		else
		{
			PostConsoleMessage(hConWnd, WM_CLOSE, 0, 0);
		}
	}
	else
	{
		m_Args.bDetached = FALSE;

		if (mp_VCon)
			gpConEmu->OnVConTerminated(mp_VCon);
	}
}

// ��������� ������ � ����
BOOL CRealConsole::CanCloseTab(BOOL abPluginRequired /*= FALSE*/)
{
	if (abPluginRequired)
	{
		if (!isFar(TRUE/* abPluginRequired */) /*&& !GuiWnd()*/)
			return FALSE;
	}
	return TRUE;
}

// ��� ���� - ����� (� ��������������) ������� ������� ���.
// ��� GUI  - WM_CLOSE, ����� ���� �����������
// ��� ��������� (cmd.exe, � �.�.) WM_CLOSE � �������. ������ �����, ��� ��������� �����
void CRealConsole::CloseTab()
{
	if (!this)
	{
		_ASSERTE(this);
		return;
	}

	if (GuiWnd())
	{
		PostConsoleMessage(GuiWnd(), WM_CLOSE, 0, 0);
	}
	else
	{
		if (CanCloseTab(TRUE))
			PostMacro(gpSet->TabCloseMacro());
		else
			PostConsoleMessage(hConWnd, WM_CLOSE, 0, 0);
	}
}

uint CRealConsole::TextWidth()
{
	_ASSERTE(this!=NULL);

	if (!this) return MIN_CON_WIDTH;
	return mp_ABuf->TextWidth();
}

uint CRealConsole::TextHeight()
{
	_ASSERTE(this!=NULL);

	if (!this) return MIN_CON_HEIGHT;
	return mp_ABuf->TextHeight();
}

uint CRealConsole::BufferHeight(uint nNewBufferHeight/*=0*/)
{
	return mp_ABuf->BufferHeight(nNewBufferHeight);
}

bool CRealConsole::isActive()
{
	if (!this) return false;

	if (!mp_VCon) return false;

	return gpConEmu->isActive(mp_VCon);
}

bool CRealConsole::isVisible()
{
	if (!this) return false;

	if (!mp_VCon) return false;

	return gpConEmu->isVisible(mp_VCon);
}

// ��������� ������� �� ������������ ��������!
// � ������� ���� ��������� ������, ��������,
// mp_ABuf->GetDetector()->GetFlags()
void CRealConsole::CheckFarStates()
{
#ifdef _DEBUG

	if (mb_DebugLocked)
		return;

#endif
	DWORD nLastState = mn_FarStatus;
	DWORD nNewState = (mn_FarStatus & (~CES_FARFLAGS));

	if (GetFarPID() == 0)
	{
		nNewState = 0;
	}
	else
	{
		// ������� �������� ��������� ������ �� ���������: ���� � ��� �� ������� ������,
		// ��� ������� "viewer/editor" - �� ���������� ������ CES_VIEWER/CES_EDITOR
		if (mp_tabs && mn_ActiveTab >= 0 && mn_ActiveTab < mn_tabsCount)
		{
			if (mp_tabs[mn_ActiveTab].Type != 1)
			{
				if (mp_tabs[mn_ActiveTab].Type == 2)
					nNewState |= CES_VIEWER;
				else if (mp_tabs[mn_ActiveTab].Type == 3)
					nNewState |= CES_EDITOR;
			}
		}

		// ���� �� ��������� ������ �� ���������� - ������ ����� ���� ���� ������, ����
		// "viewer/editor" �� ��� ���������� ������� � ����
		if ((nNewState & CES_FARFLAGS) == 0)
		{
			// �������� ���������� "viewer/editor" �� ��������� ���� �������
			if (wcsncmp(Title, ms_Editor, _tcslen(ms_Editor))==0 || wcsncmp(Title, ms_EditorRus, _tcslen(ms_EditorRus))==0)
				nNewState |= CES_EDITOR;
			else if (wcsncmp(Title, ms_Viewer, _tcslen(ms_Viewer))==0 || wcsncmp(Title, ms_ViewerRus, _tcslen(ms_ViewerRus))==0)
				nNewState |= CES_VIEWER;
			else if (isFilePanel(true, true))
				nNewState |= CES_FILEPANEL;
		}

		// ����� � ���, ����� �� �������� ������ CES_MAYBEPANEL ���� � ������� ������ ������
		// ������ ������������ ������ � ����������/�������
		if ((nNewState & (CES_EDITOR | CES_VIEWER)) != 0)
			nNewState &= ~(CES_MAYBEPANEL|CES_WASPROGRESS|CES_OPER_ERROR); // ��� ������������ � ��������/������ - �������� CES_MAYBEPANEL

		if ((nNewState & CES_FILEPANEL) == CES_FILEPANEL)
		{
			nNewState |= CES_MAYBEPANEL; // ������ � ������ - �������� ������
			nNewState &= ~(CES_WASPROGRESS|CES_OPER_ERROR); // ������ ������ ������� ����������� �� ����
		}

		if (mn_Progress >= 0 && mn_Progress <= 100)
		{
			if (mn_ConsoleProgress == mn_Progress)
			{
				// ��� ���������� ��������� �� ������ ������� - Warning ������ ������ ���
				mn_PreWarningProgress = -1;
				nNewState &= ~CES_OPER_ERROR;
				nNewState |= CES_WASPROGRESS; // �������� ������, ��� �������� ���
			}
			else
			{
				mn_PreWarningProgress = mn_Progress;

				if ((nNewState & CES_MAYBEPANEL) == CES_MAYBEPANEL)
					nNewState |= CES_WASPROGRESS; // �������� ������, ��� �������� ���

				nNewState &= ~CES_OPER_ERROR;
			}
		}
		else if ((nNewState & (CES_WASPROGRESS|CES_MAYBEPANEL)) == (CES_WASPROGRESS|CES_MAYBEPANEL)
		        && mn_PreWarningProgress != -1)
		{
			if (mn_LastWarnCheckTick == 0)
			{
				mn_LastWarnCheckTick = GetTickCount();
			}
			else if ((mn_FarStatus & CES_OPER_ERROR) == CES_OPER_ERROR)
			{
				// ��� �������� ������� - ���� ������ ��� ����������
				_ASSERTE((nNewState & CES_OPER_ERROR) == CES_OPER_ERROR);
				nNewState |= CES_OPER_ERROR;
			}
			else
			{
				DWORD nDelta = GetTickCount() - mn_LastWarnCheckTick;

				if (nDelta > CONSOLEPROGRESSWARNTIMEOUT)
				{
					nNewState |= CES_OPER_ERROR;
					//mn_LastWarnCheckTick = 0;
				}
			}
		}
	}

	if (mn_Progress == -1 && mn_PreWarningProgress != -1)
	{
		if ((nNewState & CES_WASPROGRESS) == 0)
		{
			mn_PreWarningProgress = -1; mn_LastWarnCheckTick = 0;
			gpConEmu->UpdateProgress();
		}
		else if (/*isFilePanel(true)*/ (nNewState & CES_FILEPANEL) == CES_FILEPANEL)
		{
			nNewState &= ~(CES_OPER_ERROR|CES_WASPROGRESS);
			mn_PreWarningProgress = -1; mn_LastWarnCheckTick = 0;
			gpConEmu->UpdateProgress();
		}
	}

	if (nNewState != nLastState)
	{
#ifdef _DEBUG

		if ((nNewState & CES_FILEPANEL) == 0)
			nNewState = nNewState;

#endif
		mn_FarStatus = nNewState;
		gpConEmu->UpdateProcessDisplay(FALSE);
	}
}

// mn_Progress �� ������, ��������� ����������
short CRealConsole::CheckProgressInTitle()
{
	// ��������� ��������� NeroCMD � ��. ���������� �������� (���� ������ ��������� � ������� �������)
	// ����������� � CheckProgressInConsole (-> mn_ConsoleProgress), ���������� �� FindPanels
	short nNewProgress = -1;
	int i = 0;
	wchar_t ch;

	// Wget [41%] http://....
	while((ch = Title[i])!=0 && (ch == L'{' || ch == L'(' || ch == L'['))
		i++;

	//if (Title[0] == L'{' || Title[0] == L'(' || Title[0] == L'[') {
	if (Title[i])
	{
		// ��������� �������/��������� ����������� �������� �� ���� ��������
		if (Title[i] == L' ')
		{
			i++;

			if (Title[i] == L' ')
				i++;
		}

		// ������, ���� ����� - ��������� ��������
		if (isDigit(Title[i]))
		{
			if (isDigit(Title[i+1]) && isDigit(Title[i+2])
			        && (Title[i+3] == L'%' || (Title[i+3] == L'.' && isDigit(Title[i+4]) && Title[i+7] == L'%'))
			  )
			{
				// �� ���� ������ 100% ���� �� ������ :)
				nNewProgress = 100*(Title[i] - L'0') + 10*(Title[i+1] - L'0') + (Title[i+2] - L'0');
			}
			else if (isDigit(Title[i+1])
			        && (Title[i+2] == L'%' || (Title[i+2] == L'.' && isDigit(Title[i+3]) && Title[i+4] == L'%'))
			       )
			{
				// 10 .. 99 %
				nNewProgress = 10*(Title[i] - L'0') + (Title[i+1] - L'0');
			}
			else if (Title[i+1] == L'%' || (Title[i+1] == L'.' && isDigit(Title[i+2]) && Title[i+3] == L'%'))
			{
				// 0 .. 9 %
				nNewProgress = (Title[i] - L'0');
			}

			_ASSERTE(nNewProgress<=100);

			if (nNewProgress > 100)
				nNewProgress = 100;
		}
	}

	return nNewProgress;
}

void CRealConsole::OnTitleChanged()
{
	if (!this) return;

	#ifdef _DEBUG
	if (mb_DebugLocked)
		return;
	#endif
	
	wcscpy(Title, TitleCmp);
	// ��������� ��������� ��������
	//short nLastProgress = mn_Progress;
	short nNewProgress;
	TitleFull[0] = 0;
	nNewProgress = CheckProgressInTitle();

	if (nNewProgress == -1)
	{
		// mn_ConsoleProgress ����������� � FindPanels, ������ ���� ��� ������
		if (mn_ConsoleProgress != -1)
		{
			// ��������� ��������� NeroCMD � ��. ���������� ��������
			// ���� ������ ��������� � ������� �������
			nNewProgress = mn_ConsoleProgress;
			// ���� � ��������� ��� ��������� (��� ���� ������ � �������)
			// �������� �� � ��� ���������
			wchar_t szPercents[5];
			_ltow(nNewProgress, szPercents, 10);
			lstrcatW(szPercents, L"%");

			if (!wcsstr(TitleCmp, szPercents))
			{
				TitleFull[0] = L'{';
				_ltow(nNewProgress, TitleFull+1, 10);
				lstrcatW(TitleFull, L"%} ");
			}
		}
	}

	wcscat_c(TitleFull, TitleCmp);
	// ��������� �� ��� �����
	mn_Progress = nNewProgress;

	if (nNewProgress >= 0 && nNewProgress <= 100)
		mn_PreWarningProgress = nNewProgress;

	//SetProgress(nNewProgress);

	TitleAdmin[0] = 0;
	//if (isAdministrator())
	//{
	//	wcscpy_c(TitleAdmin, TitleFull);
	//	wcscat_c(TitleAdmin, gpSet->szAdminTitleSuffix);
	//}
	// && (gpSet->bAdminShield || gpSet->szAdminTitleSuffix))
	//{
	//	if (!gpSet->bAdminShield)
	//		wcscat(TitleFull, gpSet->szAdminTitleSuffix);
	//}

	CheckFarStates();
	// ����� ����� ������������ �� ��������� ��������� �� ����,
	// ��� �� ������, ��� ������������� ��������...
	TODO("������ ����������� � ��� ������� ���������� ���������!");

	if (Title[0] == L'{' || Title[0] == L'(')
		CheckPanelTitle();

	// ������� �� GetProgress, �.�. �� ��� ��������� mn_PreWarningProgress
	nNewProgress = GetProgress(NULL);

	if (gpConEmu->isActive(mp_VCon) && wcscmp(GetTitle(), gpConEmu->GetLastTitle(false)))
	{
		// ��� �������� ������� - ��������� ���������. �������� ��������� ��� ��
		mn_LastShownProgress = nNewProgress;
		gpConEmu->UpdateTitle();
	}
	else if (mn_LastShownProgress != nNewProgress)
	{
		// ��� �� �������� ������� - ��������� ������� ����, ��� � ��� ��������� ��������
		mn_LastShownProgress = nNewProgress;
		gpConEmu->UpdateProgress();
	}
	
	mp_VCon->OnTitleChanged(); // �������� ��� �� ��������

	gpConEmu->mp_TabBar->Update(); // ������� ��������� ��������?
}

bool CRealConsole::isFilePanel(bool abPluginAllowed/*=false*/, bool abSkipEditViewCheck /*= false*/)
{
	if (!this) return false;

	if (Title[0] == 0) return false;

	// ������� ������������ � �������� �������� ������ ����, � ��� Viewer/Editor ��� ���������
	if (!abSkipEditViewCheck)
	{
		if (isEditor() || isViewer())
			return false;
	}

	// ���� ����� �����-���� ������� - ������� ��� ��� �� ������
	DWORD dwFlags = mp_ABuf ? mp_ABuf->GetDetector()->GetFlags() : FR_FREEDLG_MASK;

	if ((dwFlags & FR_FREEDLG_MASK) != 0)
		return false;

	// ����� ��� DragDrop
	if (_tcsncmp(Title, ms_TempPanel, _tcslen(ms_TempPanel)) == 0 || _tcsncmp(Title, ms_TempPanelRus, _tcslen(ms_TempPanelRus)) == 0)
		return true;

	if ((abPluginAllowed && Title[0]==_T('{')) ||
	        (_tcsncmp(Title, _T("{\\\\"), 3)==0) ||
	        (Title[0] == _T('{') && isDriveLetter(Title[1]) && Title[2] == _T(':') && Title[3] == _T('\\')))
	{
		TCHAR *Br = _tcsrchr(Title, _T('}'));

		if (Br && _tcsstr(Br, _T("} - Far")))
		{
			if (mp_RBuf->isLeftPanel() || mp_RBuf->isRightPanel())
				return true;
		}
	}

	//TCHAR *BrF = _tcschr(Title, '{'), *BrS = _tcschr(Title, '}'), *Slash = _tcschr(Title, '\\');
	//if (BrF && BrS && Slash && BrF == Title && (Slash == Title+1 || Slash == Title+3))
	//    return true;
	return false;
}

bool CRealConsole::isEditor()
{
	if (!this) return false;

	return GetFarStatus() & CES_EDITOR;
}

bool CRealConsole::isEditorModified()
{
	if (!this) return false;

	if (!isEditor()) return false;

	if (mp_tabs && mn_tabsCount)
	{
		for(int j = 0; j < mn_tabsCount; j++)
		{
			if (mp_tabs[j].Current)
			{
				if (mp_tabs[j].Type == /*Editor*/3)
				{
					return (mp_tabs[j].Modified != 0);
				}

				return false;
			}
		}
	}

	return false;
}

bool CRealConsole::isViewer()
{
	if (!this) return false;

	return GetFarStatus() & CES_VIEWER;
}

bool CRealConsole::isNtvdm()
{
	if (!this) return false;

	if (mn_ProgramStatus & CES_NTVDM)
	{
		// ������� 16bit ���������� ������ �� WinEvent. ����� �� ��������� ������ ��� ����������,
		// �.�. ������� ntvdm.exe �� �����������, � �������� � ������.
		return true;
		//if (mn_ProgramStatus & CES_FARFLAGS) {
		//  //mn_ActiveStatus &= ~CES_NTVDM;
		//} else if (isFilePanel()) {
		//  //mn_ActiveStatus &= ~CES_NTVDM;
		//} else {
		//  return true;
		//}
	}

	return false;
}

LPCWSTR CRealConsole::GetCmd()
{
	if (!this) return L"";

	if (m_Args.pszSpecialCmd)
		return m_Args.pszSpecialCmd;
	else
		return gpSet->GetCmd();
}

LPCWSTR CRealConsole::GetDir()
{
	if (!this) return L"";

	if (m_Args.pszSpecialCmd)
		return m_Args.pszStartupDir;
	else
		return gpConEmu->ms_ConEmuCurDir;
}

BOOL CRealConsole::GetUserPwd(const wchar_t** ppszUser, const wchar_t** ppszDomain, BOOL* pbRestricted)
{
	if (m_Args.bRunAsRestricted)
	{
		*pbRestricted = TRUE;
		*ppszUser = /**ppszPwd =*/ NULL;
		return TRUE;
	}

	if (m_Args.pszUserName /*&& m_Args.pszUserPassword*/)
	{
		*ppszUser = m_Args.pszUserName;
		_ASSERTE(ppszDomain!=NULL);
		*ppszDomain = m_Args.pszDomain;
		//*ppszPwd = m_Args.pszUserPassword;
		*pbRestricted = FALSE;
		return TRUE;
	}

	return FALSE;
}

short CRealConsole::GetProgress(BOOL *rpbError)
{
	if (!this) return -1;

	if (mn_Progress >= 0)
		return mn_Progress;

	if (mn_PreWarningProgress >= 0)
	{
		// mn_PreWarningProgress - ��� ��������� �������� ��������� (0..100)
		// �� ����� ���������� �������� - �� ����� ��� ���� �� �������
		if (rpbError)
		{
			//*rpbError = TRUE; --
			*rpbError = (mn_FarStatus & CES_OPER_ERROR) == CES_OPER_ERROR;
		}

		//if (mn_LastProgressTick != 0 && rpbError) {
		//	DWORD nDelta = GetTickCount() - mn_LastProgressTick;
		//	if (nDelta >= 1000) {
		//		if (rpbError) *rpbError = TRUE;
		//	}
		//}
		return mn_PreWarningProgress;
	}

	return -1;
}

//// ���������� ���������� mn_Progress � mn_LastProgressTick
//void CRealConsole::SetProgress(short anProgress)
//{
//	mn_Progress = anProgress;
//	if (anProgress >= 0 && anProgress <= 100) {
//		mn_PreWarningProgress = anProgress;
//		mn_LastProgressTick = GetTickCount();
//	} else {
//		mn_LastProgressTick = 0;
//	}
//}

void CRealConsole::UpdateGuiInfoMapping(const ConEmuGuiMapping* apGuiInfo)
{
	DWORD dwServerPID = GetServerPID();
	if (dwServerPID)
	{
		CESERVER_REQ* pIn = ExecuteNewCmd(CECMD_GUICHANGED, sizeof(CESERVER_REQ_HDR)+apGuiInfo->cbSize);
		if (pIn)
		{
			memmove(&(pIn->GuiInfo), apGuiInfo, apGuiInfo->cbSize);
			DWORD dwTickStart = timeGetTime();
			
			CESERVER_REQ *pOut = ExecuteSrvCmd(dwServerPID, pIn, ghWnd);
			
			gpSetCls->debugLogCommand(pIn, FALSE, dwTickStart, timeGetTime()-dwTickStart, L"ExecuteSrvCmd", pOut);
			
			if (pOut)
				ExecuteFreeResult(pOut);
			ExecuteFreeResult(pIn);
		}
	}
}

// ������ � �������� ��� CMD_FARSETCHANGED
// ����������� ���������: gpSet->isFARuseASCIIsort, gpSet->isShellNoZoneCheck;
void CRealConsole::UpdateFarSettings(DWORD anFarPID/*=0*/)
{
	if (!this) return;

	DWORD dwFarPID = anFarPID ? anFarPID : GetFarPID();

	if (!dwFarPID) return;

	////int nLen = /*ComSpec\0*/ 8 + /*ComSpecC\0*/ 9 + 20 +  2*_tcslen(gpConEmu->ms_ConEmuExe);
	//wchar_t szCMD[MAX_PATH+1];
	////wchar_t szData[MAX_PATH*4+64];
	//
	//// ���� ���������� ComSpecC - ������ ������������� ����������� ComSpec
	//if (!GetEnvironmentVariable(L"ComSpecC", szCMD, MAX_PATH) || szCMD[0] == 0)
	//{
	//	if (!GetEnvironmentVariable(L"ComSpec", szCMD, MAX_PATH) || szCMD[0] == 0)
	//		szCMD[0] = 0;
	//}
	//
	//if (szCMD[0] != 0)
	//{
	//	// ������ ���� ��� (��������) �� conemuc.exe
	//
	//	wchar_t* pwszCopy = wcsrchr(szCMD, L'\\'); if (!pwszCopy) pwszCopy = szCMD;
	//
	//#if !defined(__GNUC__)
	//#pragma warning( push )
	//#pragma warning(disable : 6400)
	//#endif
	//
	//	if (lstrcmpiW(pwszCopy, L"ConEmuC")==0 || lstrcmpiW(pwszCopy, L"ConEmuC.exe")==0
	//	        || lstrcmpiW(pwszCopy, L"ConEmuC64")==0 || lstrcmpiW(pwszCopy, L"ConEmuC64.exe")==0)
	//		szCMD[0] = 0;
	//
	//#if !defined(__GNUC__)
	//#pragma warning( pop )
	//#endif
	//}
	//
	//// ComSpec/ComSpecC �� ���������, ���������� cmd.exe
	//if (szCMD[0] == 0)
	//{
	//	wchar_t* psFilePart = NULL;
	//
	//	if (!SearchPathW(NULL, L"cmd.exe", NULL, MAX_PATH, szCMD, &psFilePart))
	//	{
	//		DisplayLastError(L"Can't find cmd.exe!\n", 0);
	//		return;
	//	}
	//}

	// [MAX_PATH*4+64]
	FAR_REQ_FARSETCHANGED *pSetEnvVar = (FAR_REQ_FARSETCHANGED*)calloc(sizeof(FAR_REQ_FARSETCHANGED),1); //+2*(MAX_PATH*4+64),1);
	//wchar_t *szData = pSetEnvVar->szEnv;
	pSetEnvVar->bFARuseASCIIsort = gpSet->isFARuseASCIIsort;
	pSetEnvVar->bShellNoZoneCheck = gpSet->isShellNoZoneCheck;
	pSetEnvVar->bMonitorConsoleInput = (gpSetCls->m_ActivityLoggingType == glt_Input);
	//BOOL lbNeedQuot = (wcschr(gpConEmu->ms_ConEmuCExeFull, L' ') != NULL);
	//wchar_t* pszName = szData;
	//lstrcpy(pszName, L"ComSpec");
	//wchar_t* pszValue = pszName + _tcslen(pszName) + 1;

	//if (gpSet->AutoBufferHeight)
	//{
	//	if (lbNeedQuot) *(pszValue++) = L'"';

	//	lstrcpy(pszValue, gpConEmu->ms_ConEmuCExeFull);

	//	if (lbNeedQuot) lstrcat(pszValue, L"\"");

	//	lbNeedQuot = (szCMD[0] != L'"') && (wcschr(szCMD, L' ') != NULL);
	//	pszName = pszValue + _tcslen(pszValue) + 1;
	//	lstrcpy(pszName, L"ComSpecC");
	//	pszValue = pszName + _tcslen(pszName) + 1;
	//}

	//if (lbNeedQuot) *(pszValue++) = L'"';

	//lstrcpy(pszValue, szCMD);

	//if (lbNeedQuot) lstrcat(pszValue, L"\"");

	//pszName = pszValue + _tcslen(pszValue) + 1;
	//lstrcpy(pszName, L"ConEmuOutput");
	//pszName = pszName + _tcslen(pszName) + 1;
	//lstrcpy(pszName, !gpSet->nCmdOutputCP ? L"" : ((gpSet->nCmdOutputCP == 1) ? L"AUTO"
	//        : (((gpSet->nCmdOutputCP == 2) ? L"UNICODE" : L"ANSI"))));
	//pszName = pszName + _tcslen(pszName) + 1;
	//*(pszName++) = 0;
	//*(pszName++) = 0;

	// ��������� � �������
	CConEmuPipe pipe(dwFarPID, 300);
	int nSize = sizeof(FAR_REQ_FARSETCHANGED); //+2*(pszName - szData);

	if (pipe.Init(L"FarSetChange", TRUE))
		pipe.Execute(CMD_FARSETCHANGED, pSetEnvVar, nSize);

	//pipe.Execute(CMD_SETENVVAR, szData, 2*(pszName - szData));
	free(pSetEnvVar); pSetEnvVar = NULL;
}

HWND CRealConsole::FindPicViewFrom(HWND hFrom)
{
	// !!! PicView ����� ���� ���������, �� ������� �� �������� ���
	HWND hPicView = NULL;

	//hPictureView = FindWindowEx(ghWnd, NULL, L"FarPictureViewControlClass", NULL);
	// ����� ����� ���� ��� "FarPictureViewControlClass", ��� � "FarMultiViewControlClass"
	// � ��� ��������� � ��� ���� ����
	while((hPicView = FindWindowEx(hFrom, hPicView, NULL, L"PictureView")) != NULL)
	{
		// ��������� �� �������������� ����
		DWORD dwPID, dwTID;
		dwTID = GetWindowThreadProcessId(hPicView, &dwPID);

		if (dwPID == mn_FarPID || dwPID == GetActivePID())
			break;
	}

	return hPicView;
}

// ��������� ���� ��� PictureView ������ ����� ������������� �������������, ��� ���
// ������������ �� ���� ��� ����������� "���������" - ������
HWND CRealConsole::isPictureView(BOOL abIgnoreNonModal/*=FALSE*/)
{
	if (!this) return NULL;

	if (hPictureView && (!IsWindow(hPictureView) || !isFar()))
	{
		hPictureView = NULL; mb_PicViewWasHidden = FALSE;
		gpConEmu->InvalidateAll();
	}

	// !!! PicView ����� ���� ���������, �� ������� �� �������� ���
	if (!hPictureView)
	{
		// !! �������� ���� ������ ���� ������ � ����� ���������. � ������� - ���� ������ �� ������ !!
		//hPictureView = FindWindowEx(ghWnd, NULL, L"FarPictureViewControlClass", NULL);
		//hPictureView = FindPicViewFrom(ghWnd);
		//if (!hPictureView)
		//hPictureView = FindWindowEx('ghWnd DC', NULL, L"FarPictureViewControlClass", NULL);
		hPictureView = FindPicViewFrom(mp_VCon->GetView());

		if (!hPictureView)    // FullScreen?
		{
			//hPictureView = FindWindowEx(NULL, NULL, L"FarPictureViewControlClass", NULL);
			hPictureView = FindPicViewFrom(NULL);
		}

		// �������������� �������� ���� ��� ��������� ���� FindPicViewFrom
		//if (hPictureView) {
		//    // ��������� �� �������������� ����
		//    DWORD dwPID, dwTID;
		//    dwTID = GetWindowThreadProcessId ( hPictureView, &dwPID );
		//    if (dwPID != mn_FarPID) {
		//        hPictureView = NULL; mb_PicViewWasHidden = FALSE;
		//    }
		//}
	}

	if (hPictureView)
	{
		WARNING("PicView ������ �������� � DC, �� ����� ���� FullScreen?")
		if (mb_PicViewWasHidden)
		{
			// ������ ���� ������ ��� ������������ �� ������ �������, �� �������� ��� �������
		}
		else
		if (!IsWindowVisible(hPictureView))
		{
			// ���� �������� Help (F1) - ������ PictureView �������� (����� ��������), ������� ��� �������� ���
			hPictureView = NULL; mb_PicViewWasHidden = FALSE;
		}
	}

	if (mb_PicViewWasHidden && !hPictureView) mb_PicViewWasHidden = FALSE;

	if (hPictureView && abIgnoreNonModal)
	{
		wchar_t szClassName[128];

		if (GetClassName(hPictureView, szClassName, countof(szClassName)))
		{
			if (wcscmp(szClassName, L"FarMultiViewControlClass") == 0)
			{
				// ���� ��� ������ �����������, �� ����� ����� ���� �� �������
				DWORD_PTR dwValue = GetWindowLongPtr(hPictureView, 0);

				if (dwValue != 0x200)
					return NULL;
			}
		}
	}

	return hPictureView;
}

HWND CRealConsole::ConWnd()
{
	if (!this) return NULL;

	return hConWnd;
}

HWND CRealConsole::GetView()
{
	if (!this) return NULL;

	return mp_VCon->GetView();
}

// ���� �������� � Gui-������ (Notepad, Putty, ...)
HWND CRealConsole::GuiWnd()
{
	if (!this) return NULL;

	return hGuiWnd;
}

void CRealConsole::SetGuiMode(DWORD anFlags, HWND ahGuiWnd, DWORD anStyle, DWORD anStyleEx, LPCWSTR asAppFileName, DWORD anAppPID, RECT arcPrev)
{
	if (!this)
	{
		_ASSERTE(this!=NULL);
		return;
	}

	if ((hGuiWnd != NULL) && !IsWindow(hGuiWnd))
		hGuiWnd = NULL; // ���� ���������, ��������� ������

	if (hGuiWnd != NULL && hGuiWnd != ahGuiWnd)
	{
		_ASSERTE(hGuiWnd==NULL);
		return;
	}

	CVConGuard guard(mp_VCon);

	AllowSetForegroundWindow(anAppPID);

	// ���������� ��� ����. ������ (��� ������� exe) ahGuiWnd==NULL, ������ - ����� ������������ �������� ����
	// � ����� � ���, ���� ��� ������ ShowWindow ��������� ��� SetParent �������� (XmlNotepad)
	if (hGuiWnd == NULL)
	{
		rcPreGuiWndRect = arcPrev;
	}
	hGuiWnd = ahGuiWnd;
	mn_GuiWndPID = anAppPID;
	mn_GuiWndStyle = anStyle; mn_GuiWndStylEx = anStyleEx;
	mb_GuiExternMode = FALSE;

#ifdef _DEBUG
	mp_VCon->CreateDbgDlg();
#endif

	// ������ ����� "hGuiWnd = ahGuiWnd", �.�. ��� ���-���������� ������ ������ ������.
	if (isActive())
		gpConEmu->OnBufferHeight();
	
	// ��������� ������ (ConEmuC), ��� ������� GUI �����������
	DWORD nSize = sizeof(CESERVER_REQ_HDR)+sizeof(CESERVER_REQ_ATTACHGUIAPP);
	CESERVER_REQ In;
	ExecutePrepareCmd(&In, CECMD_ATTACHGUIAPP, nSize);

	In.AttachGuiApp.nFlags = anFlags;
	In.AttachGuiApp.hWindow = ahGuiWnd;
	In.AttachGuiApp.nPID = anAppPID;
	if (asAppFileName)
		wcscpy_c(In.AttachGuiApp.sAppFileName, asAppFileName);
	
	DWORD dwTickStart = timeGetTime();
	
	CESERVER_REQ *pOut = ExecuteSrvCmd(GetServerPID(), &In, ghWnd);
	
	gpSetCls->debugLogCommand(&In, FALSE, dwTickStart, timeGetTime()-dwTickStart, L"ExecuteSrvCmd", pOut);

	if (pOut) ExecuteFreeResult(pOut);
	
	if (hGuiWnd)
	{
		mb_InGuiAttaching = TRUE;
		HWND hDcWnd = mp_VCon->GetView();
		RECT rcDC; ::GetWindowRect(hDcWnd, &rcDC);
		MapWindowPoints(NULL, hDcWnd, (LPPOINT)&rcDC, 2);
		// ����� ��������� �� ����������
		ValidateRect(hDcWnd, &rcDC);
		
		DWORD nTID = 0, nPID = 0;
		nTID = GetWindowThreadProcessId(hGuiWnd, &nPID);
		_ASSERTE(nPID == anAppPID);
		AllowSetForegroundWindow(nPID);
		
		/*
		BOOL lbThreadAttach = AttachThreadInput(nTID, GetCurrentThreadId(), TRUE);
		DWORD nMainTID = GetWindowThreadProcessId(ghWnd, NULL);
		BOOL lbThreadAttach2 = AttachThreadInput(nTID, nMainTID, TRUE);
		DWORD nErr = GetLastError();
		*/

		// ��. ������ ��� �� �������� � ConEmu, ����� �������� ����� � ������ ����������?
		// SetFocus �� ��������� - ������ �������
		//SetOtherWindowFocus(hGuiWnd, TRUE/*use apiSetForegroundWindow*/);
		apiSetForegroundWindow(hGuiWnd);

		GetWindowText(hGuiWnd, Title, countof(Title)-2);
		wcscpy_c(TitleFull, Title);
		TitleAdmin[0] = 0;
		mb_ForceTitleChanged = FALSE;
		OnTitleChanged();

		mp_VCon->UpdateThumbnail(TRUE);
	}
}

void CRealConsole::CorrectGuiChildRect(DWORD anStyle, DWORD anStyleEx, RECT& rcGui)
{
	int nX = 0, nY = 0;
	if (anStyle & WS_THICKFRAME)
	{
		nX = GetSystemMetrics(SM_CXSIZEFRAME);
		nY = GetSystemMetrics(SM_CXSIZEFRAME);
	}
	else if (anStyleEx & WS_EX_WINDOWEDGE)
	{
		nX = GetSystemMetrics(SM_CXFIXEDFRAME);
		nY = GetSystemMetrics(SM_CXFIXEDFRAME);
	}
	else if (anStyle & WS_DLGFRAME)
	{
		nX = GetSystemMetrics(SM_CXEDGE);
		nY = GetSystemMetrics(SM_CYEDGE);
	}
	else if (anStyle & WS_BORDER)
	{
		nX = GetSystemMetrics(SM_CXBORDER);
		nY = GetSystemMetrics(SM_CYBORDER);
	}
	else
	{
		nX = GetSystemMetrics(SM_CXFIXEDFRAME);
		nY = GetSystemMetrics(SM_CXFIXEDFRAME);
	}
	rcGui.left -= nX; rcGui.right += nX; rcGui.top -= nY; rcGui.bottom += nY;
}

int CRealConsole::GetStatusLineCount(int nLeftPanelEdge)
{
	if (!this || !isFar())
		return 0;
	
	// ������ ���������� ������ ��� �������� �������� ������
	_ASSERTE(mp_RBuf==mp_ABuf);
	return mp_RBuf->GetStatusLineCount(nLeftPanelEdge);
}

// abIncludeEdges - �������� 
int CRealConsole::CoordInPanel(COORD cr, BOOL abIncludeEdges /*= FALSE*/)
{
	if (!this || mp_ABuf != mp_RBuf)
		return 0;

	RECT rcPanel;

	if (GetPanelRect(FALSE, &rcPanel, FALSE, abIncludeEdges) && CoordInRect(cr, rcPanel))
		return 1;

	if (mp_RBuf->GetPanelRect(TRUE, &rcPanel, FALSE, abIncludeEdges) && CoordInRect(cr, rcPanel))
		return 2;

	return 0;
}

BOOL CRealConsole::GetPanelRect(BOOL abRight, RECT* prc, BOOL abFull /*= FALSE*/, BOOL abIncludeEdges /*= FALSE*/)
{
	if (!this || mp_ABuf != mp_RBuf)
	{
		if (prc)
			*prc = MakeRect(-1,-1);

		return FALSE;
	}

	return mp_RBuf->GetPanelRect(abRight, prc, abFull);
}

// ���������, ������� �� � ���� ����� "far /w".
// � ���� ������, ����� ����, �� ��� ���������� ������ ���������� ��� ���.
// ���������� ���� CtrlUp, CtrlDown � ����� - ���� ���������� � ���.
bool CRealConsole::isFarBufferSupported()
{
	if (!this)
		return false;

	return (m_FarInfo.cbSize && m_FarInfo.bBufferSupport && (m_FarInfo.nFarPID == GetFarPID()));
}

bool CRealConsole::isSelectionAllowed()
{
	if (!this)
		return false;
	return mp_ABuf->isSelectionAllowed();
}

bool CRealConsole::isSelectionPresent()
{
	if (!this)
		return false;
	return mp_ABuf->isSelectionPresent();
}

void CRealConsole::GetConsoleCursorInfo(CONSOLE_CURSOR_INFO *ci)
{
	if (!this) return;
	mp_ABuf->ConsoleCursorInfo(ci);
}

void CRealConsole::GetConsoleScreenBufferInfo(CONSOLE_SCREEN_BUFFER_INFO* sbi)
{
	if (!this) return;
	mp_ABuf->ConsoleScreenBufferInfo(sbi);
}

//void CRealConsole::GetConsoleCursorPos(COORD *pcr)
//{
//	if (!this) return;
//	mp_ABuf->ConsoleCursorPos(pcr);
//}

// � ���������� ���� �� ���������� �������� ��� ��������� ����������
// �� ������� ���� � ���� �� ����� ��������� ������.
// � ��������� ���������� ����������� ����� ���� ���������� ������ "Run as administrator"
// ����� ���� ��������������� ��������...
// ���� ������ ����� ��� ����������. � ����� ������� �� ����� ��������� ���������
// ��� ������� ���������� (������ elevated/non elevated)
bool CRealConsole::isAdministrator()
{
	if (!this) return false;

	//
	return m_Args.bRunAsAdministrator;
}

BOOL CRealConsole::isMouseButtonDown()
{
	if (!this) return FALSE;

	return mb_MouseButtonDown;
}

// ���������� �� CConEmuMain::OnLangChangeConsole � ������� ����
void CRealConsole::OnConsoleLangChange(DWORD_PTR dwNewKeybLayout)
{
	if (mp_RBuf->GetKeybLayout() != dwNewKeybLayout)
	{
		if (gpSetCls->isAdvLogging > 1)
		{
			wchar_t szInfo[255];
			_wsprintf(szInfo, SKIPLEN(countof(szInfo)) L"CRealConsole::OnConsoleLangChange, Old=0x%08X, New=0x%08X",
			          (DWORD)mp_RBuf->GetKeybLayout(), (DWORD)dwNewKeybLayout);
			LogString(szInfo);
		}

		mp_RBuf->SetKeybLayout(dwNewKeybLayout);
		gpConEmu->SwitchKeyboardLayout(dwNewKeybLayout);
		
		#ifdef _DEBUG
		WCHAR szMsg[255];
		HKL hkl = GetKeyboardLayout(0);
		_wsprintf(szMsg, SKIPLEN(countof(szMsg)) L"ConEmu: GetKeyboardLayout(0) after SwitchKeyboardLayout = 0x%08I64X\n",
		          (unsigned __int64)(DWORD_PTR)hkl);
		DEBUGSTRLANG(szMsg);
		//Sleep(2000);
		#endif
	}
	else
	{
		if (gpSetCls->isAdvLogging > 1)
		{
			wchar_t szInfo[255];
			_wsprintf(szInfo, SKIPLEN(countof(szInfo)) L"CRealConsole::OnConsoleLangChange skipped, mp_RBuf->GetKeybLayout() already is 0x%08X",
			          (DWORD)dwNewKeybLayout);
			LogString(szInfo);
		}
	}
}

DWORD CRealConsole::GetConsoleStates()
{
	if (!this) return 0;

	// ��� ������ ����� �����? Real ��� Active?
	_ASSERTE(mp_ABuf==mp_RBuf);
	return mp_RBuf->GetConMode();
}


void CRealConsole::CloseColorMapping()
{
	m_TrueColorerMap.CloseMap();
	//if (mp_ColorHdr) {
	//	UnmapViewOfFile(mp_ColorHdr);
	//mp_ColorHdr = NULL;
	mp_TrueColorerData = NULL;
	//}
	//if (mh_ColorMapping) {
	//	CloseHandle(mh_ColorMapping);
	//	mh_ColorMapping = NULL;
	//}
	mb_DataChanged = TRUE;
	mn_LastColorFarID = 0;
}

//void CRealConsole::CheckColorMapping(DWORD dwPID)
//{
//	if (!dwPID)
//		dwPID = GetFarPID();
//	if ((!dwPID && m_TrueColorerMap.IsValid()) || (dwPID != mn_LastColorFarID)) {
//		//CloseColorMapping();
//		if (!dwPID)
//			return;
//	}
//
//	if (dwPID == mn_LastColorFarID)
//		return; // ��� ����� ���� - ������� ��� ���������!
//
//	mn_LastColorFarID = dwPID; // ����� ��������
//
//}

void CRealConsole::CreateColorMapping()
{
	if (!this)
	{
		_ASSERTE(this!=NULL);
		return;
	}

	BOOL lbResult = FALSE;
	wchar_t szErr[512]; szErr[0] = 0;
	//wchar_t szMapName[512]; szErr[0] = 0;
	AnnotationHeader *pHdr = NULL;
	_ASSERTE(mp_VCon->GetView()!=NULL);
	// 111101 - ���� "hConWnd", �� GetConsoleWindow ������ ���������������.
	m_TrueColorerMap.InitName(AnnotationShareName, (DWORD)sizeof(AnnotationInfo), (DWORD)mp_VCon->GetView()); //-V205
	
	WARNING("������� � ����������!");
	COORD crMaxSize = mp_RBuf->GetMaxSize();
	int nMapCells = max(crMaxSize.X,200) * max(crMaxSize.Y,200) * 2;
	DWORD nMapSize = nMapCells * sizeof(AnnotationInfo) + sizeof(AnnotationHeader);

	pHdr = m_TrueColorerMap.Create(nMapSize);
	if (!pHdr)
	{
		lstrcpyn(szErr, m_TrueColorerMap.GetErrorText(), countof(szErr));
		goto wrap;
	}
	pHdr->struct_size = sizeof(AnnotationHeader);
	pHdr->bufferSize = nMapCells;
	pHdr->locked = 0;
	pHdr->flushCounter = 0;

	//_ASSERTE(mh_ColorMapping == NULL);
	//swprintf_c(szMapName, AnnotationShareName, sizeof(AnnotationInfo), (DWORD)hConWnd);
	//mh_ColorMapping = OpenFileMapping(FILE_MAP_READ, FALSE, szMapName);
	//if (!mh_ColorMapping) {
	//	DWORD dwErr = GetLastError();
	//	swprintf_c (szErr, L"ConEmu: Can't open colorer file mapping. ErrCode=0x%08X. %s", dwErr, szMapName);
	//	goto wrap;
	//}
	//
	//mp_ColorHdr = (AnnotationHeader*)MapViewOfFile(mh_ColorMapping, FILE_MAP_READ,0,0,0);
	//if (!mp_ColorHdr) {
	//	DWORD dwErr = GetLastError();
	//	wchar_t szErr[512];
	//	swprintf_c (szErr, L"ConEmu: Can't map colorer info. ErrCode=0x%08X. %s", dwErr, szMapName);
	//	CloseHandle(mh_ColorMapping); mh_ColorMapping = NULL;
	//	goto wrap;
	//}
	pHdr = m_TrueColorerMap.Ptr();
	mp_TrueColorerData = (const AnnotationInfo*)(((LPBYTE)pHdr)+pHdr->struct_size);
	lbResult = TRUE;
wrap:

	if (!lbResult && szErr[0])
	{
		gpConEmu->DebugStep(szErr);
#ifdef _DEBUG
		MBoxA(szErr);
#endif
	}

	//return lbResult;
}

BOOL CRealConsole::OpenFarMapData()
{
	BOOL lbResult = FALSE;
	wchar_t szMapName[128], szErr[512]; szErr[0] = 0;
	DWORD dwErr = 0;
	DWORD nFarPID = GetFarPID(TRUE);

	// !!! �����������
	MSectionLock CS; CS.Lock(&ms_FarInfoCS, TRUE);

	//CloseFarMapData();
	//_ASSERTE(m_FarInfo.IsValid() == FALSE);

	// ���� ������ (�������) ����������� - ��� ������ ������������� FAR Mapping!
	if (m_ServerClosing.hServerProcess)
	{
		CloseFarMapData(&CS);
		goto wrap;
	}

	nFarPID = GetFarPID(TRUE);
	if (!nFarPID)
	{
		CloseFarMapData(&CS);
		goto wrap;
	}

	if (m_FarInfo.cbSize && m_FarInfo.nFarPID == nFarPID)
	{
		goto SkipReopen; // ��� �������, ������ �� ������� ������
	}

	// ������ ��� �������������, �����
	m__FarInfo.InitName(CEFARMAPNAME, nFarPID);
	if (!m__FarInfo.Open())
	{
		lstrcpynW(szErr, m__FarInfo.GetErrorText(), countof(szErr));
		goto wrap;
	}

	if (m__FarInfo.Ptr()->nFarPID != nFarPID)
	{
		_ASSERTE(m__FarInfo.Ptr()->nFarPID != nFarPID);
		CloseFarMapData(&CS);
		_wsprintf(szErr, SKIPLEN(countof(szErr)) L"ConEmu: Invalid FAR info format. %s", szMapName);
		goto wrap;
	}

SkipReopen:
	_ASSERTE(m__FarInfo.Ptr()->nProtocolVersion == CESERVER_REQ_VER);
	m__FarInfo.GetTo(&m_FarInfo, sizeof(m_FarInfo));

	m_FarAliveEvent.InitName(CEFARALIVEEVENT, nFarPID);

	if (!m_FarAliveEvent.Open())
	{
		dwErr = GetLastError();

		if (m__FarInfo.IsValid())
		{
			_ASSERTE(m_FarAliveEvent.GetHandle()!=NULL);
		}
	}

	lbResult = TRUE;
wrap:

	if (!lbResult && szErr[0])
	{
		gpConEmu->DebugStep(szErr);
		MBoxA(szErr);
	}

	return lbResult;
}

BOOL CRealConsole::OpenMapHeader(BOOL abFromAttach)
{
	BOOL lbResult = FALSE;
	wchar_t szErr[512]; szErr[0] = 0;
	//int nConInfoSize = sizeof(CESERVER_CONSOLE_MAPPING_HDR);

	if (m_ConsoleMap.IsValid())
	{
		if (hConWnd == (HWND)(m_ConsoleMap.Ptr()->hConWnd))
		{
			_ASSERTE(m_ConsoleMap.Ptr() == NULL);
			return TRUE;
		}
	}

	//_ASSERTE(mh_FileMapping == NULL);
	//CloseMapData();
	m_ConsoleMap.InitName(CECONMAPNAME, (DWORD)hConWnd); //-V205

	if (!m_ConsoleMap.Open())
	{
		lstrcpyn(szErr, m_ConsoleMap.GetErrorText(), countof(szErr));
		//swprintf_c (szErr, L"ConEmu: Can't open console data file mapping. ErrCode=0x%08X. %s", dwErr, ms_HeaderMapName);
		goto wrap;
	}

	//swprintf_c(ms_HeaderMapName, CECONMAPNAME, (DWORD)hConWnd);
	//mh_FileMapping = OpenFileMapping(FILE_MAP_READ/*|FILE_MAP_WRITE*/, FALSE, ms_HeaderMapName);
	//if (!mh_FileMapping) {
	//	DWORD dwErr = GetLastError();
	//	swprintf_c (szErr, L"ConEmu: Can't open console data file mapping. ErrCode=0x%08X. %s", dwErr, ms_HeaderMapName);
	//	goto wrap;
	//}
	//
	//mp_ConsoleInfo = (CESERVER_CONSOLE_MAPPING_HDR*)MapViewOfFile(mh_FileMapping, FILE_MAP_READ/*|FILE_MAP_WRITE*/,0,0,0);
	//if (!mp_ConsoleInfo) {
	//	DWORD dwErr = GetLastError();
	//	wchar_t szErr[512];
	//	swprintf_c (szErr, L"ConEmu: Can't map console info. ErrCode=0x%08X. %s", dwErr, ms_HeaderMapName);
	//	goto wrap;
	//}

	if (!abFromAttach)
	{
		if (m_ConsoleMap.Ptr()->nGuiPID != GetCurrentProcessId())
		{
			_ASSERTE(m_ConsoleMap.Ptr()->nGuiPID == GetCurrentProcessId());
			WARNING("�������� ����� ����� �������� � ������ ��� GUI ��������? � ����� ������ ��� ����� ����������?");
			//PRAGMA_ERROR("�������� ����� ������� ������� ����� GUI PID. ���� ���� �� ����� ����� �����");
		}
	}

	if (m_ConsoleMap.Ptr()->hConWnd && m_ConsoleMap.Ptr()->bDataReady)
	{
		// ������ ���� MonitorThread ��� �� ��� �������
		if (mn_MonitorThreadID == 0)
		{
			_ASSERTE(mp_RBuf==mp_ABuf);
			mp_RBuf->ApplyConsoleInfo();
		}
	}

	lbResult = TRUE;
wrap:

	if (!lbResult && szErr[0])
	{
		gpConEmu->DebugStep(szErr);
		MBoxA(szErr);
	}

	return lbResult;
}

//void CRealConsole::CloseMapData()
//{
//	if (mp_ConsoleData) {
//		UnmapViewOfFile(mp_ConsoleData);
//		mp_ConsoleData = NULL;
//		lstrcpy(ms_ConStatus, L"Console data was not opened!");
//	}
//	if (mh_FileMappingData) {
//		CloseHandle(mh_FileMappingData);
//		mh_FileMappingData = NULL;
//	}
//	mn_LastConsoleDataIdx = mn_LastConsolePacketIdx = /*mn_LastFarReadIdx =*/ -1;
//	mn_LastFarReadTick = 0;
//}

void CRealConsole::CloseFarMapData(MSectionLock* pCS)
{
	MSectionLock CS;
	(pCS ? pCS : &CS)->Lock(&ms_FarInfoCS, TRUE);

	m_FarInfo.cbSize = 0; // �����
	m__FarInfo.CloseMap();

	m_FarAliveEvent.Close();
}

void CRealConsole::CloseMapHeader()
{
	CloseFarMapData();
	//CloseMapData();
	m_GetDataPipe.Close();
	m_ConsoleMap.CloseMap();
	//if (mp_ConsoleInfo) {
	//	UnmapViewOfFile(mp_ConsoleInfo);
	//	mp_ConsoleInfo = NULL;
	//}
	//if (mh_FileMapping) {
	//	CloseHandle(mh_FileMapping);
	//	mh_FileMapping = NULL;
	//}

	if (mp_RBuf) mp_RBuf->ResetBuffer();
	if (mp_EBuf) mp_EBuf->ResetBuffer();
	if (mp_SBuf) mp_SBuf->ResetBuffer();

	mb_DataChanged = TRUE;
}

bool CRealConsole::isAlive()
{
	if (!this) return false;

	if (GetFarPID(TRUE)!=0 && mn_LastFarReadTick /*mn_LastFarReadIdx != (DWORD)-1*/)
	{
		bool lbAlive = false;
		DWORD nLastReadTick = mn_LastFarReadTick;

		if (nLastReadTick)
		{
			DWORD nCurTick = GetTickCount();
			DWORD nDelta = nCurTick - nLastReadTick;

			if (nDelta < FAR_ALIVE_TIMEOUT)
				lbAlive = true;
		}

		return lbAlive;
	}

	return true;
}

LPCWSTR CRealConsole::GetConStatus()
{
	if (hGuiWnd)
		return NULL;
	return ms_ConStatus;
}

void CRealConsole::SetConStatus(LPCWSTR asStatus)
{
	lstrcpyn(ms_ConStatus, asStatus ? asStatus : L"", countof(ms_ConStatus));
	lstrcpyn(CRealConsole::ms_LastRConStatus, ms_ConStatus, countof(CRealConsole::ms_LastRConStatus));

	if (isActive())
	{
		// �������� ����� ���������� ������� �� ������������ ��� �������������
		//if (mp_VCon->Update(true))
		gpConEmu->Invalidate(mp_VCon);

		//gpConEmu->Update(true);
		//if (mp_VCon->Update(false))
		//	gpConEmu->m_Child->Redraw();
	}
}

void CRealConsole::UpdateCursorInfo()
{
	if (!this) return;

	COORD cr; CONSOLE_CURSOR_INFO ci;
	mp_RBuf->GetCursorInfo(&cr, &ci);
	gpConEmu->UpdateCursorInfo(cr, ci);
}

// ����� ���������� �� CVirtualConsole
bool CRealConsole::isCharBorderVertical(WCHAR inChar)
{
	if ((inChar != ucBoxDblHorz && inChar != ucBoxSinglHorz
	        && (inChar >= ucBoxSinglVert && inChar <= ucBoxDblVertHorz))
	        || (inChar >= ucBox25 && inChar <= ucBox75) || inChar == ucBox100
	        || inChar == ucUpScroll || inChar == ucDnScroll)
		return true;
	else
		return false;
}

bool CRealConsole::isCharBorderLeftVertical(WCHAR inChar)
{
	if (inChar < ucBoxSinglHorz || inChar > ucBoxDblVertHorz)
		return false; // ����� ������ ��������� �� ������

	if (inChar == ucBoxDblVert || inChar == ucBoxSinglVert
	        || inChar == ucBoxDblDownRight || inChar == ucBoxSinglDownRight
	        || inChar == ucBoxDblVertRight || inChar == ucBoxDblVertSinglRight
	        || inChar == ucBoxSinglVertRight
	        || (inChar >= ucBox25 && inChar <= ucBox75) || inChar == ucBox100
	        || inChar == ucUpScroll || inChar == ucDnScroll)
		return true;
	else
		return false;
}

// ����� ���������� �� CVirtualConsole
bool CRealConsole::isCharBorderHorizontal(WCHAR inChar)
{
	if (inChar == ucBoxSinglDownDblHorz || inChar == ucBoxSinglUpDblHorz
			|| inChar == ucBoxDblDownDblHorz || inChar == ucBoxDblUpDblHorz
	        || inChar == ucBoxSinglDownHorz || inChar == ucBoxSinglUpHorz || inChar == ucBoxDblUpSinglHorz
	        || inChar == ucBoxDblHorz)
		return true;
	else
		return false;
}

bool CRealConsole::GetMaxConSize(COORD* pcrMaxConSize)
{
	bool bOk = false;

	//if (mp_ConsoleInfo)
	if (m_ConsoleMap.IsValid())
	{
		if (pcrMaxConSize)
			*pcrMaxConSize = m_ConsoleMap.Ptr()->crMaxConSize;

		bOk = true;
	}

	return bOk;
}

int CRealConsole::GetDetectedDialogs(int anMaxCount, SMALL_RECT* rc, DWORD* rf)
{
	if (!this || !mp_ABuf)
		return 0;

	return mp_ABuf->GetDetector()->GetDetectedDialogs(anMaxCount, rc, rf);
	//int nCount = min(anMaxCount,m_DetectedDialogs.Count);
	//if (nCount>0) {
	//	if (rc)
	//		memmove(rc, m_DetectedDialogs.Rects, nCount*sizeof(SMALL_RECT));
	//	if (rb)
	//		memmove(rb, m_DetectedDialogs.bWasFrame, nCount*sizeof(bool));
	//}
	//return nCount;
}

const CRgnDetect* CRealConsole::GetDetector()
{
	if (!this)
		return NULL;
	return mp_ABuf->GetDetector();
}

// ������������� ���������� ���������� ������� � ���������� ������ ������
// (������� ����� ������� ������� ������ � ��������������� ������� ������)
bool CRealConsole::ConsoleRect2ScreenRect(const RECT &rcCon, RECT *prcScr)
{
	if (!this) return false;
	return mp_ABuf->ConsoleRect2ScreenRect(rcCon, prcScr);
}

DWORD CRealConsole::PostMacroThread(LPVOID lpParameter)
{
	PostMacroAnyncArg* pArg = (PostMacroAnyncArg*)lpParameter;
	if (pArg->bPipeCommand)
	{
		CConEmuPipe pipe(pArg->pRCon->GetFarPID(TRUE), CONEMUREADYTIMEOUT);
		if (pipe.Init(_T("CRealConsole::PostMacroThread"), TRUE))
		{
			gpConEmu->DebugStep(_T("ProcessFarHyperlink: Waiting for result (10 sec)"));
			pipe.Execute(pArg->nCmdID, pArg->Data, pArg->nCmdSize);
			gpConEmu->DebugStep(NULL);
		}
	}
	else
	{
		pArg->pRCon->PostMacro(pArg->szMacro, FALSE/*������ - ����� Sync*/);
	}
	free(pArg);
	return 0;
}

void CRealConsole::PostCommand(DWORD anCmdID, DWORD anCmdSize, LPCVOID ptrData)
{
	if (mh_PostMacroThread != NULL)
	{
		DWORD nWait = WaitForSingleObject(mh_PostMacroThread, 0);
		if (nWait == WAIT_OBJECT_0)
		{
			CloseHandle(mh_PostMacroThread);
			mh_PostMacroThread = NULL;
		}
		else
		{
			// ������ ���� NULL, ���� ��� - ������ ����� ����������� ������
			_ASSERTE(mh_PostMacroThread==NULL);
			TerminateThread(mh_PostMacroThread, 100);
			CloseHandle(mh_PostMacroThread);
		}
	}

	size_t nArgSize = sizeof(PostMacroAnyncArg) + anCmdSize;
	PostMacroAnyncArg* pArg = (PostMacroAnyncArg*)calloc(1,nArgSize);
	pArg->pRCon = this;
	pArg->bPipeCommand = TRUE;
	pArg->nCmdID = anCmdID;
	pArg->nCmdSize = anCmdSize;
	if (ptrData && anCmdSize)
		memmove(pArg->Data, ptrData, anCmdSize);
	mh_PostMacroThread = CreateThread(NULL, 0, PostMacroThread, pArg, 0, &mn_PostMacroThreadID);	
	if (mh_PostMacroThread == NULL)
	{
		// ���� �� ������� ��������� ����
		MBoxAssert(mh_PostMacroThread!=NULL);
		free(pArg);
	}
	return;
}

void CRealConsole::PostMacro(LPCWSTR asMacro, BOOL abAsync /*= FALSE*/)
{
	if (!this || !asMacro || !*asMacro)
		return;

	DWORD nPID = GetFarPID(TRUE/*abPluginRequired*/);

	if (!nPID)
		return;

	if (abAsync)
	{
		if (mh_PostMacroThread != NULL)
		{
			DWORD nWait = WaitForSingleObject(mh_PostMacroThread, 0);
			if (nWait == WAIT_OBJECT_0)
			{
				CloseHandle(mh_PostMacroThread);
				mh_PostMacroThread = NULL;
			}
			else
			{
				// ������ ���� NULL, ���� ��� - ������ ����� ����������� ������
				_ASSERTE(mh_PostMacroThread==NULL);
				TerminateThread(mh_PostMacroThread, 100);
				CloseHandle(mh_PostMacroThread);
			}
		}

		size_t nLen = _tcslen(asMacro);
		size_t nArgSize = sizeof(PostMacroAnyncArg) + nLen*sizeof(*asMacro);
		PostMacroAnyncArg* pArg = (PostMacroAnyncArg*)calloc(1,nArgSize);
		pArg->pRCon = this;
		pArg->bPipeCommand = FALSE;
		_wcscpy_c(pArg->szMacro, nLen+1, asMacro);
		mh_PostMacroThread = CreateThread(NULL, 0, PostMacroThread, pArg, 0, &mn_PostMacroThreadID);	
		if (mh_PostMacroThread == NULL)
		{
			// ���� �� ������� ��������� ����
			MBoxAssert(mh_PostMacroThread!=NULL);
			free(pArg);
		}
		return;
	}

#ifdef _DEBUG
	DEBUGSTRMACRO(asMacro); OutputDebugStringW(L"\n");
#endif
	CConEmuPipe pipe(nPID, CONEMUREADYTIMEOUT);

	if (pipe.Init(_T("CRealConsole::PostMacro"), TRUE))
	{
		//DWORD cbWritten=0;
		gpConEmu->DebugStep(_T("Macro: Waiting for result (10 sec)"));
		pipe.Execute(CMD_POSTMACRO, asMacro, (_tcslen(asMacro)+1)*2);
		gpConEmu->DebugStep(NULL);
	}
}

bool CRealConsole::Detach()
{
	if (!this)
		return false;

	if (hGuiWnd)
	{
		if (MessageBox(NULL, L"Detach GUI application from ConEmu?", GetTitle(), MB_ICONQUESTION|MB_YESNO|MB_DEFBUTTON2) != IDYES)
			return false;

		//#ifdef _DEBUG
		//WINDOWPLACEMENT wpl = {sizeof(wpl)};
		//GetWindowPlacement(hGuiWnd, &wpl); // ���� ���������� ����������
		//#endif
		
		RECT rcGui = rcPreGuiWndRect;
		GetWindowRect(hGuiWnd, &rcGui); // �������� ��� �� �������� ���������� � ��� �� �����
	
		ShowOtherWindow(hGuiWnd, SW_HIDE, FALSE/*���������*/);
		SetOtherWindowParent(hGuiWnd, NULL);
		SetOtherWindowPos(hGuiWnd, HWND_NOTOPMOST, rcGui.left, rcGui.top, rcGui.right-rcGui.left, rcGui.bottom-rcGui.top, SWP_SHOWWINDOW);
		// �������� ����������, ����� ��� ������� �� ��������
		hGuiWnd = NULL;
		// ������� �������
		CloseConsole(FALSE);
	}
	else
	{
		if (MessageBox(NULL, L"Detach console from ConEmu?", GetTitle(), MB_ICONQUESTION|MB_YESNO|MB_DEFBUTTON2) != IDYES)
			return false;
	
		ShowConsole(1);
		// ��������� ������, ��� �� ������ �� ���
		CESERVER_REQ in;
		ExecutePrepareCmd(&in, CECMD_DETACHCON, sizeof(CESERVER_REQ_HDR));
		DWORD dwTickStart = timeGetTime();
		
		CESERVER_REQ *pOut = ExecuteSrvCmd(GetServerPID(), &in, ghWnd);
		
		gpSetCls->debugLogCommand(&in, FALSE, dwTickStart, timeGetTime()-dwTickStart, L"ExecuteSrvCmd", pOut);
		
		if (pOut) ExecuteFreeResult(pOut);
	}

	// ����� �������� �� ������� RealConsole?
	m_Args.bDetached = TRUE;
	return true;
}

// ��������� Elevated ����� ���� � ���� �� ������� �� �������
void CRealConsole::AdminDuplicate()
{
	if (!this) return;

	TODO("��������� Elevated ��� � ����������� - �������� ������");
}

const CEFAR_INFO_MAPPING* CRealConsole::GetFarInfo()
{
	if (!this) return NULL;

	//return m_FarInfo.Ptr(); -- ������, ����� ���� ������ � ������ ������!

	if (!m_FarInfo.cbSize)
		return NULL;
	return &m_FarInfo;
}

/*LPCWSTR CRealConsole::GetLngNameTime()
{
	if (!this) return NULL;
	return ms_NameTitle;
}*/

BOOL CRealConsole::InCreateRoot()
{
	return (mb_InCreateRoot && !mn_ConEmuC_PID);
}

// ����� �� � ���� ������� ��������� GUI ����������
BOOL CRealConsole::GuiAppAttachAllowed(LPCWSTR asAppFileName, DWORD anAppPID)
{
	if (!this)
		return false;
	// ���� ���� ������ ��� �� �������
	if (InCreateRoot())
		return false;

	int nProcCount = GetProcesses(NULL);
	DWORD nActivePID = GetActivePID();

	// ���������� ��� ����. ������ (��� ������� exe) ahGuiWnd==NULL, ������ - ����� ������������ �������� ����

	if (nProcCount > 0 && nActivePID == anAppPID)
		return true; // ��� ����������� ������ � ����� ����������, � ������ ���� ������� ����

	// ����� - ��������� ������
	if ((nProcCount > 1) || (nActivePID != 0))
		return false;

	// ���������, �������� �� asAppFileName � ����������
	LPCWSTR pszCmd = GetCmd();
	if (pszCmd && *pszCmd && asAppFileName && *asAppFileName)
	{
		wchar_t szApp[MAX_PATH+1], szArg[MAX_PATH+1];
		LPCWSTR pszArg = NULL, pszApp = NULL, pszOnly = NULL;

		while (pszCmd[0] == L'"' && pszCmd[1] == L'"')
			pszCmd++;

		pszOnly = PointToName(pszCmd);

		// ������ ��, ��� �������� (������ �� GUI ����������)
		pszApp = PointToName(asAppFileName);
		lstrcpyn(szApp, pszApp, countof(szApp));
		wchar_t* pszDot = wcsrchr(szApp, L'.'); // ����������?
		CharUpperBuff(szApp, lstrlen(szApp));

		if (NextArg(&pszCmd, szArg, &pszApp) == 0)
		{
			// ��� �������� ��������� � �������
			CharUpperBuff(szArg, lstrlen(szArg));
			pszArg = PointToName(szArg);
			if (lstrcmp(pszArg, szApp) == 0)
				return true;
			if (!wcschr(pszArg, L'.') && pszDot)
			{
				*pszDot = 0;
				if (lstrcmp(pszArg, szApp) == 0)
					return true;
				*pszDot = L'.';
			}
		}

		// ����� ��� ������� ���, � ���� � ��������
		lstrcpyn(szArg, pszOnly, countof(szArg));
		CharUpperBuff(szArg, lstrlen(szArg));
		if (lstrcmp(szArg, szApp) == 0)
			return true;
		if (pszArg && !wcschr(pszArg, L'.') && pszDot)
		{
			*pszDot = 0;
			if (lstrcmp(pszArg, szApp) == 0)
				return true;
			*pszDot = L'.';
		}

		return false;
	}

	_ASSERTE(pszCmd && *pszCmd && asAppFileName && *asAppFileName);
	return true;
}

void CRealConsole::ShowPropertiesDialog()
{
	if (!this)
		return;
	
	// ���� � RealConsole ��� ���� ������ ������� SC_PROPERTIES_SECRET,
	// �� ��� �������� ������ �� ���� (!) �������� �������� - ConHost ������!
	// �������, ������� ���������� ����� ������ ��������, � ������ ���� ��� - �������� msg
	HWND hConProp = NULL;
	wchar_t szTitle[255]; int nDefLen = _tcslen(CEC_INITTITLE);
	// � ���������, ��� �� ��������� ����� ���� �������, ���� ������� ����
	// ������� �� ����� ConEmu (��������, ��� ������� Far, � ����� ������ Attach).
	while ((hConProp = FindWindowEx(NULL, hConProp, (LPCWSTR)32770, NULL)) != NULL)
	{
		if (GetWindowText(hConProp, szTitle, countof(szTitle))
			&& szTitle[0] == L'"' && szTitle[nDefLen+1] == L'"'
			&& !wmemcmp(szTitle+1, CEC_INITTITLE, nDefLen))
		{
			apiSetForegroundWindow(hConProp);
			return; // �����, ����������!
		}
	}

	POSTMESSAGE(ghConWnd, WM_SYSCOMMAND, SC_PROPERTIES_SECRET/*65527*/, 0, TRUE);
}

//void CRealConsole::LogShellStartStop()
//{
//	// ���� - ������ ��� ������� Far-�������
//	DWORD nFarPID = GetFarPID(TRUE);
//
//	if (!nFarPID)
//		return;
//
//	if (!mb_ShellActivityLogged)
//	{
//		OPENFILENAME ofn; memset(&ofn,0,sizeof(ofn));
//		ofn.lStructSize=sizeof(ofn);
//		ofn.hwndOwner = ghWnd;
//		ofn.lpstrFilter = _T("Log files (*.log)\0*.log\0\0");
//		ofn.nFilterIndex = 1;
//
//		if (ms_LogShellActivity[0] == 0)
//		{
//			lstrcpyn(ms_LogShellActivity, gpConEmu->ms_ConEmuCurDir, MAX_PATH-32);
//			int nCurLen = _tcslen(ms_LogShellActivity);
//			_wsprintf(ms_LogShellActivity+nCurLen, SKIPLEN(countof(ms_LogShellActivity)-nCurLen)
//			          L"\\ShellLog-%u.log", nFarPID);
//		}
//
//		ofn.lpstrFile = ms_LogShellActivity;
//		ofn.nMaxFile = countof(ms_LogShellActivity);
//		ofn.lpstrTitle = L"Log CreateProcess...";
//		ofn.lpstrDefExt = L"log";
//		ofn.Flags = OFN_ENABLESIZING|OFN_NOCHANGEDIR
//		            | OFN_PATHMUSTEXIST|OFN_EXPLORER|OFN_HIDEREADONLY|OFN_OVERWRITEPROMPT;
//
//		if (!GetSaveFileName(&ofn))
//			return;
//
//		mb_ShellActivityLogged = true;
//	}
//	else
//	{
//		mb_ShellActivityLogged = false;
//	}
//
//	//TODO: �������� �� ���� m_ConsoleMap<CESERVER_CONSOLE_MAPPING_HDR>.sLogCreateProcess
//	// ��������� � �������
//	CConEmuPipe pipe(nFarPID, 300);
//
//	if (pipe.Init(L"LogShell", TRUE))
//	{
//		LPCVOID pData; wchar_t wch = 0;
//		UINT nDataSize;
//
//		if (mb_ShellActivityLogged && ms_LogShellActivity[0])
//		{
//			pData = ms_LogShellActivity;
//			nDataSize = (_tcslen(ms_LogShellActivity)+1)*2;
//		}
//		else
//		{
//			pData = &wch;
//			nDataSize = 2;
//		}
//
//		pipe.Execute(CMD_LOG_SHELL, pData, nDataSize);
//	}
//}

//bool CRealConsole::IsLogShellStarted()
//{
//	return mb_ShellActivityLogged && ms_LogShellActivity[0];
//}

DWORD CRealConsole::GetConsoleCP()
{
	/*return con.m_dwConsoleCP;*/
	return mp_RBuf->GetConsoleCP();
}

DWORD CRealConsole::GetConsoleOutputCP()
{
	/*return con.m_dwConsoleOutputCP;*/
	return mp_RBuf->GetConsoleOutputCP();
}

DWORD CRealConsole::GetConsoleMode()
{
	/*return con.m_dwConsoleMode;*/
	return mp_RBuf->GetConMode();
}