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
#include <regex>
#include "DragDropToClipboard.h"

HINSTANCE sInstanceHandle;
HWND sDialogHandle;

// Forward declarations of functions included in this code module:
INT_PTR CALLBACK DropFilesDlg(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
	sInstanceHandle = hInstance;
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

	DialogBox(hInstance, MAKEINTRESOURCE(IDD_DROP_AREA), NULL, DropFilesDlg);

    return 0;
}

HGLOBAL CopyObject(HGLOBAL hSource)
{
	auto source = GlobalLock(hSource);
	auto sourceLen = GlobalSize(hSource);
	HGLOBAL hCopy = GlobalAlloc(GMEM_ZEROINIT | GMEM_MOVEABLE | GMEM_DDESHARE, sourceLen);
	memcpy(GlobalLock(hCopy), source, sourceLen);
	GlobalUnlock(hCopy);
	GlobalUnlock(hSource);
	return hCopy;
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
	std::wstring regexFrom(1024, L'\0');
	std::wstring regexTo(1024, L'\0');
	regexFrom.resize(GetWindowText(GetDlgItem(sDialogHandle, IDC_FROM), &regexFrom[0], regexFrom.size() - 1));
	regexTo.resize(GetWindowText(GetDlgItem(sDialogHandle, IDC_TO), &regexTo[0], regexTo.size() - 1));
	std::wstring files;
	sFileCount = DragQueryFile(hDropInfo, -1, NULL, 0);
	for (size_t i = 0; i < sFileCount; ++i)
	{
		std::wstring path(MAX_PATH + 1, L'\0');
		DragQueryFile(hDropInfo, i, &path[0], MAX_PATH + 1);
		path.resize(std::wcslen(path.c_str()));
		std::wstring file = path;
		if (!regexFrom.empty() && !regexTo.empty())
			file = std::regex_replace(path, std::wregex{ regexFrom }, regexTo);
		if (!file.empty())
		{
			if (!files.empty())
				files += L"\r\n";
			files += file;
		}
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

void DeletePathsFromClipboard()
{
	OpenClipboard(0);
	HGLOBAL hCopy = CopyObject(reinterpret_cast<HGLOBAL>(GetClipboardData(CF_HDROP)));
	EmptyClipboard();
	SetClipboardData(CF_HDROP, hCopy);
	CloseClipboard();
}

bool RemoveFormatting()
{
	OpenClipboard(0);
	auto next = EnumClipboardFormats(0);
	bool foundFormatting = false;
	while (next != 0 && !foundFormatting)
	{
		switch (next)
		{
		case CF_UNICODETEXT:
		case CF_TEXT:
		case CF_HDROP:
			break;
		default:
			foundFormatting = true;
			break;
		}
		next = EnumClipboardFormats(next);
	}
	if (!foundFormatting)
	{
		CloseClipboard();
		return false;
	}
	HGLOBAL copies[3];
	copies[0] = CopyObject(reinterpret_cast<HGLOBAL>(GetClipboardData(CF_HDROP)));
	copies[1] = CopyObject(reinterpret_cast<HGLOBAL>(GetClipboardData(CF_TEXT)));
	copies[2] = CopyObject(reinterpret_cast<HGLOBAL>(GetClipboardData(CF_UNICODETEXT)));
	EmptyClipboard();
	if (copies[0] != NULL)
		SetClipboardData(CF_HDROP, copies[0]);
	if (copies[1] != NULL)
		SetClipboardData(CF_TEXT, copies[1]);
	if (copies[2] != NULL)
		SetClipboardData(CF_UNICODETEXT, copies[2]);
	CloseClipboard();
	return true;
}

void CopyToClipBoard(HDROP hDropInfo)
{
	OpenClipboard(0);
	EmptyClipboard();
	SetClipboardData(CF_HDROP, CopyObject(hDropInfo));
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
		sDialogHandle = hDlg;
		DragAcceptFiles(hDlg, TRUE);
		hwndNextClipboardViewer = SetClipboardViewer(hDlg);
		if (ClipboardHasFiles() && sFileCount == 0)
			sFileCount = CountFilesInClipboard();
		SetWindowText(GetDlgItem(hDlg, IDC_FROM), L"^(.*)$");
		SetWindowText(GetDlgItem(hDlg, IDC_TO), L"$1");
		return (INT_PTR)TRUE;
	case WM_ERASEBKGND:
		{
			RECT rect;
			GetClientRect(hDlg, &rect);
			HDC hdc = reinterpret_cast<HDC>(wParam);
			FillRect(hdc, &rect, reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
			SetTextColor(hdc, RGB(0, 0, 0));
			auto hOldFont = SelectFont(hdc, reinterpret_cast<HFONT>(SendMessage(hDlg, WM_GETFONT, 0, 0)));
			// Tell me how to have a non-owner draw CHECKBOX with a transparent background and I will
			// throw the next several lines of bollocks away.  Gotta love WIN32...
			RECT rectRemoveFormatting;
			GetWindowRect(GetDlgItem(hDlg, IDC_REMOVE_FORMATTING), &rectRemoveFormatting);
			ScreenToClient(hDlg, &reinterpret_cast<POINT*>(&rectRemoveFormatting)[0]);
			ScreenToClient(hDlg, &reinterpret_cast<POINT*>(&rectRemoveFormatting)[1]);
			rectRemoveFormatting.right = rectRemoveFormatting.left;
			rectRemoveFormatting.left = rect.left;
			std::wstring removeFormattingLabel{ L"Auto remove formatting: " };
			DrawText(hdc, removeFormattingLabel.c_str(), removeFormattingLabel.size(), &rectRemoveFormatting, DT_RIGHT);
			RECT rectAutoCopy;
			GetWindowRect(GetDlgItem(hDlg, IDC_AUTO_COPY), &rectAutoCopy);
			ScreenToClient(hDlg, &reinterpret_cast<POINT*>(&rectAutoCopy)[0]);
			ScreenToClient(hDlg, &reinterpret_cast<POINT*>(&rectAutoCopy)[1]);
			rectAutoCopy.right = rectAutoCopy.left;
			rectAutoCopy.left = rect.left;
			std::wstring autoCopyLabel{ L"Auto copy: " };
			DrawText(hdc, autoCopyLabel.c_str(), autoCopyLabel.size(), &rectAutoCopy, DT_RIGHT);
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
				DrawText(hdc, msg.c_str(), msg.length(), &rectInfoArea, DT_CENTER);
				SelectFont(hdc, hOldFont);
			}
		}
		return (INT_PTR)TRUE;
	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		else if ((LOWORD(wParam) == IDC_FROM || LOWORD(wParam) == IDC_TO) && HIWORD(wParam) == EN_CHANGE)
		{
			if (ClipboardHasPaths())
				DeletePathsFromClipboard();
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
			static bool sInHere = false;
			if (!sInHere)
			{
				sInHere = true;
				if (ClipboardHasFiles() && !ClipboardHasPaths() && Button_GetCheck(GetDlgItem(hDlg, IDC_AUTO_COPY)) == BST_CHECKED)
				{
					CopyPathsToClipboard();
					RECT rect;
					GetClientRect(hDlg, &rect);
					InvalidateRect(hDlg, &rect, TRUE);
				}
				if (Button_GetCheck(GetDlgItem(hDlg, IDC_REMOVE_FORMATTING)) == BST_CHECKED)
				{
					if (RemoveFormatting())
						Beep(1000, 100);
				}
				sInHere = false;
			}
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
