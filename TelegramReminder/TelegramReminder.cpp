
// TelegramReminder.cpp : Defines the class behaviors for the application.
//

#include "pch.h"
#include "framework.h"
#include "TelegramReminder.h"
#include "TelegramReminderDlg.h"

#include <ext/core.h>
#include <ext/std/filesystem.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace {

HWND FindMainWindowOfExe(CString exeName)
{
    HWND result = nullptr;

    // получаем перечень всех запущенных процессов
    const HANDLE m_hSnapshot = ::CreateToolhelp32Snapshot(TH32CS_SNAPALL, 0);
    if (m_hSnapshot != INVALID_HANDLE_VALUE)
    {
        if (exeName.Find(L".exe") == -1)
            exeName += L".exe";
        exeName.MakeLower();

        const auto currentProcessId = ::GetCurrentProcessId();

        PROCESSENTRY32 pe = { sizeof(pe) };

        // ищем все не наши процессы с именем нашей экзешки
        for (auto bOk = ::Process32First(m_hSnapshot, &pe); bOk;
             bOk = ::Process32Next(m_hSnapshot, &pe))
        {
            if (pe.th32ProcessID != currentProcessId && (CString(pe.szExeFile).MakeLower() == exeName))
            {
                struct WindowInfo
                {
                    WindowInfo(const DWORD _processId, HWND& _result)
                        : processId(_processId)
                        , resultHandle(_result)
                    {}

                    DWORD processId;
                    HWND& resultHandle;
                } info(pe.th32ProcessID, result);

                // ищем основное окно у процесса
                ::EnumWindows([](HWND handle, LPARAM lParam)
                {
                    // фильтруем не основные окна, считаем что это окна без владельцев
                    // и икона со стилем WS_EX_APPWINDOW(вспомогательные окна могут быть тоже без родителя)
                    if (::GetWindow(handle, GW_OWNER) != (HWND)0 ||
                        !(GetWindowLongPtr(handle, GWL_EXSTYLE) & WS_EX_APPWINDOW))
                        return TRUE;

                    // процесс который ищем
                    const DWORD searchProcessId = reinterpret_cast<WindowInfo*>(lParam)->processId;
                    // процесс окна
                    DWORD windowProcessId = 0;
                    ::GetWindowThreadProcessId(handle, &windowProcessId);

                    if (searchProcessId != windowProcessId)
                        return TRUE;

                    reinterpret_cast<WindowInfo*>(lParam)->resultHandle = handle;

                    return FALSE;
                }, LPARAM(&info));

                break;
            }
        }
        CloseHandle(m_hSnapshot);
    }

    return result;
}

bool AnotherAppIsRunning()
{
	const std::wstring exeName = std::filesystem::get_binary_name().c_str();

	static CMutex m_RunOnceMutex(FALSE, exeName.c_str());
	static CSingleLock m_RunOnceLock(&m_RunOnceMutex);

	// проверяем, не запущена ли другая копия нашей программы
	if (m_RunOnceLock.Lock(0))
		return false;

	// вычленяем из полного пути имя экзешки
	const HWND handle = FindMainWindowOfExe(exeName.c_str());
	if (handle != nullptr)
	{
		// если окно скрыто - показываем его
		if (!::IsWindowVisible(handle))
			::ShowWindow(handle, SW_SHOW);

		::OpenIcon(handle);
		::SetActiveWindow(handle);
		::SetForegroundWindow(handle);

		std::wstring commandLine = GetCommandLine();
		COPYDATASTRUCT data = { 0 };
		data.cbData = (commandLine.size() + 1) * sizeof(wchar_t);
		data.lpData = reinterpret_cast<PVOID>(commandLine.data());
		SendMessage(handle, WM_COPYDATA, reinterpret_cast<WPARAM>(handle), reinterpret_cast<LPARAM>(&data));
	}

	return true;
}

}
// CTelegramReminderApp

BEGIN_MESSAGE_MAP(CTelegramReminderApp, CWinApp)
	ON_COMMAND(ID_HELP, &CWinApp::OnHelp)
END_MESSAGE_MAP()


// CTelegramReminderApp construction

CTelegramReminderApp::CTelegramReminderApp()
{
	// support Restart Manager
	m_dwRestartManagerSupportFlags = AFX_RESTART_MANAGER_SUPPORT_RESTART;

	// TODO: add construction code here,
	// Place all significant initialization in InitInstance
}


// The one and only CTelegramReminderApp object

CTelegramReminderApp theApp;


// CTelegramReminderApp initialization

BOOL CTelegramReminderApp::InitInstance()
{
	// InitCommonControlsEx() is required on Windows XP if an application
	// manifest specifies use of ComCtl32.dll version 6 or later to enable
	// visual styles.  Otherwise, any window creation will fail.
	INITCOMMONCONTROLSEX InitCtrls;
	InitCtrls.dwSize = sizeof(InitCtrls);
	// Set this to include all the common control classes you want to use
	// in your application.
	InitCtrls.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&InitCtrls);

	CWinApp::InitInstance();

	if (AnotherAppIsRunning())
		return FALSE;

	AfxEnableControlContainer();

	// Create the shell manager, in case the dialog contains
	// any shell tree view or shell list view controls.
	CShellManager *pShellManager = new CShellManager;

	// Activate "Windows Native" visual manager for enabling themes in MFC controls
	CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerWindows));

	// Standard initialization
	// If you are not using these features and wish to reduce the size
	// of your final executable, you should remove from the following
	// the specific initialization routines you do not need
	// Change the registry key under which our settings are stored
	// TODO: You should modify this string to be something appropriate
	// such as the name of your company or organization
	SetRegistryKey(_T("Local AppWizard-Generated Applications"));

	ext::core::Init();

	CTelegramReminderDlg dlg;
	m_pMainWnd = &dlg;
	INT_PTR nResponse = dlg.DoModal();
	if (nResponse == IDOK)
	{
		// TODO: Place code here to handle when the dialog is
		//  dismissed with OK
	}
	else if (nResponse == IDCANCEL)
	{
		// TODO: Place code here to handle when the dialog is
		//  dismissed with Cancel
	}
	else if (nResponse == -1)
	{
		TRACE(traceAppMsg, 0, "Warning: dialog creation failed, so application is terminating unexpectedly.\n");
		TRACE(traceAppMsg, 0, "Warning: if you are using MFC controls on the dialog, you cannot #define _AFX_NO_MFC_CONTROLS_IN_DIALOGS.\n");
	}

	// Delete the shell manager created above.
	if (pShellManager != nullptr)
	{
		delete pShellManager;
	}

#if !defined(_AFXDLL) && !defined(_AFX_NO_MFC_CONTROLS_IN_DIALOGS)
	ControlBarCleanUp();
#endif

	// Since the dialog has been closed, return FALSE so that we exit the
	//  application, rather than start the application's message pump.
	return FALSE;
}

