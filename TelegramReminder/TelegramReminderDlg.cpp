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
	ext::core::Init();
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
	m_editInterval.SetUseOnlyIntegersValue();

	m_editToken.SetWindowTextW(m_settings.token.c_str());
	m_editPassword.SetWindowTextW(m_settings.password.c_str());
	m_message.SetWindowTextW(m_settings.message.c_str());

	m_timeNotificationsStart.SetFormat(L"HH:mm");
	m_timeNotificationsStart.SetTime(&m_settings.start);
	m_timeNotificationsEnd.SetFormat(L"HH:mm");
	m_timeNotificationsEnd.SetTime(&m_settings.end);

	m_checkWeekdays.SetCheck(m_settings.weekdays ? 1 : 0);
	m_checkWeekends.SetCheck(m_settings.weekends ? 1 : 0);

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
	
	EndDialog(IDOK);
}

void CTelegramReminderDlg::StoreParameters()
{
	CString str;
	m_editToken.GetWindowTextW(str);
	m_settings.token = str;
	m_editPassword.GetWindowTextW(str);
	m_settings.password = str;
	m_message.GetWindowTextW(str);
	m_settings.message = str;

	m_timeNotificationsStart.GetTime(m_settings.start);
	m_timeNotificationsEnd.GetTime(m_settings.end);

	m_settings.weekdays = m_checkWeekdays.GetCheck() != 0;
	m_settings.weekends = m_checkWeekends.GetCheck() != 0;

	m_editInterval.GetWindowTextW(str);
	m_settings.interval = std::stoi(str.GetString());

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

void CTelegramReminderDlg::EnableControlsOnReminder(bool start)
{
	const std::vector<CWnd*> activeControls{
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
		L"Stop reminder" :
		L"Run reminder");
}

bool CTelegramReminderDlg::ValidateParameters()
{
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
		if (res != IDOK)
			return false;

		m_timeNotificationsStart.SetTime(&endTime);
		m_timeNotificationsEnd.SetTime(&startTime);
		std::swap(startTime, endTime);
	}

	return true;
}

void CTelegramReminderDlg::OnBnClickedButtonRun()
{
	if (m_reminderThread.joinable())
	{
		StopThread();
		EnableControlsOnReminder(false);
		return;
	}

	if (!ValidateParameters())
		return;

	EnableControlsOnReminder(true);
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

			ext::InvokeMethodAsync([telegramThread, pUser = commandMessage->from, chatId = commandMessage->chat->id]()
				{
					auto& settings = ext::get_service<Settings>();

					const bool exist = std::any_of(settings.registeredUsers.begin(), settings.registeredUsers.end(),
						[&](const Settings::User& user)
						{
							return user.id == pUser->id;
						});
					if (exist)
						telegramThread->SendMessage(chatId, L"User already registered");
					else
					{
						settings.registeredUsers.emplace_back(pUser);
						settings.Store();

						telegramThread->SendMessage(chatId, L"User registered");
					}
				});
		};
	const auto onUnknownCommand = [onMessage](const MessagePtr& commandMessage)
	{
		onMessage(commandMessage);
	};

	std::list<ITelegramThread::CommandInfo> commands{
		{
			L"ping", L"Verify that reminder is working", [&](const TgBot::Message::Ptr message) {
				if (m_reminderThread.joinable())
					m_telegramThread->SendMessage(message->chat->id, L"Reminder is online");
				else
					m_telegramThread->SendMessage(message->chat->id, L"Reminder is paused");
		}},
		{
			L"continue_reminder", L"Continue reminder work after pausing", [&](const TgBot::Message::Ptr message) {
				ext::InvokeMethodAsync([&, userId = message->from->id, chatId = message->chat->id]()
				{
					const bool userRegistered = std::any_of(m_settings.registeredUsers.begin(), m_settings.registeredUsers.end(),
						[&](const Settings::User& user)
						{
							return user.id == userId;
						});
					if (!userRegistered)
					{
						m_telegramThread->SendMessage(chatId, L"User not registered");
						return;
					}
					if (m_reminderThread.joinable())
					{
						m_telegramThread->SendMessage(chatId, L"Reminder is already working");
						return;
					}
								
					m_reminderThread.run(&CTelegramReminderDlg::ThreadFunction, this);
					EnableControlsOnReminder(false);
					m_telegramThread->SendMessage(chatId, L"Reminder started");
				});
			}
		},
		{
			L"pause_reminder", L"Pause the reminder, to continue use /continue_reminder", [&](const TgBot::Message::Ptr message) {
				ext::InvokeMethodAsync([&, userId = message->from->id, chatId = message->chat->id]()
				{
					const bool userRegistered = std::any_of(m_settings.registeredUsers.begin(), m_settings.registeredUsers.end(),
						[&](const Settings::User& user)
						{
							return user.id == userId;
						});
					if (!userRegistered)
					{
						m_telegramThread->SendMessage(chatId, L"User not registered");
						return;
					}
					if (!m_reminderThread.joinable())
					{
						m_telegramThread->SendMessage(chatId, L"Reminder is not running");
						return;
					}

					m_reminderThread.interrupt_and_join();
					m_telegramThread->SendMessage(chatId, L"Reminder paused");
					EnableControlsOnReminder(false);
					m_buttonRun.SetWindowTextW(L"Reminder paused, stop reminder");
				});
			}
		},
		{
			L"unregister", L"Unregister from notifications", [&](const TgBot::Message::Ptr message) {
				ext::InvokeMethodAsync([&, userId = message->from->id, chatId = message->chat->id]() {
					auto it = std::find_if(m_settings.registeredUsers.begin(), m_settings.registeredUsers.end(),
						[&](const Settings::User& user)
						{
							return user.id == userId;
						});
					if (it == m_settings.registeredUsers.end())
					{
						m_telegramThread->SendMessage(chatId, L"User not registered");
						return;
					}

					m_settings.registeredUsers.erase(it);
					m_settings.Store();
					m_telegramThread->SendMessage(chatId, L"User unregistered");
				});
		}},
	};

	m_telegramThread = CreateTelegramThread(std::narrow(m_settings.token).c_str(),
		[errorDialog = m_errorDialog](const std::wstring& error)
		{
			ext::InvokeMethodAsync([error, errorDialog]()
									{
										errorDialog->ShowWindow(error);
									});
		});
	try
	{
		m_telegramThread->StartTelegramThread(commands, onUnknownCommand, onMessage);
	}
	catch (const std::exception& e)
	{
		MessageBoxA(m_hWnd, e.what(), "Failed to start telegram bot", MB_ICONERROR);
		m_telegramThread->StopTelegramThread();
		EnableControlsOnReminder(false);
		return;
	}

	m_reminderThread.run(&CTelegramReminderDlg::ThreadFunction, this);
}

void CTelegramReminderDlg::ThreadFunction()
{
	int startHour, startMinute, endHour, endMinute;
	int intervalInMinutes;
	bool workOnWeekdays, workOnWeekends;

	ext::InvokeMethod([&]() {
		startHour = m_settings.start.GetHour();
		startMinute = m_settings.start.GetMinute();
		endHour = m_settings.end.GetHour();
		endMinute = m_settings.end.GetMinute();
		intervalInMinutes = m_settings.interval;
		workOnWeekdays = m_settings.weekdays;
		workOnWeekends = m_settings.weekends;
	});

	using namespace std::chrono;
	const auto setHourAndMinute = [](system_clock::time_point& tp, int hour, int minute) {
		std::time_t time = system_clock::to_time_t(tp);
		std::tm timeStruct;
		localtime_s(&timeStruct, &time);
		timeStruct.tm_hour = hour;
		timeStruct.tm_min = minute;
		timeStruct.tm_sec = 0;
		tp = system_clock::from_time_t(std::mktime(&timeStruct));
	};
	
	auto startTime = system_clock::now(), endTime = startTime;
	setHourAndMinute(startTime, startHour, startMinute);
	setHourAndMinute(endTime, endHour, endMinute);

	bool waitForStartTime = false;

	const auto wait = [&]() {
		try
		{
			if (waitForStartTime)
				ext::this_thread::interruptible_sleep_until(startTime);
			else
			{
				const auto minutesSinceStart = std::chrono::duration_cast<std::chrono::minutes>(system_clock::now() - startTime).count();
				const int possibleIterations = minutesSinceStart / intervalInMinutes;

				const auto nextMessageTime = startTime + std::chrono::minutes(intervalInMinutes * (possibleIterations + 1));

				ext::this_thread::interruptible_sleep_until(nextMessageTime);
			}
			return true;
		}
		catch (ext::thread::thread_interrupted)
		{
			return false;
		}
	};

	while (wait()) {
		waitForStartTime = false;

		const auto curTime = system_clock::now();

		if (curTime < startTime)
		{
			waitForStartTime = true;
			continue;
		}
		else
		{
			std::time_t time = system_clock::to_time_t(curTime);
			std::tm timeStruct;
			localtime_s(&timeStruct, &time);

			const bool notWorkingDuringWeekday = !workOnWeekdays && (timeStruct.tm_wday > 0 || timeStruct.tm_wday < 6);
			const bool notWorkingDuringWeekend = !workOnWeekdays && (timeStruct.tm_wday == 0 || timeStruct.tm_wday == 6);

			if (curTime > endTime || notWorkingDuringWeekday || notWorkingDuringWeekend)
			{
				startTime += std::chrono::hours(24);
				endTime += std::chrono::hours(24);

				waitForStartTime = true;
				continue;
			}
		}

		SendMessageToUsers();
	}
}

void CTelegramReminderDlg::OnBnClickedButtonSendMessage()
{
	SendMessageToUsers();

	MessageBox(std::string_swprintf(L"Message sent to %u users", m_settings.registeredUsers.size()).c_str(),
		L"Send message result", MB_OK);
}

void CTelegramReminderDlg::OnOK()
{}

void CTelegramReminderDlg::OnCancel()
{}
