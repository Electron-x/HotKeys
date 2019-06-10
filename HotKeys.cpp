// HotKeys.cpp : Defines the entry point for the application.
//

#ifndef STRICT
#define STRICT 1
#endif
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <keybd.h>
#include "resource.h"

#define TRAY_NOTIFYICON WM_USER + 6000
#define ID_TRAY	6000

#ifndef ICON_SMALL
#define ICON_SMALL 0
#endif
#ifndef ICON_BIG
#define ICON_BIG   1
#endif

#define MAX_HOTKEYS 50

typedef struct _HOTKEYLIST
{
	DWORD dwHotKeys[MAX_HOTKEYS];
} HOTKEYLIST, *PHOTKEYLIST;


BOOL CALLBACK HotKeyDlgProc(HWND, UINT, WPARAM, LPARAM);
BOOL CALLBACK AddKeyDlgProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK ButtonDlgProc(HWND, UINT, WPARAM, LPARAM);
BOOL TrayMessage(HWND, DWORD, UINT, HICON);
void LoadHotKeys(HWND, PHOTKEYLIST);
void SaveHotKey(int, DWORD);
BOOL DoneDialog(HWND, PHOTKEYLIST);
void FormatListboxString(UINT uChar, UINT uMod, UINT uVKey, LPTSTR szKeyName);
void GetKeyName(DWORD dwVKey, LPTSTR szVKey);

const TCHAR g_szTitle[]       = TEXT("WR-Tools HotKeys");
const TCHAR g_szAbout[]       = TEXT("HotKeys - Version 1.3\r\nCopyright © 2006-2010\r\nby Wolfgang Rolke\r\nwww.wolfgang-rolke.de");
const TCHAR g_szDlgClass[]    = TEXT("Dialog");
const TCHAR g_szWndClass[]    = TEXT("WRHotKeyDlgClass");
const TCHAR g_szMutex[]       = TEXT("WRToolsHotKeys");
const TCHAR g_szErrorCreate[] = TEXT("Cannot create main window.");
const TCHAR g_szRegPath[]     = TEXT("Software\\WR-Tools\\HotKeys");
const TCHAR g_szModWin[]      = TEXT("Win+");
const TCHAR g_szModAlt[]      = TEXT("Alt+");
const TCHAR g_szModCtrl[]     = TEXT("Ctrl+");
const TCHAR g_szModShift[]    = TEXT("Shift+");
const TCHAR g_szFmtHotKey[]   = TEXT("HotKey%02d");
const TCHAR g_szSendVKey[]    = TEXT("SendVKey");

HINSTANCE g_hInstance = NULL;
BOOL      g_bGerUI = FALSE;
HICON     g_hSmallIcon = NULL;
BOOL      g_bSendVKey = FALSE;
WNDPROC   g_lpPrevButtonDlgProc = NULL;


int WINAPI WinMain(	HINSTANCE hInstance,
					HINSTANCE hPrevInstance,
					LPTSTR    lpCmdLine,
					int       nCmdShow)
{
	HANDLE hMutex = CreateMutex(NULL, FALSE, g_szMutex);
	if (hMutex == NULL || GetLastError() == ERROR_ALREADY_EXISTS)
	{
		HWND hWnd = FindWindow(g_szDlgClass, g_szTitle);
		if (hWnd == NULL)
			hWnd = FindWindow(g_szWndClass, g_szTitle);
		if (hWnd != NULL)
		{
			if (!IsWindowVisible(hWnd)) ShowWindow(hWnd, SW_SHOW);
			SetForegroundWindow(hWnd);
			if (hMutex != NULL) CloseHandle(hMutex);
			return 0;
		}
	}

	g_hInstance = hInstance;
	g_bGerUI = (PRIMARYLANGID(GetUserDefaultLangID()) == LANG_GERMAN);

	INITCOMMONCONTROLSEX icex;
	icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
	icex.dwICC = ICC_UPDOWN_CLASS;
	InitCommonControlsEx(&icex);

	WNDCLASS wndcls;
	if (!GetClassInfo(NULL, g_szDlgClass, &wndcls))
	{
		wndcls.style		= 0;
		wndcls.cbClsExtra	= 0;
		wndcls.cbWndExtra	= DLGWINDOWEXTRA + 4;
	}
	wndcls.lpfnWndProc		= DefDlgProc;
	wndcls.hInstance		= g_hInstance;
	wndcls.hIcon			= LoadIcon(g_hInstance, MAKEINTRESOURCE(IDI_HOTKEY));
#if (_WIN32_WCE > 201)
	wndcls.hCursor			= LoadCursor(NULL, IDC_ARROW);
#else
	wndcls.hCursor			= 0;
#endif
	wndcls.hbrBackground	= (HBRUSH)(COLOR_STATIC + 1);
	wndcls.lpszMenuName		= NULL;
	wndcls.lpszClassName	= g_szWndClass;

	if (!RegisterClass(&wndcls))
	{
		if (hMutex != NULL) CloseHandle(hMutex);
		return 0;
	}

	g_hSmallIcon = (HICON)LoadImage(g_hInstance, MAKEINTRESOURCE(IDI_TRAY), IMAGE_ICON, 16, 16, 0);
	
	HWND hwndDialog = CreateDialog(hInstance,
		g_bGerUI ? MAKEINTRESOURCE(IDD_GER_HOTKEY) : MAKEINTRESOURCE(IDD_ENG_HOTKEY),
		NULL, (DLGPROC)HotKeyDlgProc);
	if (hwndDialog == NULL)
	{
		if (g_hSmallIcon != NULL) DestroyIcon(g_hSmallIcon);
		if (hMutex != NULL) CloseHandle(hMutex);
		MessageBox(NULL, g_szErrorCreate, g_szTitle, MB_OK | MB_ICONSTOP);
		return 0;
	}

	MSG msg;
	HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDR_HOTKEY));
	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (!IsWindow(hwndDialog) ||
			(!TranslateAccelerator(hwndDialog, hAccelTable, &msg) &&
			!IsDialogMessage(hwndDialog, &msg)))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	if (g_hSmallIcon != NULL) DestroyIcon(g_hSmallIcon);
	if (hMutex != NULL) CloseHandle(hMutex);

	return (int)msg.wParam;
}


BOOL CALLBACK HotKeyDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static BOOL bHide = FALSE;
	static HOTKEYLIST HotkeyList = {0};

	switch(msg)
	{
		case WM_HOTKEY:
			{
				if (wParam >= 0 && wParam < MAX_HOTKEYS)
				{
					UINT uCharacter  = (UINT)LOWORD(HotkeyList.dwHotKeys[wParam]);
					UINT fuModifiers = (UINT)LOWORD(lParam);
					UINT uVirtKey    = (UINT)HIWORD(lParam);
					UINT nShiftState = KeyStateDownFlag;
					if (fuModifiers & MOD_SHIFT)   nShiftState |= KeyShiftAnyShiftFlag;
					if (fuModifiers & MOD_CONTROL) nShiftState |= KeyShiftAnyCtrlFlag;
					if (fuModifiers & MOD_ALT)     nShiftState |= KeyShiftAnyAltFlag;
					if (fuModifiers & MOD_WIN)     nShiftState |= KeyShiftLeftWinFlag;
					if (uCharacter == 0)           nShiftState |= KeyShiftNoCharacterFlag;
					PostKeybdMessage((HWND)-1, g_bSendVKey ? uVirtKey : 0,
						nShiftState, 1, &nShiftState, &uCharacter);
				}
			}
			return TRUE;

		case TRAY_NOTIFYICON:
			switch (lParam)
			{
				case WM_LBUTTONDOWN:
					if (wParam == ID_TRAY)
					{
						if (bHide)
						{
							ShowWindow(hDlg, SW_SHOW);
							bHide = FALSE;
						}
						SetForegroundWindow(hDlg);
					}
			}
			break;

		case WM_COMMAND:
			{
				switch (LOWORD(wParam))
				{
					case IDOK:
						ShowWindow(hDlg, SW_HIDE);
						bHide = TRUE;
						return FALSE;

					case IDCANCEL:
						DoneDialog(hDlg, &HotkeyList);
						return FALSE;

					case IDC_ADD:
						{
							DWORD dwHotKey = 0;
							LPARAM lpHotKey = (LPARAM)&dwHotKey;
							int nReturn = IDCANCEL;
							__try
							{
								nReturn = DialogBoxParam(g_hInstance,
									MAKEINTRESOURCE(g_bGerUI ? IDD_GER_ADDKEY : IDD_ENG_ADDKEY),
									hDlg, (DLGPROC)AddKeyDlgProc, lpHotKey);
							}
							__except (EXCEPTION_EXECUTE_HANDLER)
							{
								nReturn = IDCANCEL;
								MessageBox(hDlg, g_szErrorCreate, g_szTitle, MB_OK | MB_ICONSTOP);
							}
							if (nReturn != IDOK)
								return FALSE;

							UINT uChar =  dwHotKey & 0x0000FFFF;
							UINT uVKey = (dwHotKey & 0x00FF0000) >> 16;
							UINT uMod  = (dwHotKey & 0xFF000000) >> 24;

							if (uChar == 0 || uVKey == 0)
							{
								TCHAR szError[256];
								if (LoadString(g_hInstance, g_bGerUI ? IDP_GER_INPUT : IDP_ENG_INPUT,
									szError, sizeof(szError)/sizeof(TCHAR)) > 0)
									MessageBox(hDlg, szError, g_szTitle, MB_OK | MB_ICONEXCLAMATION);
								return FALSE;
							}

							int i;
							for (i = 0; i < MAX_HOTKEYS; i++)
							{
								if (!HotkeyList.dwHotKeys[i])
									break;
							}
							if (i >= MAX_HOTKEYS)
								return FALSE;

							BOOL bSuccess = FALSE;
							__try { bSuccess = RegisterHotKey(hDlg, i, uMod, uVKey); }
							__except (EXCEPTION_EXECUTE_HANDLER) { bSuccess = FALSE; }
							if (!bSuccess)
							{
								TCHAR szError[256];
								if (LoadString(g_hInstance, g_bGerUI ? IDP_GER_ERROR : IDP_ENG_ERROR,
									szError, sizeof(szError)/sizeof(TCHAR)) > 0)
									MessageBox(hDlg, szError, g_szTitle, MB_OK | MB_ICONEXCLAMATION);
								return FALSE;
							}

							TCHAR szKeyName[128];
							FormatListboxString(uChar, uMod, uVKey, szKeyName);
							HWND hwndKeyList = GetDlgItem(hDlg, IDC_LISTBOX);
							int nIndex = ListBox_AddString(hwndKeyList, szKeyName);
							ListBox_SetItemData(hwndKeyList, nIndex, dwHotKey);
							EnableWindow(GetDlgItem(hDlg, IDC_ADD),
								ListBox_GetCount(hwndKeyList) < MAX_HOTKEYS);
							EnableWindow(GetDlgItem(hDlg, IDC_DELETE),
								ListBox_GetCurSel(hwndKeyList) != LB_ERR);
							HotkeyList.dwHotKeys[i] = dwHotKey;
							SaveHotKey(i, dwHotKey);
						}
						return FALSE;

					case IDC_DELETE:
						{
							HWND hwndKeyList = GetDlgItem(hDlg, IDC_LISTBOX);
							int nIndex = ListBox_GetCurSel(hwndKeyList);
							if (nIndex == LB_ERR)
								return FALSE;
							DWORD dwHotKey = ListBox_GetItemData(hwndKeyList, nIndex);
							if (dwHotKey == LB_ERR || dwHotKey == 0)
								return FALSE;

							TCHAR szConfirm[256];
							if (LoadString(g_hInstance, g_bGerUI ? IDP_GER_DELETE : IDP_ENG_DELETE,
								szConfirm, sizeof(szConfirm)/sizeof(TCHAR)) > 0)
							{
								if (MessageBox(hDlg, szConfirm, g_szTitle,
									MB_YESNO | MB_ICONEXCLAMATION) != IDYES)
									return FALSE;
							}

							for (int i = 0; i < MAX_HOTKEYS; i++)
							{
								if (HotkeyList.dwHotKeys[i] == dwHotKey)
								{
									if (UnregisterHotKey(hDlg, i))
									{
										ListBox_DeleteString(hwndKeyList, nIndex);
										EnableWindow(GetDlgItem(hDlg, IDC_ADD),
											ListBox_GetCount(hwndKeyList) < MAX_HOTKEYS);
										EnableWindow(GetDlgItem(hDlg, IDC_DELETE),
											ListBox_GetCurSel(hwndKeyList) != LB_ERR);
										HotkeyList.dwHotKeys[i] = 0;
										SaveHotKey(i, 0);
									}
									break;
								}
							}
						}
						return FALSE;

					case IDC_LISTBOX:
						{
							HWND hwndKeyList = GetDlgItem(hDlg, IDC_LISTBOX);
							int nIndex = ListBox_GetCurSel(hwndKeyList);
							EnableWindow(GetDlgItem(hDlg, IDC_DELETE), (nIndex != LB_ERR));
							if (nIndex == LB_ERR)
								return FALSE;

							if (GET_WM_COMMAND_CMD(wParam, lParam) == LBN_DBLCLK)
							{
								DWORD dwHotKeyOld = ListBox_GetItemData(hwndKeyList, nIndex);
								if (dwHotKeyOld == LB_ERR || dwHotKeyOld == 0)
									return FALSE;
								DWORD dwHotKey = dwHotKeyOld;
								LPARAM lpHotKey = (LPARAM)&dwHotKey;
								int nReturn = IDCANCEL;
								__try
								{
									nReturn = DialogBoxParam(g_hInstance,
										MAKEINTRESOURCE(g_bGerUI ? IDD_GER_ADDKEY : IDD_ENG_ADDKEY),
										hDlg, (DLGPROC)AddKeyDlgProc, lpHotKey);
								}
								__except (EXCEPTION_EXECUTE_HANDLER)
								{
									nReturn = IDCANCEL;
									MessageBox(hDlg, g_szErrorCreate, g_szTitle, MB_OK | MB_ICONSTOP);
								}
								if (nReturn != IDOK)
									return FALSE;
								
								UINT uChar =  dwHotKey & 0x0000FFFF;
								UINT uVKey = (dwHotKey & 0x00FF0000) >> 16;
								UINT uMod  = (dwHotKey & 0xFF000000) >> 24;
								
								if (dwHotKey == dwHotKeyOld)
									return FALSE;
								if (uChar == 0 || uVKey == 0)
								{
									TCHAR szError[256];
									if (LoadString(g_hInstance, g_bGerUI ? IDP_GER_INPUT : IDP_ENG_INPUT,
										szError, sizeof(szError)/sizeof(TCHAR)) > 0)
										MessageBox(hDlg, szError, g_szTitle, MB_OK | MB_ICONEXCLAMATION);
									return FALSE;
								}

								int i;
								for (i = 0; i < MAX_HOTKEYS; i++)
								{
									if (HotkeyList.dwHotKeys[i] == dwHotKeyOld)
										break;
								}
								if (i >= MAX_HOTKEYS)
									return FALSE;

								if (!UnregisterHotKey(hDlg, i))
									return FALSE;
								
								ListBox_DeleteString(hwndKeyList, nIndex);
								EnableWindow(GetDlgItem(hDlg, IDC_ADD),
									ListBox_GetCount(hwndKeyList) < MAX_HOTKEYS);
								EnableWindow(GetDlgItem(hDlg, IDC_DELETE),
									ListBox_GetCurSel(hwndKeyList) != LB_ERR);
								HotkeyList.dwHotKeys[i] = 0;

								BOOL bSuccess = FALSE;
								__try { bSuccess = RegisterHotKey(hDlg, i, uMod, uVKey); }
								__except (EXCEPTION_EXECUTE_HANDLER) { bSuccess = FALSE; }
								if (bSuccess)
								{
									TCHAR szKeyName[128];
									FormatListboxString(uChar, uMod, uVKey, szKeyName);
									nIndex = ListBox_AddString(hwndKeyList, szKeyName);
									ListBox_SetItemData(hwndKeyList, nIndex, dwHotKey);
									EnableWindow(GetDlgItem(hDlg, IDC_ADD),
										ListBox_GetCount(hwndKeyList) < MAX_HOTKEYS);
									EnableWindow(GetDlgItem(hDlg, IDC_DELETE),
										ListBox_GetCurSel(hwndKeyList) != LB_ERR);
									HotkeyList.dwHotKeys[i] = dwHotKey;
								}
								else
								{
									TCHAR szError[256];
									if (LoadString(g_hInstance, g_bGerUI ? IDP_GER_ERROR : IDP_ENG_ERROR,
										szError, sizeof(szError)/sizeof(TCHAR)) > 0)
										MessageBox(hDlg, szError, g_szTitle, MB_OK | MB_ICONEXCLAMATION);
								}

								SaveHotKey(i, HotkeyList.dwHotKeys[i]);
							}
						}
						return FALSE;
				}
			}
			break;

		case WM_HELP:
			{
				static BOOL bAboutBox = FALSE;
				if (!bAboutBox)
				{
					bAboutBox = TRUE;
					MessageBox(hDlg, g_szAbout, g_szTitle, MB_OK | MB_ICONINFORMATION);
					bAboutBox = FALSE;
				}
			}
			return TRUE;

		case WM_INITDIALOG:
			{
				int nTabStop = 16;
				bHide = TRUE;

				SetWindowText(hDlg, g_szTitle);

				HWND hwndKeyList = GetDlgItem(hDlg, IDC_LISTBOX);
				ListBox_SetTabStops(hwndKeyList, 1, &nTabStop);

				LoadHotKeys(hDlg, &HotkeyList);

				int nCount = ListBox_GetCount(hwndKeyList);
				if (nCount == LB_ERR) nCount = 0;

				EnableWindow(GetDlgItem(hDlg, IDC_ADD), nCount < MAX_HOTKEYS);
				EnableWindow(GetDlgItem(hDlg, IDC_DELETE),
					ListBox_GetCurSel(hwndKeyList) != LB_ERR);

				if (g_hSmallIcon != NULL)
					PostMessage(hDlg, WM_SETICON, (WPARAM)(BOOL)ICON_SMALL, (LPARAM)g_hSmallIcon);
				PostMessage(hDlg, WM_SETICON, (WPARAM)(BOOL)ICON_BIG,
					(LPARAM)LoadIcon(g_hInstance, MAKEINTRESOURCE(IDI_HOTKEY)));

				if (!TrayMessage(hDlg, NIM_ADD, ID_TRAY, g_hSmallIcon) || (nCount == 0))
				{
					ShowWindow(hDlg, SW_SHOWNORMAL);
					bHide = FALSE;
				}
			}
			return TRUE;

		case WM_CLOSE:
			DoneDialog(hDlg, &HotkeyList);
			break;

		case WM_DESTROY:
			PostQuitMessage(0);
			break;
	}

	return FALSE;
}


BOOL CALLBACK AddKeyDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static LPDWORD pdwHotKey = NULL;
	static BOOL bSendVKeyOld = FALSE;

	switch(msg)
	{
		case WM_INITDIALOG:
			{
				TCHAR szKey[32];
				TCHAR szHexChar[32];
				DWORD dwHotKey = 0;
				bSendVKeyOld = g_bSendVKey;
				g_bSendVKey = TRUE;
				pdwHotKey = (LPDWORD)lParam;
				if (pdwHotKey != NULL)
					dwHotKey = *pdwHotKey;

				SendDlgItemMessage(hDlg, IDC_UNICODE_SPIN, UDM_SETBASE, 16, 0);
				SendDlgItemMessage(hDlg, IDC_UNICODE_SPIN, UDM_SETRANGE, 0,
					(LPARAM)MAKELONG((short)UD_MAXVAL, (short)0));
				SendDlgItemMessage(hDlg, IDC_UNICODE_SPIN, UDM_SETRANGE32, 0, 65535);
				SendDlgItemMessage(hDlg, IDC_UNICODE_SPIN, UDM_SETPOS, 0,
					(LPARAM)MAKELONG(LOWORD(dwHotKey), 0));
				
				wsprintf(szHexChar, TEXT("0x%04X"), LOWORD(dwHotKey));
				SetDlgItemText(hDlg, IDC_UNICODE_CHAR, szHexChar);
				
				szKey[0] = LOWORD(dwHotKey);
				szKey[1] = TEXT('\0');
				SetDlgItemText(hDlg, IDC_CHARACTER, szKey);
				
				BYTE cVirtKey = LOBYTE(HIWORD(dwHotKey));
				GetKeyName(cVirtKey, szKey);
				SetDlgItemText(hDlg, IDC_KEY, szKey);
				
				wsprintf(szHexChar, TEXT("0x%02X"), (WORD)cVirtKey);
				SetDlgItemText(hDlg, IDC_UNICODE_KEY, szHexChar);
				
				BYTE fModKeys = HIBYTE(HIWORD(dwHotKey));
				SendDlgItemMessage(hDlg, IDC_SHIFT, BM_SETCHECK,
					(fModKeys & MOD_SHIFT) ? BST_CHECKED : BST_UNCHECKED, 0);
				SendDlgItemMessage(hDlg, IDC_CTRL, BM_SETCHECK,
					(fModKeys & MOD_CONTROL) ? BST_CHECKED : BST_UNCHECKED, 0);
				SendDlgItemMessage(hDlg, IDC_ALT, BM_SETCHECK,
					(fModKeys & MOD_ALT) ? BST_CHECKED : BST_UNCHECKED, 0);
				SendDlgItemMessage(hDlg, IDC_WIN, BM_SETCHECK,
					(fModKeys & MOD_WIN) ? BST_CHECKED : BST_UNCHECKED, 0);
				
				SendDlgItemMessage(hDlg, IDC_UNICODE_CHAR, EM_LIMITTEXT, 6, 0);
				
				SendDlgItemMessage(hDlg, IDC_CHARACTER, CB_LIMITTEXT, 1, 0);
				SendDlgItemMessage(hDlg, IDC_CHARACTER, CB_ADDSTRING, 0, (LPARAM)TEXT("\x2013"));
				SendDlgItemMessage(hDlg, IDC_CHARACTER, CB_ADDSTRING, 0, (LPARAM)TEXT("\x2014"));
				SendDlgItemMessage(hDlg, IDC_CHARACTER, CB_ADDSTRING, 0,
					g_bGerUI ? (LPARAM)TEXT("\x201A") : (LPARAM)TEXT("\x2018"));
				SendDlgItemMessage(hDlg, IDC_CHARACTER, CB_ADDSTRING, 0, (LPARAM)TEXT("\x2019"));
				SendDlgItemMessage(hDlg, IDC_CHARACTER, CB_ADDSTRING, 0,
					g_bGerUI ? (LPARAM)TEXT("\x201E") : (LPARAM)TEXT("\x201C"));
				SendDlgItemMessage(hDlg, IDC_CHARACTER, CB_ADDSTRING, 0, (LPARAM)TEXT("\x201D"));
				if (g_bGerUI)
				{
					SendDlgItemMessage(hDlg, IDC_CHARACTER, CB_ADDSTRING, 0, (LPARAM)TEXT("\x00BB"));
					SendDlgItemMessage(hDlg, IDC_CHARACTER, CB_ADDSTRING, 0, (LPARAM)TEXT("\x00AB"));
				}
				SendDlgItemMessage(hDlg, IDC_CHARACTER, CB_ADDSTRING, 0, (LPARAM)TEXT("\x2026"));
				SendDlgItemMessage(hDlg, IDC_CHARACTER, CB_ADDSTRING, 0, (LPARAM)TEXT("\x2022"));
				if (g_bGerUI)
				{
					SendDlgItemMessage(hDlg, IDC_CHARACTER, CB_ADDSTRING, 0, (LPARAM)TEXT("\x00E4"));
					SendDlgItemMessage(hDlg, IDC_CHARACTER, CB_ADDSTRING, 0, (LPARAM)TEXT("\x00C4"));
					SendDlgItemMessage(hDlg, IDC_CHARACTER, CB_ADDSTRING, 0, (LPARAM)TEXT("\x00F6"));
					SendDlgItemMessage(hDlg, IDC_CHARACTER, CB_ADDSTRING, 0, (LPARAM)TEXT("\x00D6"));
					SendDlgItemMessage(hDlg, IDC_CHARACTER, CB_ADDSTRING, 0, (LPARAM)TEXT("\x00FC"));
					SendDlgItemMessage(hDlg, IDC_CHARACTER, CB_ADDSTRING, 0, (LPARAM)TEXT("\x00DC"));
					SendDlgItemMessage(hDlg, IDC_CHARACTER, CB_ADDSTRING, 0, (LPARAM)TEXT("\x00DF"));
				}
				else
				{
					SendDlgItemMessage(hDlg, IDC_CHARACTER, CB_ADDSTRING, 0, (LPARAM)TEXT("\x2264"));
					SendDlgItemMessage(hDlg, IDC_CHARACTER, CB_ADDSTRING, 0, (LPARAM)TEXT("\x2265"));
					SendDlgItemMessage(hDlg, IDC_CHARACTER, CB_ADDSTRING, 0, (LPARAM)TEXT("\x00F7"));
					SendDlgItemMessage(hDlg, IDC_CHARACTER, CB_ADDSTRING, 0, (LPARAM)TEXT("\x00D7"));
					SendDlgItemMessage(hDlg, IDC_CHARACTER, CB_ADDSTRING, 0, (LPARAM)TEXT("\x00A9"));
				}
				SendDlgItemMessage(hDlg, IDC_CHARACTER, CB_ADDSTRING, 0, (LPARAM)TEXT("\x20AC"));
				
				g_lpPrevButtonDlgProc = (WNDPROC)SetWindowLong(GetDlgItem(hDlg, IDC_VIRTKEY),
					GWL_WNDPROC, (LONG)ButtonDlgProc);
			}
			return TRUE;

		case WM_COMMAND:
			{
				static BOOL bUpdateBuddy = TRUE;

				switch (LOWORD(wParam))
				{
					case IDC_CHARACTER:
						if (HIWORD(wParam) == CBN_EDITCHANGE)
						{
							if (bUpdateBuddy)
							{
								TCHAR szChar[32];
								bUpdateBuddy = FALSE;
								SetDlgItemText(hDlg, IDC_UNICODE_CHAR, TEXT("0x0000"));
								if (GetWindowText((HWND)lParam, szChar, (sizeof(szChar)/sizeof(TCHAR))+1))
								{
									TCHAR szHexChar[32];
									wsprintf(szHexChar, TEXT("0x%04X"), (WORD)szChar[0]);
									SetDlgItemText(hDlg, IDC_UNICODE_CHAR, szHexChar);
								}
								bUpdateBuddy = TRUE;
							}
						}
						else if (HIWORD(wParam) == CBN_SELCHANGE)
						{
							if (bUpdateBuddy)
							{
								TCHAR szChar[32];
								bUpdateBuddy = FALSE;
								SetDlgItemText(hDlg, IDC_UNICODE_CHAR, TEXT("0x0000"));
								int nIndex = ComboBox_GetCurSel((HWND)lParam);
								if (nIndex != CB_ERR && ComboBox_GetLBText((HWND)lParam, nIndex, szChar) > 0)
								{
									TCHAR szHexChar[32];
									wsprintf(szHexChar, TEXT("0x%04X"), (WORD)szChar[0]);
									SetDlgItemText(hDlg, IDC_UNICODE_CHAR, szHexChar);
								}
								bUpdateBuddy = TRUE;
							}
						}
						return FALSE;

					case IDC_UNICODE_CHAR:
						if (HIWORD(wParam) == EN_CHANGE)
						{
							if (bUpdateBuddy)
							{
								TCHAR szChar[32];
								bUpdateBuddy = FALSE;
								SetDlgItemText(hDlg, IDC_CHARACTER, TEXT(""));
								if (GetWindowText((HWND)lParam, szChar, (sizeof(szChar)/sizeof(TCHAR))+1))
								{
									LPTSTR lpsz;
									UINT uChar = _tcstoul(szChar, &lpsz, 16);
									if (uChar > 0xFFFF)
										uChar = 0;
									szChar[0] = (TCHAR)uChar;
									szChar[1] = TEXT('\0');
									SetDlgItemText(hDlg, IDC_CHARACTER, szChar);
								}
								bUpdateBuddy = TRUE;
							}
						}
						return FALSE;

					case IDCANCEL:
						g_bSendVKey = bSendVKeyOld;
						EndDialog(hDlg, IDCANCEL);
						return FALSE;

					case IDOK:
						{
							LPTSTR lpsz;
							TCHAR szChar[32];
							UINT nUnicodeChar = 0;
							if (GetDlgItemText(hDlg, IDC_UNICODE_CHAR, szChar, sizeof(szChar)/sizeof(TCHAR)))
								nUnicodeChar = _tcstoul(szChar, &lpsz, 16);
							if (nUnicodeChar > 0xFFFF)
								nUnicodeChar = 0;
							UINT nVirtualKey = 0;
							if (GetDlgItemText(hDlg, IDC_UNICODE_KEY, szChar, sizeof(szChar)/sizeof(TCHAR)))
								nVirtualKey = _tcstoul(szChar, &lpsz, 16);
							UINT nModKeys = 0;
							nModKeys |= SendMessage(GetDlgItem(hDlg, IDC_SHIFT), BM_GETCHECK, 0, 0) ? MOD_SHIFT : 0;
							nModKeys |= SendMessage(GetDlgItem(hDlg, IDC_CTRL), BM_GETCHECK, 0, 0) ? MOD_CONTROL : 0;
							nModKeys |= SendMessage(GetDlgItem(hDlg, IDC_ALT), BM_GETCHECK, 0, 0) ? MOD_ALT : 0;
							nModKeys |= SendMessage(GetDlgItem(hDlg, IDC_WIN), BM_GETCHECK, 0, 0) ? MOD_WIN : 0;
							if (pdwHotKey != NULL)
								*pdwHotKey = nUnicodeChar | (nVirtualKey << 16) | (nModKeys << 24);
							g_bSendVKey = bSendVKeyOld;
							EndDialog(hDlg, IDOK);
						}
						return FALSE;
				}
			}
			break;

		case WM_HELP:
			{
				static BOOL bHelpBox = FALSE;
				if (!bHelpBox)
				{
					bHelpBox = TRUE;
					TCHAR szHelp[256];
					if (LoadString(g_hInstance, g_bGerUI ? IDP_GER_HELP : IDP_ENG_HELP,
						szHelp, sizeof(szHelp)/sizeof(TCHAR)) > 0)
						MessageBox(hDlg, szHelp, g_szTitle, MB_OK | MB_ICONINFORMATION);
					bHelpBox = FALSE;
				}
			}
			return TRUE;

		case WM_CLOSE:
			g_bSendVKey = bSendVKeyOld;
			EndDialog(hDlg, 0);
			break;
	}

	return FALSE;
}


LRESULT CALLBACK ButtonDlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	LRESULT lRet = CallWindowProc(g_lpPrevButtonDlgProc, hwnd, uMsg, wParam, lParam);

	if (uMsg == WM_GETDLGCODE && lParam != NULL)
	{
		LPMSG lpMsg = (LPMSG)lParam;
		if (lpMsg->message == WM_KEYDOWN ||
			lpMsg->message == WM_SYSKEYDOWN)
		{
			if (lpMsg->wParam == VK_RETURN)
				PostMessage(GetParent(hwnd), WM_COMMAND, 1, 0);
			else if (lpMsg->wParam != VK_ESCAPE && lpMsg->wParam != VK_TAB)
			{
				HWND hwndParent = GetParent(hwnd);
				TCHAR szKey[32];
				GetKeyName(lpMsg->wParam, szKey);
				SetDlgItemText(hwndParent, IDC_KEY, szKey);
				wsprintf(szKey, TEXT("0x%02X"), lpMsg->wParam);
				SetDlgItemText(hwndParent, IDC_UNICODE_KEY, szKey);
				SendDlgItemMessage(hwndParent, IDC_SHIFT, BM_SETCHECK,
					(GetKeyState(VK_SHIFT) >> 15) ? BST_CHECKED : BST_UNCHECKED, 0);
				SendDlgItemMessage(hwndParent, IDC_CTRL, BM_SETCHECK,
					(GetKeyState(VK_CONTROL) >> 15) ? BST_CHECKED : BST_UNCHECKED, 0);
				SendDlgItemMessage(hwndParent, IDC_ALT, BM_SETCHECK,
					(GetKeyState(VK_MENU) >> 15) ? BST_CHECKED : BST_UNCHECKED, 0);
			}
		}
	}

	return lRet;
}


BOOL TrayMessage(HWND hwnd, DWORD dwMessage, UINT uID, HICON hIcon)
{
	NOTIFYICONDATA tnd;

	tnd.cbSize = sizeof(NOTIFYICONDATA);
	tnd.hWnd = hwnd;
	tnd.uID = uID;
	tnd.uFlags = NIF_MESSAGE;
	if (hIcon != NULL)
		tnd.uFlags |= NIF_ICON;
	tnd.uCallbackMessage = TRAY_NOTIFYICON;
	tnd.hIcon = hIcon;
	tnd.szTip[0] = TEXT('\0');

	return Shell_NotifyIcon(dwMessage, &tnd);
}


BOOL DoneDialog(HWND hDlg, PHOTKEYLIST pHotkeyList)
{
	TrayMessage(hDlg, NIM_DELETE, ID_TRAY, NULL);

	if (pHotkeyList != NULL)
	{
		for (int i = 0; i < MAX_HOTKEYS; i++)
		{
			if (pHotkeyList->dwHotKeys[i] && UnregisterHotKey(hDlg, i))
				pHotkeyList->dwHotKeys[i] = 0;
		}
	}

	return DestroyWindow(hDlg);
}


void LoadHotKeys(HWND hDlg, PHOTKEYLIST pHotkeyList)
{
	if (pHotkeyList == NULL)
		return;

	HKEY hKey;
	if (RegOpenKeyEx(HKEY_CURRENT_USER, g_szRegPath, 0, 0, &hKey) != ERROR_SUCCESS)
		return;

	DWORD dwType = REG_DWORD;
	DWORD dwSize = sizeof(DWORD);
	DWORD dwHotKey = 0;
	UINT  uChar;
	UINT  uVKey;
	UINT  uMod;
	BOOL  bSuccess;
	TCHAR szIndex[32];

	if (RegQueryValueEx(hKey, g_szSendVKey, NULL, &dwType,
		(LPBYTE)&dwHotKey, &dwSize) == ERROR_SUCCESS)
		g_bSendVKey = (BOOL)dwHotKey;

	for (int i = 0; i < MAX_HOTKEYS; i++)
	{
		dwHotKey = 0;
		wsprintf(szIndex, g_szFmtHotKey, (int)(i+1));
		if (RegQueryValueEx(hKey, szIndex, NULL, &dwType,
			(LPBYTE)&dwHotKey, &dwSize) != ERROR_SUCCESS)
			continue;

		if (dwHotKey == 0)
			continue;

		uChar =  dwHotKey & 0x0000FFFF;
		uVKey = (dwHotKey & 0x00FF0000) >> 16;
		uMod  = (dwHotKey & 0xFF000000) >> 24;
		if (uChar == 0 || uVKey == 0)
			continue;

		bSuccess = FALSE;
		__try { bSuccess = RegisterHotKey(hDlg, i, uMod, uVKey); }
		__except (EXCEPTION_EXECUTE_HANDLER) { bSuccess = FALSE; }
		if (bSuccess)
		{
			TCHAR szKeyName[128];
			FormatListboxString(uChar, uMod, uVKey, szKeyName);
			HWND hwndKeyList = GetDlgItem(hDlg, IDC_LISTBOX);
			int nIndex = ListBox_AddString(hwndKeyList, szKeyName);
			ListBox_SetItemData(hwndKeyList, nIndex, dwHotKey);
			pHotkeyList->dwHotKeys[i] = dwHotKey;
		}
	}

	RegCloseKey(hKey);
}


void SaveHotKey(int nIndex, DWORD dwHotKey)
{
	HKEY hKey;
	DWORD Disp;

	if (RegCreateKeyEx(HKEY_CURRENT_USER, g_szRegPath,
		0, NULL, 0, 0, NULL, &hKey, &Disp) != ERROR_SUCCESS)
		return;

	TCHAR szIndex[32];
	wsprintf(szIndex, g_szFmtHotKey, (int)(nIndex+1));
	RegSetValueEx(hKey, szIndex, 0, REG_DWORD, (LPBYTE)&dwHotKey, sizeof(DWORD));

	RegCloseKey(hKey);
}


void FormatListboxString(UINT uChar, UINT uMod, UINT uVKey, LPTSTR szKeyName)
{
	if (szKeyName == NULL)
		return;

	TCHAR szChar[32];
	szChar[0] = (TCHAR)uChar;
	szChar[1] = TEXT('\0');
	wsprintf(szKeyName, TEXT(" %s \t"), szChar);

	if (uMod & MOD_WIN)
		_tcscat(szKeyName, g_szModWin);
	if (uMod & MOD_ALT)
		_tcscat(szKeyName, g_szModAlt);
	if (uMod & MOD_CONTROL)
		_tcscat(szKeyName, g_szModCtrl);
	if (uMod & MOD_SHIFT)
		_tcscat(szKeyName, g_szModShift);

	TCHAR szKey[32];
	GetKeyName(uVKey, szKey);
	_tcscat(szKeyName, szKey);
}


void GetKeyName(DWORD dwVKey, LPTSTR szVKey)
{
	if (szVKey == NULL)
		return;

	if ((dwVKey >= 0x30 && dwVKey <= 0x39) ||
		(dwVKey >= 0x41 && dwVKey <= 0x5A))
	{
		szVKey[0] = (TCHAR)dwVKey;
		szVKey[1] = TEXT('\0');
		return;
	}

	switch (dwVKey)
	{
		case 0:
			_tcscpy(szVKey, TEXT(""));
			break;
		case VK_ESCAPE:
			_tcscpy(szVKey, TEXT("ESC"));
			break;
		case VK_RETURN:
			_tcscpy(szVKey, TEXT("ENTER"));
			break;
		case VK_TAB:
			_tcscpy(szVKey, TEXT("TAB"));
			break;
		case VK_CAPITAL:
			_tcscpy(szVKey, TEXT("CAPS LOCK"));
			break;
		case VK_SHIFT:
			_tcscpy(szVKey, TEXT("SHIFT"));
			break;
		case VK_RSHIFT:
			_tcscpy(szVKey, TEXT("RIGHT SHIFT"));
			break;
		case VK_LSHIFT:
			_tcscpy(szVKey, TEXT("LEFT SHIFT"));
			break;
		case VK_CONTROL:
			_tcscpy(szVKey, TEXT("CTRL"));
			break;
		case VK_RCONTROL:
			_tcscpy(szVKey, TEXT("RIGHT CTRL"));
			break;
		case VK_LCONTROL:
			_tcscpy(szVKey, TEXT("LEFT CTRL"));
			break;
		case VK_MENU:
			_tcscpy(szVKey, TEXT("ALT"));
			break;
		case VK_RMENU:
			_tcscpy(szVKey, TEXT("RIGHT ALT"));
			break;
		case VK_LMENU:
			_tcscpy(szVKey, TEXT("LEFT ALT"));
			break;
		case VK_RWIN:
			_tcscpy(szVKey, TEXT("RIGHT WINDOWS"));
			break;
		case VK_LWIN:
			_tcscpy(szVKey, TEXT("LEFT WINDOWS"));
			break;
		case VK_APPS:
			_tcscpy(szVKey, TEXT("APPLICATIONS"));
			break;
		case VK_BACKSLASH:
			_tcscpy(szVKey, TEXT("BACKSLASH"));
			break;
		case VK_LBRACKET:
			_tcscpy(szVKey, TEXT("LEFT BRACKET"));
			break;
		case VK_RBRACKET:
			_tcscpy(szVKey, TEXT("RIGHT BRACKET"));
			break;
		case VK_BACK:
			_tcscpy(szVKey, TEXT("BACKSPACE"));
			break;
		case VK_SEMICOLON:
			_tcscpy(szVKey, TEXT("SEMICOLON"));
			break;
		case VK_EQUAL:
			_tcscpy(szVKey, TEXT("EQUAL"));
			break;
		case VK_BACKQUOTE:
			_tcscpy(szVKey, TEXT("BACKQUOTE"));
			break;
		case VK_APOSTROPHE:
			_tcscpy(szVKey, TEXT("APOSTROPHE"));
			break;
		case VK_SLASH:
			_tcscpy(szVKey, TEXT("SLASH"));
			break;
		case VK_OEM_102:
			_tcscpy(szVKey, TEXT("EXTEND-BSLASH"));
			break;
		case VK_COMMA:
			_tcscpy(szVKey, TEXT("COMMA"));
			break;
		case VK_PERIOD:
			_tcscpy(szVKey, TEXT("PERIOD"));
			break;
		case VK_HYPHEN:
			_tcscpy(szVKey, TEXT("HYPHEN"));
			break;
		case VK_SPACE:
			_tcscpy(szVKey, TEXT("SPACE"));
			break;
		case VK_RIGHT:
			_tcscpy(szVKey, TEXT("RIGHT"));
			break;
		case VK_LEFT:
			_tcscpy(szVKey, TEXT("LEFT"));
			break;
		case VK_UP:
			_tcscpy(szVKey, TEXT("UP"));
			break;
		case VK_DOWN:
			_tcscpy(szVKey, TEXT("DOWN"));
			break;
		case VK_INSERT:
			_tcscpy(szVKey, TEXT("INSERT"));
			break;
		case VK_DELETE:
			_tcscpy(szVKey, TEXT("DELETE"));
			break;
		case VK_HOME:
			_tcscpy(szVKey, TEXT("HOME"));
			break;
		case VK_END:
			_tcscpy(szVKey, TEXT("END"));
			break;
		case VK_PRIOR:
			_tcscpy(szVKey, TEXT("PAGE UP"));
			break;
		case VK_NEXT:
			_tcscpy(szVKey, TEXT("PAGE DOWN"));
			break;
		case VK_SNAPSHOT:
			_tcscpy(szVKey, TEXT("PRINT SCREEN"));
			break;
		case VK_SCROLL:
			_tcscpy(szVKey, TEXT("SCROLL LOCK"));
			break;
		case VK_PAUSE:
			_tcscpy(szVKey, TEXT("PAUSE"));
			break;
		case VK_NUMLOCK:
			_tcscpy(szVKey, TEXT("NUM LOCK"));
			break;
		case VK_NUMPAD0:
			_tcscpy(szVKey, TEXT("NUM 0"));
			break;
		case VK_NUMPAD1:
			_tcscpy(szVKey, TEXT("NUM 1"));
			break;
		case VK_NUMPAD2:
			_tcscpy(szVKey, TEXT("NUM 2"));
			break;
		case VK_NUMPAD3:
			_tcscpy(szVKey, TEXT("NUM 3"));
			break;
		case VK_NUMPAD4:
			_tcscpy(szVKey, TEXT("NUM 4"));
			break;
		case VK_NUMPAD5:
			_tcscpy(szVKey, TEXT("NUM 5"));
			break;
		case VK_NUMPAD6:
			_tcscpy(szVKey, TEXT("NUM 6"));
			break;
		case VK_NUMPAD7:
			_tcscpy(szVKey, TEXT("NUM 7"));
			break;
		case VK_NUMPAD8:
			_tcscpy(szVKey, TEXT("NUM 8"));
			break;
		case VK_NUMPAD9:
			_tcscpy(szVKey, TEXT("NUM 9"));
			break;
		case VK_CLEAR:
			_tcscpy(szVKey, TEXT("CLEAR"));
			break;
		case VK_DIVIDE:
			_tcscpy(szVKey, TEXT("DIVIDE"));
			break;
		case VK_MULTIPLY:
			_tcscpy(szVKey, TEXT("MULTIPLY"));
			break;
		case VK_SUBTRACT:
			_tcscpy(szVKey, TEXT("SUBTRACT"));
			break;
		case VK_ADD:
			_tcscpy(szVKey, TEXT("ADD"));
			break;
		case VK_DECIMAL:
			_tcscpy(szVKey, TEXT("DECIMAL"));
			break;
		case VK_F1:
			_tcscpy(szVKey, TEXT("F1/SOFT1"));
			break;
		case VK_F2:
			_tcscpy(szVKey, TEXT("F2/SOFT2"));
			break;
		case VK_F3:
			_tcscpy(szVKey, TEXT("F3/TALK"));
			break;
		case VK_F4:
			_tcscpy(szVKey, TEXT("F4/END"));
			break;
		case VK_F5:
			_tcscpy(szVKey, TEXT("F5"));
			break;
		case VK_F6:
			_tcscpy(szVKey, TEXT("F6/VOLUMEUP/DONE"));
			break;
		case VK_F7:
			_tcscpy(szVKey, TEXT("F7/VOLUMEDOWN/MOJI"));
			break;
		case VK_F8:
			_tcscpy(szVKey, TEXT("F8/STAR"));
			break;
		case VK_F9:
			_tcscpy(szVKey, TEXT("F9/POUND"));
			break;
		case VK_F10:
			_tcscpy(szVKey, TEXT("F10/RECORD"));
			break;
		case VK_F11:
			_tcscpy(szVKey, TEXT("F11/SYMBOL"));
			break;
		case VK_F12:
			_tcscpy(szVKey, TEXT("F12"));
			break;
		case VK_F13:
			_tcscpy(szVKey, TEXT("F13"));
			break;
		case VK_F14:
			_tcscpy(szVKey, TEXT("F14"));
			break;
		case VK_F15:
			_tcscpy(szVKey, TEXT("F15/ENDDATACALLS"));
			break;
		case VK_F16:
			_tcscpy(szVKey, TEXT("F16/SPEAKERTOGGLE"));
			break;
		case VK_F17:
			_tcscpy(szVKey, TEXT("F17/FLIP"));
			break;
		case VK_F18:
			_tcscpy(szVKey, TEXT("F18/POWER"));
			break;
		case VK_F19:
			_tcscpy(szVKey, TEXT("F19/REDKEY"));
			break;
		case VK_F20:
			_tcscpy(szVKey, TEXT("F20/ROCKER"));
			break;
		case VK_F21:
			_tcscpy(szVKey, TEXT("F21/DPAD"));
			break;
		case VK_F22:
			_tcscpy(szVKey, TEXT("F22/KEYLOCK"));
			break;
		case VK_F23:
			_tcscpy(szVKey, TEXT("F23/ACTION"));
			break;
		case VK_F24:
			_tcscpy(szVKey, TEXT("F24/VOICEDIAL"));
			break;
		case 0xc1:
			_tcscpy(szVKey, TEXT("APP 1"));
			break;
		case 0xc2:
			_tcscpy(szVKey, TEXT("APP 2"));
			break;
		case 0xc3:
			_tcscpy(szVKey, TEXT("APP 3"));
			break;
		case 0xc4:
			_tcscpy(szVKey, TEXT("APP 4"));
			break;
		case 0xc5:
			_tcscpy(szVKey, TEXT("APP 5"));
			break;
		case 0xc6:
			_tcscpy(szVKey, TEXT("APP 6"));
			break;
		case VK_SEPARATOR:
			_tcscpy(szVKey, TEXT("SEPARATOR"));
			break;
		case VK_PRINT:
			_tcscpy(szVKey, TEXT("PRINT"));
			break;
		case VK_EXECUTE:
			_tcscpy(szVKey, TEXT("EXECUTE"));
			break;
		case VK_CANCEL:
			_tcscpy(szVKey, TEXT("BREAK"));
			break;
		case VK_HELP:
			_tcscpy(szVKey, TEXT("HELP"));
			break;
		case VK_OEM_CLEAR:
			_tcscpy(szVKey, TEXT("OEM-CLEAR"));
			break;
		case VK_OFF:
			_tcscpy(szVKey, TEXT("OFF"));
			break;
		case 0x5f:
			_tcscpy(szVKey, TEXT("SLEEP"));
			break;
		case VK_SELECT:
			_tcscpy(szVKey, TEXT("SELECT"));
			break;
		case VK_ATTN:
			_tcscpy(szVKey, TEXT("ATTN/NOROMAN"));
			break;
		case VK_CRSEL:
			_tcscpy(szVKey, TEXT("CRSEL/WORDREGISTER"));
			break;
		case VK_EREOF:
			_tcscpy(szVKey, TEXT("EREOF/FLUSHSTRING"));
			break;
		case VK_EXSEL:
			_tcscpy(szVKey, TEXT("EXSEL/IMECONFIG"));
			break;
		case VK_NONAME:
			_tcscpy(szVKey, TEXT("NONAME/DETERMSTR"));
			break;
		case VK_PA1:
			_tcscpy(szVKey, TEXT("PA1/DLGCONVERSION"));
			break;
		case VK_PLAY:
			_tcscpy(szVKey, TEXT("PLAY/CODEINPUT"));
			break;
		case VK_ZOOM:
			_tcscpy(szVKey, TEXT("ZOOM/NOCODEINPUT"));
			break;
		case 0x15:
			_tcscpy(szVKey, TEXT("KANA/HANGUL"));
			break;
		case 0x17:
			_tcscpy(szVKey, TEXT("JUNJA"));
			break;
		case 0x18:
			_tcscpy(szVKey, TEXT("FINAL"));
			break;
		case 0x19:
			_tcscpy(szVKey, TEXT("KANJI/HANJA"));
			break;
		case 0x1c:
			_tcscpy(szVKey, TEXT("CONVERT"));
			break;
		case 0x1d:
			_tcscpy(szVKey, TEXT("NONCONVERT"));
			break;
		case 0xf0:
			_tcscpy(szVKey, TEXT("ALPHANUMERIC"));
			break;
		case 0xf1:
			_tcscpy(szVKey, TEXT("KATAKANA"));
			break;
		case 0xf2:
			_tcscpy(szVKey, TEXT("HIRAGANA"));
			break;
		case 0xf3:
			_tcscpy(szVKey, TEXT("SBCSCHAR"));
			break;
		case 0xf4:
			_tcscpy(szVKey, TEXT("DBCSCHAR"));
			break;
		case 0xf5:
			_tcscpy(szVKey, TEXT("ROMAN"));
			break;
		case 0xE5:
			_tcscpy(szVKey, TEXT("PROCESS KEY"));
			break;
		case VK_LBUTTON:
			_tcscpy(szVKey, TEXT("LEFT BUTTON"));
			break;
		case VK_RBUTTON:
			_tcscpy(szVKey, TEXT("RIGHT BUTTON"));
			break;
		case VK_MBUTTON:
			_tcscpy(szVKey, TEXT("MIDDLE BUTTON"));
			break;
		case 0x05:
			_tcscpy(szVKey, TEXT("X1 BUTTON"));
			break;
		case 0x06:
			_tcscpy(szVKey, TEXT("X2 BUTTON"));
			break;
		default:
			wsprintf(szVKey, TEXT("0x%02X"), dwVKey);
			break;
	}
}
