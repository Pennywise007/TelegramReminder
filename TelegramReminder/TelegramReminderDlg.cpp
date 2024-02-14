#include "pch.h"
#include "framework.h"
#include "TelegramReminder.h"
#include "TelegramReminderDlg.h"
#include "afxdialogex.h"
#include "TelegramThread.h"

#include <ext/thread/invoker.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CTelegramReminderDlg dialog
CTelegramReminderDlg::CTelegramReminderDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_TELEGRAMREMINDER_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CTelegramReminderDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_EDIT_TELEGRAM_TOKEN, m_editToken);
	DDX_Control(pDX, IDC_EDIT_TELEGRAM_PASSWORD, m_editPassword);
	DDX_Control(pDX, IDC_EDIT_MESSAGE, m_message);
	DDX_Control(pDX, IDC_BUTTON_RUN, m_buttonRun);
}

BEGIN_MESSAGE_MAP(CTelegramReminderDlg, CDialogEx)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_CLOSE()
	ON_BN_CLICKED(IDC_BUTTON_RUN, &CTelegramReminderDlg::OnBnClickedButtonRun)
	ON_BN_CLICKED(IDC_BUTTON_SEND_MESSAGE, &CTelegramReminderDlg::OnBnClickedButtonSendMessage)
END_MESSAGE_MAP()

BOOL CTelegramReminderDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	m_editToken.SetWindowTextW(m_settings.token.c_str());
	m_editPassword.SetWindowTextW(m_settings.password.c_str());

	m_errorDialog = std::make_shared<ErrorDialog>(this);


	return TRUE;  // return TRUE  unless you set the focus to a control
}

void CTelegramReminderDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting
		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

HCURSOR CTelegramReminderDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CTelegramReminderDlg::OnClose()
{
	CDialogEx::OnClose();

	StoreParameters();
	StopThread();
}

void CTelegramReminderDlg::StoreParameters()
{
	CString str;
	m_editToken.GetWindowTextW(str);
	m_settings.token = str;
	m_editPassword.GetWindowTextW(str);
	m_settings.password = str;
	m_settings.Store();
}

void CTelegramReminderDlg::StopThread()
{
	if (!m_telegramThread)
		return;

	m_telegramThread->StopTelegramThread();
	m_telegramThread = nullptr;
}

void CTelegramReminderDlg::SendMessageToUsers(const CString& text)
{
	if (m_settings.registeredUsers.empty())
	{
		m_errorDialog->ShowWindow((L"No registered users for hanlde: " + text).GetString());
		return;
	}

	const ITelegramThreadPtr thread = CreateTelegramThread(std::narrow(m_settings.token).c_str(),
		[errorDialog = m_errorDialog](const std::wstring& error)
		{
			errorDialog->ShowWindow(error);
		});

	for (const auto& user : m_settings.registeredUsers)
	{
		thread->SendMessageW(user.id, text.GetString());
	}
}

void CTelegramReminderDlg::OnBnClickedButtonRun()
{
	const auto onExecute = [&](bool start)
	{
		ext::InvokeMethodAsync([&, start]()
			{
				m_editPassword.EnableWindow(!start);
				m_editToken.EnableWindow(!start);
				m_buttonRun.SetWindowTextW(start ?
					L"Stop waiting for users" :
					L"Run bot and wait for users");
			});
	};

	const bool threadWorking = !!m_telegramThread;

	if (threadWorking)
	{
		StopThread();
		onExecute(false);
		return;
	}

	onExecute(true);

	m_telegramThread = CreateTelegramThread(std::narrow(m_settings.token).c_str(),
		[errorDialog = m_errorDialog](const std::wstring& error)
		{
			ext::InvokeMethodAsync([error, errorDialog]()
				{
					errorDialog->ShowWindow(error);
				});
		});

	StoreParameters();

	const auto onMessage = [pass = m_settings.password, telegramThread = m_telegramThread]
		(const MessagePtr& commandMessage)
		{
			const std::wstring messageText = getUNICODEString(commandMessage->text);
			if (messageText != pass)
			{
				telegramThread->SendMessage(commandMessage->chat->id, L"Unknown message");
				return;
			}

			ext::InvokeMethodAsync([telegramThread, pUser = commandMessage->from]()
				{
					auto& settings = ext::get_service<Settings>();

					auto& users = settings.registeredUsers;
					const bool exist = std::any_of(users.begin(), users.end(),
						[&](const Settings::User& user)
						{
							return user.id == pUser->id;
						});
					if (exist)
						telegramThread->SendMessage(pUser->id, L"User already registered");
					else
					{
						users.emplace_back(pUser);
						settings.Store();

						telegramThread->SendMessage(pUser->id, L"User registered");
					}
				});
		};
	const auto onUnknownCommand = [onMessage](const MessagePtr& commandMessage)
	{
		onMessage(commandMessage);
	};

	m_telegramThread->StartTelegramThread({}, onUnknownCommand, onMessage);
}

void CTelegramReminderDlg::OnBnClickedButtonSendMessage()
{
	if (m_settings.registeredUsers.empty())
	{
		MessageBox(L"No registered users", L"Can't send message", MB_ICONERROR);
		return;
	}

	CString text;
	m_message.GetWindowTextW(text);
	SendMessageToUsers(text);

	MessageBox(std::string_swprintf(L"Message sent to %u users", m_settings.registeredUsers.size()).c_str(),
		L"Can't send message", MB_ICONERROR);
}

BOOL CTelegramReminderDlg::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message == WM_KEYDOWN)
	{
		switch (pMsg->wParam)
		{
		case VK_RETURN:
			OnBnClickedButtonSendMessage();
			return TRUE;
		case VK_ESCAPE:
			m_message.SetWindowTextW(L"");
			return TRUE;
		default:
			break;
		}
	}
	return CDialogEx::PreTranslateMessage(pMsg);
}
