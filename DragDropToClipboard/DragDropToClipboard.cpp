/*
*  Copyright (c) 2018 Leigh Johnston.
*
*  All rights reserved.
*
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions are
*  met:
*
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in the
*       documentation and/or other materials provided with the distribution.
*
*     * Neither the name of Leigh Johnston nor the names of any
*       other contributors to this software may be used to endorse or
*       promote products derived from this software without specific prior
*       written permission.
*
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
*  IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
*  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
*  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
*  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
*  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
*  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
*  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
*  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
*  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
*  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "stdafx.h"
#include <vector>
#include <string>
#include "DragDropToClipboard.h"

// Forward declarations of functions included in this code module:
INT_PTR CALLBACK DropFilesDlg(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

	DialogBox(hInstance, MAKEINTRESOURCE(IDD_DROP_AREA), NULL, DropFilesDlg);

    return 0;
}

bool ClipboardHasFiles()
{
	OpenClipboard(0);
	auto next = EnumClipboardFormats(0);
	while (next != 0)
	{
		if (next == CF_HDROP)
		{
			CloseClipboard();
			return true;
		}
		next = EnumClipboardFormats(next);
	}
	CloseClipboard();
	return false;
}

bool ClipboardHasPaths()
{
	if (!ClipboardHasFiles())
		return false;
	OpenClipboard(0);
	auto next = EnumClipboardFormats(0);
	while (next != 0)
	{
		if (next == CF_UNICODETEXT)
		{
			CloseClipboard();
			return true;
		}
		next = EnumClipboardFormats(next);
	}
	CloseClipboard();
	return false;
}

uint32_t sFileCount;

void CopyPathsToClipboard(HDROP hDropInfo)
{
	std::u16string files;
	sFileCount = DragQueryFile(hDropInfo, -1, NULL, 0);
	for (size_t i = 0; i < sFileCount; ++i)
	{
		char16_t pathBuffer[MAX_PATH + 1];
		DragQueryFile(hDropInfo, i, reinterpret_cast<LPWSTR>(pathBuffer), MAX_PATH + 1);
		if (!files.empty())
			files += u"\r\n";
		files += pathBuffer;
	}
	auto len = (files.size() + 1) * sizeof(files[0]);
	HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
	memcpy(GlobalLock(hMem), files.c_str(), len);
	GlobalUnlock(hMem);
	OpenClipboard(0);
	SetClipboardData(CF_UNICODETEXT, hMem);
	CloseClipboard();
}

uint32_t CountFilesInClipboard()
{
	uint32_t result = 0;
	OpenClipboard(0);
	HGLOBAL hGlobal = reinterpret_cast<HGLOBAL>(GetClipboardData(CF_HDROP));
	if (hGlobal)
	{
		HDROP hDrop = reinterpret_cast<HDROP>(GlobalLock(hGlobal));
		if (hDrop)
		{
			result = DragQueryFile(hDrop, -1, NULL, 0);
			GlobalUnlock(hGlobal);
		}
	}
	CloseClipboard();
	return result;
}

void CopyPathsToClipboard()
{
	OpenClipboard(0);
	HGLOBAL hGlobal = reinterpret_cast<HGLOBAL>(GetClipboardData(CF_HDROP));
	if (hGlobal)
	{
		HDROP hDrop = reinterpret_cast<HDROP>(GlobalLock(hGlobal));
		if (hDrop)
		{
			CopyPathsToClipboard(hDrop);
			GlobalUnlock(hGlobal);
		}
	}
	CloseClipboard();
}

void CopyToClipBoard(HDROP hDropInfo)
{
	auto source = GlobalLock(hDropInfo);
	auto sourceLen = GlobalSize(hDropInfo);
	HGLOBAL hDropInfoCopy = GlobalAlloc(GMEM_ZEROINIT | GMEM_MOVEABLE | GMEM_DDESHARE, sourceLen);
	memcpy(GlobalLock(hDropInfoCopy), source, sourceLen);
	GlobalUnlock(hDropInfoCopy);
	GlobalUnlock(hDropInfo);
	OpenClipboard(0);
	EmptyClipboard();
	SetClipboardData(CF_HDROP, hDropInfoCopy);
	CloseClipboard();
	CopyPathsToClipboard(hDropInfo);
	DragFinish(hDropInfo);
}

// Message handler for dialog.
INT_PTR CALLBACK DropFilesDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	static HWND hwndNextClipboardViewer;
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
	case WM_INITDIALOG:
		DragAcceptFiles(hDlg, TRUE);
		hwndNextClipboardViewer = SetClipboardViewer(hDlg);
		if (ClipboardHasFiles() && sFileCount == 0)
			sFileCount = CountFilesInClipboard();
		return (INT_PTR)TRUE;
	case WM_ERASEBKGND:
		{
			RECT rect;
			GetClientRect(hDlg, &rect);
			HDC hdc = reinterpret_cast<HDC>(wParam);
			FillRect(hdc, &rect, reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
			if (ClipboardHasFiles())
			{
				RECT rectInfoArea;
				GetWindowRect(GetDlgItem(hDlg, IDC_DROP_AREA), &rectInfoArea);
				ScreenToClient(hDlg, &reinterpret_cast<POINT*>(&rectInfoArea)[0]);
				ScreenToClient(hDlg, &reinterpret_cast<POINT*>(&rectInfoArea)[1]);
				rectInfoArea.top = rectInfoArea.bottom;
				rectInfoArea.bottom = rect.bottom;
				std::wstring msg;
				if (ClipboardHasPaths())
				{
					msg.resize(128);
					wsprintf(&msg[0], L"%d path(s)", sFileCount);
					msg.resize(msg.find(L'\0'));
				}
				else
				{
					msg = L"Click to copy path(s)";
				}
				SetTextColor(hdc, RGB(0, 0, 0));
				DrawText(hdc, msg.c_str(), msg.length(), &rectInfoArea, DT_CENTER);
			}
		}
		return (INT_PTR)TRUE;
	case WM_USER:
		return (INT_PTR)TRUE;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
	case WM_DROPFILES:
		CopyToClipBoard(reinterpret_cast<HDROP>(wParam));
		return (INT_PTR)TRUE;
	case WM_CHANGECBCHAIN:
		hwndNextClipboardViewer = reinterpret_cast<HWND>(lParam);
		if (reinterpret_cast<HWND>(wParam) == hwndNextClipboardViewer)
			hwndNextClipboardViewer = reinterpret_cast<HWND>(lParam);
		else if (hwndNextClipboardViewer != NULL)
			SendMessage(hwndNextClipboardViewer, WM_CHANGECBCHAIN, wParam, lParam);
		return (INT_PTR)TRUE;
	case WM_DRAWCLIPBOARD:
		{
			RECT rc;
			GetClientRect(hDlg, &rc);
			InvalidateRect(hDlg, &rc, TRUE);
		}
		if (hwndNextClipboardViewer != NULL)
			SendMessage(hwndNextClipboardViewer, WM_DRAWCLIPBOARD, wParam, lParam);
		return (INT_PTR)TRUE;
	case WM_SETCURSOR:
		if (ClipboardHasFiles() && !ClipboardHasPaths())
		{
			SetCursor(LoadCursor(NULL, IDC_HAND));
			return (INT_PTR)TRUE;
		}
		break;
	case WM_LBUTTONDOWN:
		if (ClipboardHasFiles() && !ClipboardHasPaths())
		{
			CopyPathsToClipboard();
		}
		return (INT_PTR)TRUE;
    }
    return (INT_PTR)FALSE;
}
