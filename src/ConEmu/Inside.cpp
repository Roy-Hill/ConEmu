
/*
Copyright (c) 2012 Maximus5
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

#ifndef HIDE_USE_EXCEPTION_INFO
#define HIDE_USE_EXCEPTION_INFO
#endif

#ifndef SHOWDEBUGSTR
#define SHOWDEBUGSTR
#endif

#define DEBUGSTRINSIDE(s) //DEBUGSTR(s)

#include "header.h"
#include <Tlhelp32.h>

#include "ConEmu.h"
#include "Inside.h"
#include "Options.h"
#include "RealConsole.h"
#include "VConRelease.h"
#include "VirtualConsole.h"


#define EMID_TIPOFDAY 41536


CConEmuInside::CConEmuInside()
{
	m_InsideIntegration = ii_None; mb_InsideIntegrationShift = false; mn_InsideParentPID = 0;
	mb_InsideSynchronizeCurDir = false;
	ms_InsideSynchronizeCurDir = NULL;
	mb_InsidePaneWasForced = false;
	mh_InsideParentRoot = mh_InsideParentWND = mh_InsideParentRel = NULL;
	mh_InsideParentPath = mh_InsideParentCD = NULL; ms_InsideParentPath[0] = 0;
	mb_TipPaneWasShown = false;
	//mh_InsideSysMenu = NULL;
}

bool CConEmuInside::InitInside(bool bRunAsAdmin, bool bSyncDir, LPCWSTR pszSyncDirCmdFmt, DWORD nParentPID, HWND hParentWnd)
{
	CConEmuInside* pInside = gpConEmu->mp_Inside;
	if (!pInside)
	{
		pInside = new CConEmuInside();
		if (!pInside)
		{
			return false;
		}
		gpConEmu->mp_Inside = pInside;
	}

	pInside->mn_InsideParentPID = nParentPID;
	pInside->mh_InsideParentWND = hParentWnd;

	pInside->m_InsideIntegration = (hParentWnd == NULL) ? ii_Auto : ii_Simple;
	
	pInside->mb_InsideIntegrationShift = bRunAsAdmin /*isPressed(VK_SHIFT)*/;

	if (bSyncDir)
	{
		pInside->mb_InsideSynchronizeCurDir = true;
		pInside->ms_InsideSynchronizeCurDir = lstrdup(pszSyncDirCmdFmt); // \eCD /d %1 - \e - ESC, \b - BS, \n - ENTER, %1 - "dir", %2 - "bash dir"
	}
	else
	{
		_ASSERTE(pInside->ms_InsideSynchronizeCurDir==NULL);
		pInside->mb_InsideSynchronizeCurDir = false;
	}

	return true;
}

CConEmuInside::~CConEmuInside()
{
	// If "Tip pane" was shown by ConEmu - hide pane after ConEmu close
	if (mb_TipPaneWasShown)
	{
		PostMessage(mh_InsideParentRoot, WM_COMMAND, EMID_TIPOFDAY/*41536*/, 0);
	}

	SafeFree(ms_InsideSynchronizeCurDir);
}

BOOL CConEmuInside::EnumInsideFindParent(HWND hwnd, LPARAM lParam)
{
	DWORD nPID = 0;
	if (IsWindowVisible(hwnd)
		&& GetWindowThreadProcessId(hwnd, &nPID)
		&& (nPID == (DWORD)lParam))
	{
		DWORD nNeedStyles = WS_CAPTION|WS_SYSMENU|WS_THICKFRAME|WS_MINIMIZEBOX|WS_MAXIMIZEBOX;
		DWORD nStyles = GetWindowLong(hwnd, GWL_STYLE);
		if ((nStyles & nNeedStyles) == nNeedStyles)
		{
			// �����
			gpConEmu->mp_Inside->mn_InsideParentPID = nPID;
			gpConEmu->mp_Inside->mh_InsideParentRoot = hwnd;
			return FALSE;
		}
	}

	// Next window
	return TRUE;
}

HWND  CConEmuInside::InsideFindConEmu(HWND hFrom)
{
	wchar_t szClass[128];
	HWND hChild = NULL, hNext = NULL;
	//HWND hXpView = NULL, hXpPlace = NULL;

	while ((hChild = FindWindowEx(hFrom, hChild, NULL, NULL)) != NULL)
	{
		GetClassName(hChild, szClass, countof(szClass));
		if (lstrcmp(szClass, gsClassNameParent) == 0)
		{
			return hChild;
		}

		hNext = InsideFindConEmu(hChild);
		if (hNext)
		{
			return hNext;
		}
	}

	return NULL;
}

bool CConEmuInside::InsideFindShellView(HWND hFrom)
{
	wchar_t szClass[128];
	wchar_t szParent[128];
	wchar_t szRoot[128];
	HWND hChild = NULL;
	// ��� WinXP
	HWND hXpView = NULL, hXpPlace = NULL;

	while ((hChild = FindWindowEx(hFrom, hChild, NULL, NULL)) != NULL)
	{
		// ��� ���������� ������ ������� ����!
		if (!IsWindowVisible(hChild))
			continue;

		// Windows 7, Windows 8.
		GetClassName(hChild, szClass, countof(szClass));
		if (lstrcmp(szClass, L"SHELLDLL_DefView") == 0)
		{
			GetClassName(hFrom, szParent, countof(szParent));
			if (lstrcmp(szParent, L"CtrlNotifySink") == 0)
			{
				HWND hParent = GetParent(hFrom);
				if (hParent)
				{
					GetClassName(hParent, szRoot, countof(szRoot));
					_ASSERTE(lstrcmp(szRoot, L"DirectUIHWND") == 0);

					mh_InsideParentWND = hParent;
					mh_InsideParentRel = hFrom;
					m_InsideIntegration = ii_Explorer;

					return true;
				}
			}
			else if ((gnOsVer < 0x600) && (lstrcmp(szParent, L"ExploreWClass") == 0))
			{
				_ASSERTE(mh_InsideParentRoot == hFrom);
				hXpView = hChild;
			}
		}
		else if ((gnOsVer < 0x600) && (lstrcmp(szClass, L"BaseBar") == 0))
		{
			RECT rcBar = {}; GetWindowRect(hChild, &rcBar);
			MapWindowPoints(NULL, hFrom, (LPPOINT)&rcBar, 2);
			RECT rcParent = {}; GetClientRect(hFrom, &rcParent);
			if ((-10 <= (rcBar.right - rcParent.right))
				&& ((rcBar.right - rcParent.right) <= 10))
			{
				// ��� ���������� �������, ������������ � �������-������� ����
				hXpPlace = hChild;
			}
		}
		// ���� � ���� (hChild) �������� � ������� "Address: D:\users\max"
		else if ((gnOsVer >= 0x600) && lstrcmp(szClass, L"ToolbarWindow32") == 0)
		{
			GetClassName(hFrom, szParent, countof(szParent));
			if (lstrcmp(szParent, L"Breadcrumb Parent") == 0)
			{
				HWND hParent = GetParent(hFrom);
				if (hParent)
				{
					GetClassName(hParent, szRoot, countof(szRoot));
					_ASSERTE(lstrcmp(szRoot, L"msctls_progress32") == 0);

					mh_InsideParentPath = hChild;

					// �������� ComboBox/Edit, � ������� ����� ��������� ����, ����� ��������� ��������� �� ���� �������
					// �� ���� ��������. ���� ������� �� ��������� ��� �������� ����!

					return true;
				}
			}
		}

		if ((hChild != hXpView) && (hChild != hXpPlace))
		{
			if (InsideFindShellView(hChild))
			{
				if (mh_InsideParentRel && mh_InsideParentPath)
					return true;
				else
					break;
			}
		}

		if (hXpView && hXpPlace)
		{
			mh_InsideParentRel = FindWindowEx(hXpPlace, NULL, L"ReBarWindow32", NULL);
			if (!mh_InsideParentRel)
			{
				_ASSERTE(mh_InsideParentRel && L"ReBar must be found on XP & 2k3");
				return true; // ��������� �����
			}
			mh_InsideParentWND = hXpPlace;
			mh_InsideParentPath = mh_InsideParentRoot;
			m_InsideIntegration = ii_Explorer;
			return true;
		}
	}

	return false;
}

// ���������� ��� ������������� �� Settings::LoadSettings()
HWND CConEmuInside::InsideFindParent()
{
	bool bFirstStep = true;
	DWORD nParentPID = 0;

	if (!m_InsideIntegration)
	{
		return NULL;
	}

	if (mh_InsideParentWND)
	{
		if (IsWindow(mh_InsideParentWND))
		{
			if (m_InsideIntegration == ii_Simple)
			{
				if (mh_InsideParentRoot == NULL)
				{
					// ���� ��� �� ������ "��������" ����
					HWND hParent = mh_InsideParentWND;
					while (hParent)
					{
						mh_InsideParentRoot = hParent;
						hParent = GetParent(hParent);
					}
				}
				// � ���� ������ �������� ��� ���������� �������
				_ASSERTE(mh_InsideParentRel==NULL);
				mh_InsideParentRel = NULL;
			}

			_ASSERTE(mh_InsideParentWND!=NULL);
			goto wrap;
		}
		else
		{
			if (m_InsideIntegration == ii_Simple)
			{
				DisplayLastError(L"Specified window not found");
				mh_InsideParentWND = NULL;
				goto wrap;
			}
			_ASSERTE(IsWindow(mh_InsideParentWND));
			mh_InsideParentRoot = mh_InsideParentWND = mh_InsideParentRel = NULL;
		}
	}

	_ASSERTE(m_InsideIntegration!=ii_Simple);

	if (mn_InsideParentPID)
	{
		PROCESSENTRY32 pi = {sizeof(pi)};
		if ((mn_InsideParentPID == GetCurrentProcessId())
			|| !GetProcessInfo(mn_InsideParentPID, &pi))
		{
			DisplayLastError(L"Invalid parent process specified");
			m_InsideIntegration = ii_None;
			mh_InsideParentWND = NULL;
			goto wrap;
		}
		nParentPID = mn_InsideParentPID;
	}
	else
	{
		PROCESSENTRY32 pi = {sizeof(pi)};
		if (!GetProcessInfo(GetCurrentProcessId(), &pi) || !pi.th32ParentProcessID)
		{
			DisplayLastError(L"GetProcessInfo(GetCurrentProcessId()) failed");
			m_InsideIntegration = ii_None;
			mh_InsideParentWND = NULL;
			goto wrap;
		}
		nParentPID = pi.th32ParentProcessID;
	}

	EnumWindows(EnumInsideFindParent, nParentPID);
	if (!mh_InsideParentRoot)
	{
		int nBtn = MessageBox(L"Can't find appropriate parent window!\n\nContinue in normal mode?", MB_ICONSTOP|MB_YESNO|MB_DEFBUTTON2);
		if (nBtn != IDYES)
		{
			mh_InsideParentWND = INSIDE_PARENT_NOT_FOUND;
			return mh_InsideParentWND; // ���������!
		}
		// ���������� � ������� ������
		m_InsideIntegration = ii_None;
		mh_InsideParentWND = NULL;
		goto wrap;
	}


    HWND hExistConEmu;
    if ((hExistConEmu = InsideFindConEmu(mh_InsideParentRoot)) != NULL)
    {
    	_ASSERTE(FALSE && "Continue to create tab in existing instance");
    	// ���� � ���������� ��� ���� ConEmu - ������� � ��� ����� �������
    	gpSetCls->SingleInstanceShowHide = sih_None;
    	LPCWSTR pszCmdLine = GetCommandLine();
    	LPCWSTR pszCmd = StrStrI(pszCmdLine, L" /cmd ");
    	gpConEmu->RunSingleInstance(hExistConEmu, pszCmd ? (pszCmd + 6) : NULL);

		mh_InsideParentWND = INSIDE_PARENT_NOT_FOUND;
		return mh_InsideParentWND; // ���������!
    }

	// ������ ����� ����� �������� ����
	// 1. � ������� ����� ����������
	// 2. �� �������� ����� �����������������
	// 3. ��� ������������� �������� ����
	InsideFindShellView(mh_InsideParentRoot);

RepeatCheck:
	if (!mh_InsideParentWND || (!mh_InsideParentRel && (m_InsideIntegration == ii_Explorer)))
	{
		wchar_t szAddMsg[128] = L"", szMsg[1024];
		if (bFirstStep)
		{
			bFirstStep = false;

			if (TurnExplorerTipPane(szAddMsg))
			{
				goto RepeatCheck;
			}
		}

		//MessageBox(L"Can't find appropriate shell window!", MB_ICONSTOP);
		_wsprintf(szMsg, SKIPLEN(countof(szMsg)) L"%sCan't find appropriate shell window!\nUnrecognized layout of the Explorer.\n\nContinue in normal mode?", szAddMsg);
		int nBtn = MessageBox(szMsg, MB_ICONSTOP|MB_YESNO|MB_DEFBUTTON2);

		if (nBtn != IDYES)
		{
			mh_InsideParentWND = INSIDE_PARENT_NOT_FOUND;
			return mh_InsideParentWND; // ���������!
		}
		m_InsideIntegration = ii_None;
		mh_InsideParentRoot = NULL;
		mh_InsideParentWND = NULL;
		goto wrap;
	}

wrap:
	if (!mh_InsideParentWND)
	{
		m_InsideIntegration = ii_None;
		mh_InsideParentRoot = NULL;
	}
	else
	{
		GetWindowThreadProcessId(mh_InsideParentWND, &mn_InsideParentPID);
		// ��� ����������� �����
		GetCurrentDirectory(countof(ms_InsideParentPath), ms_InsideParentPath);
		int nLen = lstrlen(ms_InsideParentPath);
		if ((nLen > 3) && (ms_InsideParentPath[nLen-1] == L'\\'))
		{
			ms_InsideParentPath[nLen-1] = 0;
		}
	}

	return mh_InsideParentWND;
}

bool CConEmuInside::TurnExplorerTipPane(wchar_t (&szAddMsg)[128])
{
	bool bRepeat = false;
	int nBtn = IDCANCEL;
	DWORD_PTR nRc;
	LRESULT lSendRc;

	if (gnOsVer < 0x600)
	{
		// WinXP, Win2k3
		nBtn = IDYES; // MessageBox(L"Tip pane is not found in Explorer window!\nThis pane is required for 'ConEmu Inside' mode.\nDo you want to show this pane?", MB_ICONQUESTION|MB_YESNO);
		if (nBtn == IDYES)
		{

		#if 0

			HWND hWorker = GetDlgItem(mh_InsideParentRoot, 0xA005);

			#ifdef _DEBUG
			if (hWorker)
			{
				HWND hReBar = FindWindowEx(hWorker, NULL, L"ReBarWindow32", NULL);
				if (hReBar)
				{
					POINT pt = {4,4};
					HWND hTool = ChildWindowFromPoint(hReBar, pt);
					if (hTool)
					{
						//TBBUTTON tb = {};
						//lSendRc = SendMessageTimeout(mh_InsideParentRoot, TB_GETBUTTON, 2, (LPARAM)&tb, SMTO_NOTIMEOUTIFNOTHUNG, 500, &nRc);
						//if (tb.idCommand)
						//{
						//	NMTOOLBAR nm = {{hTool, 0, TBN_DROPDOWN}, 32896/*tb.idCommand*/};
						//	lSendRc = SendMessageTimeout(mh_InsideParentRoot, WM_NOTIFY, 0, (LPARAM)&nm, SMTO_NOTIMEOUTIFNOTHUNG, 500, &nRc);
						//}
						lSendRc = SendMessageTimeout(mh_InsideParentRoot, WM_COMMAND, 32896, 0, SMTO_NOTIMEOUTIFNOTHUNG, 500, &nRc);
					}
				}
			}
			// There is no way to force "Tip pane" in WinXP
			// if popup menu "View -> Explorer bar" was not _shown_ at least once
			// In that case, Explorer ignores WM_COMMAND(41536) and does not reveal tip pane.
			HMENU hMenu1 = GetMenu(mh_InsideParentRoot), hMenu2 = NULL, hMenu3 = NULL;
			if (hMenu1)
			{
				hMenu2 = GetSubMenu(hMenu1, 2);
				//if (!hMenu2)
				//{
				//	lSendRc = SendMessageTimeout(mh_InsideParentRoot, WM_MENUSELECT, MAKELONG(2,MF_POPUP), (LPARAM)hMenu1, SMTO_NOTIMEOUTIFNOTHUNG, 1500, &nRc);
				//	hMenu2 = GetSubMenu(hMenu1, 2);
				//}

				if (hMenu2)
				{
					hMenu3 = GetSubMenu(hMenu2, 2);
				}
			}
			#endif
		#endif

			//INPUT keys[4] = {INPUT_KEYBOARD,INPUT_KEYBOARD,INPUT_KEYBOARD,INPUT_KEYBOARD};
			//keys[0].ki.wVk = VK_F10; keys[0].ki.dwFlags = 0; //keys[0].ki.wScan = MapVirtualKey(VK_F10,MAPVK_VK_TO_VSC);
			//keys[1].ki.wVk = VK_F10; keys[1].ki.dwFlags = KEYEVENTF_KEYUP; //keys[0].ki.wScan = MapVirtualKey(VK_F10,MAPVK_VK_TO_VSC);
			//keys[1].ki.wVk = 'V';
			//keys[2].ki.wVk = 'E';
			//keys[3].ki.wVk = 'T';

			//LRESULT lSentRc = 0;
			//SetForegroundWindow(mh_InsideParentRoot);
			//LRESULT lSentRc = SendInput(2/*countof(keys)*/, keys, sizeof(keys[0]));
			//lSentRc = SendMessageTimeout(mh_InsideParentRoot, WM_SYSKEYUP, 'V', 0, SMTO_NOTIMEOUTIFNOTHUNG, 5000, &nRc);

			lSendRc = SendMessageTimeout(mh_InsideParentRoot, WM_COMMAND, EMID_TIPOFDAY/*41536*/, 0, SMTO_NOTIMEOUTIFNOTHUNG, 5000, &nRc);
			UNREFERENCED_PARAMETER(lSendRc);

			mb_InsidePaneWasForced = true;
			// ���������� ��������� ���� �� �������
			wcscpy_c(szAddMsg, L"Forcing Explorer to show Tip pane failed.\nShow pane yourself and recall ConEmu.\n\n");
		}
	}

	if (nBtn == IDYES)
	{
		// ������ ��������
		mh_InsideParentWND = mh_InsideParentRel = NULL;
		m_InsideIntegration = ii_Auto;
		InsideFindShellView(mh_InsideParentRoot);
		if (mh_InsideParentWND && mh_InsideParentRel)
		{
			bRepeat = true;
			goto wrap;
		}
		// ���� �� ����� - �������� � ��������� ��������
		Sleep(500);
		mh_InsideParentWND = mh_InsideParentRel = NULL;
		m_InsideIntegration = ii_Auto;
		InsideFindShellView(mh_InsideParentRoot);
		if (mh_InsideParentWND && mh_InsideParentRel)
		{
			bRepeat = true;
			goto wrap;
		}

		// Last chance - try to post key sequence "F10 Left Left Down Down Down Left"
		// This will opens popup menu containing "Tip of the day" menuitem
		// "Esc Esc Esc" it and post {WM_COMMAND,EMID_TIPOFDAY} again
		WORD vkPostKeys[] = {VK_ESCAPE, VK_ESCAPE, VK_ESCAPE, VK_RIGHT, VK_RIGHT, VK_RIGHT, VK_DOWN, VK_DOWN, VK_DOWN, VK_RIGHT, VK_ESCAPE, VK_ESCAPE, VK_ESCAPE, 0};

		// Go
		if (SendVkKeySequence(mh_InsideParentRoot, vkPostKeys))
		{
			// Try again (Tip of the day)
			lSendRc = SendMessageTimeout(mh_InsideParentRoot, WM_COMMAND, EMID_TIPOFDAY/*41536*/, 0, SMTO_NOTIMEOUTIFNOTHUNG, 5000, &nRc);
			// Wait and check again
			Sleep(500);
			mh_InsideParentWND = mh_InsideParentRel = NULL;
			m_InsideIntegration = ii_Auto;
			InsideFindShellView(mh_InsideParentRoot);
			if (mh_InsideParentWND && mh_InsideParentRel)
			{
				bRepeat = true;
				goto wrap;
			}
		}
	}

wrap:
	if (bRepeat)
	{
		mb_TipPaneWasShown = true;
	}
	return bRepeat;
}

static bool CheckClassName(HWND h, LPCWSTR pszNeed)
{
	wchar_t szClass[128];
	if (!GetClassName(h, szClass, countof(szClass)))
		return false;

	int nCmp = lstrcmp(szClass, pszNeed);

	return (nCmp == 0);
}

static HWND FindTopWindow(HWND hParent, LPCWSTR sClass)
{
	HWND hLast = NULL;
	HWND hFind = NULL;
	int Coord = 99999;
	while ((hFind = FindWindowEx(hParent, hFind, sClass, NULL)) != NULL)
	{
		RECT rc; GetWindowRect(hFind, &rc);
		if ((hLast == NULL)
			|| (rc.top < Coord))
		{
			Coord = rc.top;
			hLast = hFind;
		}
	}
	return hLast;
}

// pvkKeys - 0-terminated VKKeys array
bool CConEmuInside::SendVkKeySequence(HWND hWnd, WORD* pvkKeys)
{
	bool bSent = false;
	//DWORD_PTR nRc1, nRc2;
	LRESULT lSendRc = 0;
	DWORD nErrCode = 0;

	if (!pvkKeys || !*pvkKeys)
	{
		_ASSERTE(pvkKeys && *pvkKeys);
		return false;
	}

	// ������ ��� XP
	_ASSERTE(gnOsVer < 0x600);
	HWND hWorker1 = GetDlgItem(hWnd, 0xA005);
	if (!CheckClassName(hWorker1, L"WorkerW"))
		return false;
	HWND hReBar1 = GetDlgItem(hWorker1, 0xA005);
	if (!CheckClassName(hReBar1, L"ReBarWindow32"))
		return false;
	HWND hMenuBar = FindTopWindow(hReBar1, L"ToolbarWindow32");
	if (!hMenuBar)
		return false;

	size_t k = 0;

	HWND hSend = hMenuBar;

	while (pvkKeys[k])
	{
		// Prep send msg values
		UINT nMsg1 = (pvkKeys[k] == VK_F10) ? WM_SYSKEYDOWN : WM_KEYDOWN;
		DEBUGTEST(UINT nMsg2 = (pvkKeys[k] == VK_F10) ? WM_SYSKEYUP : WM_KEYUP);
		UINT vkScan = MapVirtualKey(pvkKeys[k], 0/*MAPVK_VK_TO_VSC*/);
		LPARAM lParam1 = 0x00000001 | (vkScan << 16);
		DEBUGTEST(LPARAM lParam2 = 0xC0000001 | (vkScan << 16));

		// Post KeyDown&KeyUp
		if (pvkKeys[k] == VK_F10)
		{
			PostMessage(hMenuBar, WM_LBUTTONDOWN, 0, MAKELONG(5,5));
			PostMessage(hMenuBar, WM_LBUTTONUP, 0, MAKELONG(5,5));
			//lSendRc = PostMessage(hWnd, nMsg1, pvkKeys[k], lParam1)
			//	&& PostMessage(hWnd, nMsg2, pvkKeys[k], lParam2);
			Sleep(100);
			hSend = hMenuBar;
		}
		else
		{
			// Sequental keys send to "menu" control
			lSendRc = PostMessage(hSend, nMsg1, pvkKeys[k], lParam1);
			if (lSendRc)
			{
				Sleep(100);
				//lSendRc = PostMessage(hSend, nMsg2, pvkKeys[k], lParam2);
				//Sleep(100);
			}
		}

		if (lSendRc)
		{
			bSent = true;
		}
		else
		{
			// failed, may be ERROR_TIMEOUT?
			nErrCode = GetLastError();
			bSent = false;
			break;
		}

		k++;
	}

	// SendMessageTimeout failed?
	_ASSERTE(bSent);

	UNREFERENCED_PARAMETER(nErrCode);

	return bSent;
}

void CConEmuInside::InsideParentMonitor()
{
	// ��� ����� ��������� "����������" - �������� ���� ������/���������
	InsideUpdatePlacement();

	if (mb_InsideSynchronizeCurDir)
	{
		// ��� ����� ����� � ����������
		InsideUpdateDir();
	}
}

void CConEmuInside::InsideUpdateDir()
{
	CVConGuard VCon;
	
	if (mh_InsideParentPath && IsWindow(mh_InsideParentPath) && (gpConEmu->GetActiveVCon(&VCon) >= 0) && VCon->RCon())
	{
		wchar_t szCurText[512] = {};
		DWORD_PTR lRc = 0;
		if (SendMessageTimeout(mh_InsideParentPath, WM_GETTEXT, countof(szCurText), (LPARAM)szCurText, SMTO_ABORTIFHUNG|SMTO_NORMAL, 300, &lRc))
		{
			if (gnOsVer < 0x600)
			{
				// ���� � ��������� ��� ������� ����
				if (wcschr(szCurText, L'\\') == NULL)
				{
					// ����� �������
					return;
				}
			}

			LPCWSTR pszPath = NULL;
			// ���� ��� ��� ���� - �� ������� �� ��������
            if ((szCurText[0] == L'\\' && szCurText[1] == L'\\' && szCurText[2]) // ������� ����
            	|| (szCurText[0] && szCurText[1] == L':' && szCurText[2] == L'\\' /*&& szCurText[3]*/)) // ���� ����� ����� �����
        	{
        		pszPath = szCurText;
        	}
        	else
        	{
        		// ����� - �������� �������. �� ���������� ����� ��� "Address: D:\dir1\dir2"
				pszPath = wcschr(szCurText, L':');
        		if (pszPath)
        			pszPath = SkipNonPrintable(pszPath+1);
        	}

        	// ���� ������� - ���������� � ms_InsideParentPath
        	if (pszPath && *pszPath && (lstrcmpi(ms_InsideParentPath, pszPath) != 0))
        	{
        		int nLen = lstrlen(pszPath);
        		if (nLen >= (int)countof(ms_InsideParentPath))
        		{
        			_ASSERTE((nLen<countof(ms_InsideParentPath)) && "Too long path?");
        		}
        		else //if (VCon->RCon())
        		{
        			// ��������� ��� ���������
        			lstrcpyn(ms_InsideParentPath, pszPath, countof(ms_InsideParentPath));
        			// ����������� ������� ��� ���������� � Shell
        			VCon->RCon()->PostPromptCmd(true, pszPath);
        		}
        	}
		}
	}
}

void CConEmuInside::InsideUpdatePlacement()
{
	if (!mh_InsideParentWND || !IsWindow(mh_InsideParentWND))
		return;

	if ((m_InsideIntegration != ii_Explorer) && (m_InsideIntegration != ii_Simple))
		return;

	if ((m_InsideIntegration == ii_Explorer) && mh_InsideParentRel
		&& (!IsWindow(mh_InsideParentRel) || !IsWindowVisible(mh_InsideParentRel)))
	{
		//Vista: ��������� ��� ����������� ������ �� ������� ������, ��� ����� ����� ��������
		HWND hChild = NULL;
		bool bFound = false;
		while (((hChild = FindWindowEx(mh_InsideParentWND, hChild, NULL, NULL)) != NULL))
		{
			HWND hView = FindWindowEx(hChild, NULL, L"SHELLDLL_DefView", NULL);
			if (hView && IsWindowVisible(hView))
			{
				bFound = true;
				mh_InsideParentRel = hView;
				break;
			}
		}

		if (!bFound)
			return;
	}

	//if ((m_InsideIntegration == ii_Simple) || IsWindow(mh_InsideParentRel))
	{
		RECT rcParent = {}, rcRelative = {};
		GetClientRect(mh_InsideParentWND, &rcParent);
		if (m_InsideIntegration != ii_Simple)
		{
			GetWindowRect(mh_InsideParentRel, &rcRelative);
			MapWindowPoints(NULL, mh_InsideParentWND, (LPPOINT)&rcRelative, 2);
		}

		if (memcmp(&mrc_InsideParent, &rcParent, sizeof(rcParent))
			|| ((m_InsideIntegration != ii_Simple) && memcmp(&mrc_InsideParentRel, &rcRelative, sizeof(rcRelative))))
		{
			// ���������
			RECT rcWnd = {}; // GetDefaultRect();
			if (GetInsideRect(&rcWnd))
			{
				// ��������� � ���������
				gpConEmu->UpdateInsideRect(rcWnd);
			}
		}
	}
}

bool CConEmuInside::GetInsideRect(RECT* prWnd)
{
	RECT rcWnd = {};

	if (m_InsideIntegration == ii_None)
	{
		_ASSERTE(m_InsideIntegration != ii_None);
		return false;
	}

	RECT rcParent = {}, rcRelative = {};
	GetClientRect(mh_InsideParentWND, &rcParent);
	if (m_InsideIntegration == ii_Simple)
	{
		mrc_InsideParent = rcParent;
		ZeroStruct(mrc_InsideParentRel);
		rcWnd = rcParent;
	}
	else
	{
		RECT rcChild = {};
		GetWindowRect(mh_InsideParentRel, &rcChild);
		MapWindowPoints(NULL, mh_InsideParentWND, (LPPOINT)&rcChild, 2);
		mrc_InsideParent = rcParent;
		mrc_InsideParentRel = rcChild;
		IntersectRect(&rcRelative, &rcParent, &rcChild);

		// WinXP & Win2k3
		if (gnOsVer < 0x600)
		{
			rcWnd = rcRelative;
		}
		// Windows 7
		else if ((rcParent.bottom - rcRelative.bottom) >= 100)
		{
			// ���������������
			// ����� - �������� �� OS
			if (gnOsVer < 0x600)
			{
				rcWnd = MakeRect(rcRelative.left, rcRelative.bottom + 4, rcParent.right, rcParent.bottom);
			}
			else
			{
				rcWnd = MakeRect(rcParent.left, rcRelative.bottom + 4, rcParent.right, rcParent.bottom);
			}
		}
		else if ((rcParent.right - rcRelative.right) >= 200)
		{
			rcWnd = MakeRect(rcRelative.right + 4, rcRelative.top, rcParent.right, rcRelative.bottom);
		}
		else
		{
			TODO("������ ������� � �������� �� ����������");
			rcWnd = MakeRect(rcParent.left, rcParent.bottom - 100, rcParent.right, rcParent.bottom);
		}
	}

	if (prWnd)
		*prWnd = rcWnd;

	return true;
}
