// ErrorDialog.cpp : implementation file
//

#include "pch.h"
#include "TelegramReminder.h"
#include "afxdialogex.h"
#include "ErrorDialog.h"

#include <ext/core/check.h>

IMPLEMENT_DYNAMIC(ErrorDialog, CDialogEx)

ErrorDialog::ErrorDialog(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DIALOG_ERROR, pParent)
	, m_parent(pParent)
{}

void ErrorDialog::ShowWindow(const std::wstring& error)
{
	if (IsWindow(*this))
	{
		CString text;
		m_errorEdit.GetWindowTextW(text);
		text.Append((L"\r\n" + error).c_str());
		m_errorEdit.SetWindowTextW(text);
	}
	else
	{
		Create(IDD_DIALOG_ERROR, m_parent);
		m_errorEdit.SetWindowTextW(error.c_str());
	}
	CDialogEx::ShowWindow(SW_SHOW);
}

void ErrorDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_EDIT_ERROR, m_errorEdit);
}

BEGIN_MESSAGE_MAP(ErrorDialog, CDialogEx)
END_MESSAGE_MAP()
