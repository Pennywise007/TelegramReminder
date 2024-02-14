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
	DDX_Control(pDX, IDC_DATETIMEPICKER_START, m_timeNotificationsStart);
	DDX_Control(pDX, IDC_DATETIMEPICKER_END, m_timeNotificationsEnd);
	DDX_Control(pDX, IDC_CHECK_WEEKDAYS, m_checkWeekdays);
	DDX_Control(pDX, IDC_CHECK_WEEKENDS, m_checkWeekends);
	DDX_Control(pDX, IDC_EDIT_INTERVAL, m_editInterval);
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

	m_editInterval.UsePositiveDigitsOnly();

	m_editToken.SetWindowTextW(m_settings.token.c_str());
	m_editPassword.SetWindowTextW(m_settings.password.c_str());

	m_timeNotificationsStart.SetTime(&m_settings.start);
	m_timeNotificationsEnd.SetTime(&m_settings.end);

	m_editInterval.SetWindowTextW(std::to_wstring(m_settings.interval).c_str());

	m_errorDialog = std::make_shared<ErrorDialog>(this);

	m_telegramThread = CreateTelegramThread(std::narrow(m_settings.token).c_str(),
		[errorDialog = m_errorDialog](const std::wstring& error)
		{
			ext::InvokeMethodAsync([error, errorDialog]()
									{
										errorDialog->ShowWindow(error);
									});
		});

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

	if (m_reminderThread.joinable())
		StopThread();
}

void CTelegramReminderDlg::StoreParameters()
{
	CString str;
	m_editToken.GetWindowTextW(str);
	m_settings.token = str;
	m_editPassword.GetWindowTextW(str);
	m_settings.password = str;

	m_timeNotificationsStart.GetTime(m_settings.start);
	m_timeNotificationsEnd.GetTime(m_settings.end);

	m_settings.weekdays = m_checkWeekdays.GetCheck() != 0;
	m_settings.weekends = m_checkWeekends.GetCheck() != 0;

	m_editInterval.GetWindowTextW(str);
	m_settings.interval = std::stoi(str.GetString());

	m_message.GetWindowTextW(str);
	m_settings.message = str;

	m_settings.Store();
}

void CTelegramReminderDlg::StopThread()
{
	m_reminderThread.interrupt();
	m_telegramThread->StopTelegramThread();
	m_reminderThread.join();
}

void CTelegramReminderDlg::SendMessageToUsers()
{
	ext::InvokeMethod([&]() {
		StoreParameters();

		if (m_settings.registeredUsers.empty())
		{
			m_errorDialog->ShowWindow(L"No registered users to send message");
			return;
		}

		for (const auto& user : m_settings.registeredUsers)
		{
			m_telegramThread->SendMessage(user.id, m_settings.message);
		}
	});
}

void CTelegramReminderDlg::OnBnClickedButtonRun()
{
	const auto onExecute = [&](bool start)
	{
			//ext::InvokeMethodAsync([&, start]()
			//	{

		const std::vector<CWnd*> activeControls {
			&m_editPassword,
			&m_editToken,
			&m_timeNotificationsStart,
			&m_timeNotificationsEnd,
			&m_checkWeekdays,
			&m_checkWeekends,
			&m_editInterval,
		};

		for (auto ctrl : activeControls)
		{
			ctrl->EnableWindow(!start);
		}

		m_buttonRun.SetWindowTextW(start ?
			L"Stop waiting for users" :
			L"Run bot and wait for users");
	};

	if (m_reminderThread.joinable())
	{
		StopThread();
		onExecute(false);
		return;
	}

	const auto validateParams = [&]() {
		CTime startTime, endTime;
		m_timeNotificationsStart.GetTime(startTime);
		m_timeNotificationsEnd.GetTime(endTime);

		if (startTime == endTime)
		{
			MessageBoxW(L"Start time equal to the end time", L"Invalid params");
			return false;
		}

		if (startTime > endTime)
		{
			const auto res = MessageBoxW(L"Start time is after end time, do you want to swap them?", L"Invalid params", MB_OKCANCEL);
			if (res != MB_OK)
				return false;

			m_timeNotificationsStart.SetTime(&endTime);
			m_timeNotificationsEnd.SetTime(&startTime);
			std::swap(startTime, endTime);
		}

		return true;
	};

	if (!validateParams())
		return

	onExecute(true);

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

	std::list<ITelegramThread::CommandInfo> commands{
		{
			L"ping", L"Verify that reminder is working", [thread = m_telegramThread](const TgBot::Message::Ptr message) {
				thread->SendMessage(message->from->id, L"Reminder is online");
		}},
		{
			L"stop_reminder", L"Stop the reminder", [&](const TgBot::Message::Ptr message) {
				if (m_reminderThread.joinable())
				{
					ext::InvokeMethod([&]()
						{
							StopThread();
							m_telegramThread->SendMessage(message->from->id, L"Reminder stopped");
							onExecute(false);
						});
				}
		}},
		{
			L"unregister", L"Unregister from notifications", [&](const TgBot::Message::Ptr message) {
				ext::InvokeMethod([&]() {
					auto it = std::find_if(m_settings.registeredUsers.begin(), m_settings.registeredUsers.end(),
						[&](const Settings::User& user)
						{
							return user.id == message->from->id;
						});
					if (it == m_settings.registeredUsers.end())
					{
						m_telegramThread->SendMessage(message->from->id, L"User not registered");
						return;
					}

					m_settings.registeredUsers.erase(it);
					m_settings.Store();
					m_telegramThread->SendMessage(message->from->id, L"User unregistered");
				});
		}},
	};

	m_telegramThread->StartTelegramThread(commands, onUnknownCommand, onMessage);
	m_reminderThread.run([&](CTime startTime, CTime endTime, int intervalInMinutes) {
		const auto wait = [intervalInMinutes]() {
			try
			{
				ext::this_thread::interruptible_sleep_for(std::chrono::minutes(intervalInMinutes));
				return true;
			}
			catch (ext::thread::thread_interrupted)
			{
				return false;
			}
		};

		const auto startHour(startTime.GetHour()), startMinute(startTime.GetMinute()), endHour(endTime.GetHour()), endMinute(endTime.GetMinute());
		do {
			const auto curTime = CTime::GetCurrentTime();

			const auto curHour = curTime.GetHour();
			const auto curMinute = curTime.GetMinute();
			if (curHour < startHour && curHour > endHour)
				continue;

			if (curHour == startHour && curMinute < startMinute)
				continue;
			if (curHour == endHour && curMinute > endMinute)
				continue;

			SendMessageToUsers();
		} while (wait());
	}, m_settings.start, m_settings.end, m_settings.interval);
}

void CTelegramReminderDlg::OnBnClickedButtonSendMessage()
{
	SendMessageToUsers();

	MessageBox(std::string_swprintf(L"Message sent to %u users", m_settings.registeredUsers.size()).c_str(),
		L"Send message result", MB_OK);
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
