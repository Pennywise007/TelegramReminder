#pragma once

#include "afxdialogex.h"

#include <string>

// ErrorDialog dialog

class ErrorDialog : public CDialogEx
{
	DECLARE_DYNAMIC(ErrorDialog)

public:
	ErrorDialog(CWnd* pParent = nullptr);   // standard constructor

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DIALOG_ERROR };
#endif

	void ShowWindow(const std::wstring& error);

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()

public:
	CEdit m_errorEdit;
	CWnd* m_parent;
};
