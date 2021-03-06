//////////////////////////////////////////////////////////////////////
//
// Redirector - to redirect the input / output of a console
//
// Developer: Jeff Lee
// Dec 10, 2001
//
//////////////////////////////////////////////////////////////////////

#include <windows.h>
#include <tchar.h>
#include <crtdbg.h>
#include <iostream>
#include <process.h>
#include "Redir.h"
#include "tstring.h"

using namespace std;

//#define _TEST_REDIR
#define BUFF_SIZE   4024

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CRedirector::CRedirector(std::ostream *predirout, CRITICAL_SECTION *pcs) :
    m_hStdinWrite(NULL),
    m_hStdoutRead(NULL),
    m_hChildProcess(NULL),
    m_hThread(NULL),
    m_hEvtStop(NULL),
    m_dwThreadId(0),
    m_dwWaitTime(100),
    m_predirout(predirout),
    m_pcs(pcs)
{

}

CRedirector::~CRedirector()
{
	Close();
}

//////////////////////////////////////////////////////////////////////
// CRedirector implementation
//////////////////////////////////////////////////////////////////////

BOOL CRedirector::Open(LPTSTR pszCmdLine, LPCTSTR pszStartDir)
{
    if(!m_predirout) // if no output stream buffer was specified then launch the process with no redirection
        return LaunchChildNoRedir(pszCmdLine, pszStartDir);

    HANDLE hStdoutReadTmp;				// parent stdout read handle
    HANDLE hStdoutWrite, hStderrWrite;	// child stdout write handle
    HANDLE hStdinWriteTmp;				// parent stdin write handle
    HANDLE hStdinRead;					// child stdin read handle
    SECURITY_ATTRIBUTES sa;

    Close();
    hStdoutReadTmp = NULL;
    hStdoutWrite = hStderrWrite = NULL;
    hStdinWriteTmp = NULL;
    hStdinRead = NULL;

    // Set up the security attributes struct.
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;

    BOOL bOK = FALSE;
    __try
    {
        // Create a child stdout pipe.
        if (!::CreatePipe(&hStdoutReadTmp, &hStdoutWrite, &sa, 0))
            __leave;

        // Create a duplicate of the stdout write handle for the std
        // error write handle. This is necessary in case the child
        // application closes one of its std output handles.
        if (!::DuplicateHandle(
            ::GetCurrentProcess(),
            hStdoutWrite,
            ::GetCurrentProcess(),
            &hStderrWrite,
            0, TRUE,
            DUPLICATE_SAME_ACCESS))
            __leave;

        // Create a child stdin pipe.
        if (!::CreatePipe(&hStdinRead, &hStdinWriteTmp, &sa, 0))
            __leave;

        // Create new stdout read handle and the stdin write handle.
        // Set the inheritance properties to FALSE. Otherwise, the child
        // inherits the these handles; resulting in non-closeable
        // handles to the pipes being created.
        if (!::DuplicateHandle(
            ::GetCurrentProcess(),
            hStdoutReadTmp,
            ::GetCurrentProcess(),
            &m_hStdoutRead,
            0, FALSE,			// make it uninheritable.
            DUPLICATE_SAME_ACCESS))
            __leave;

        if (!::DuplicateHandle(
            ::GetCurrentProcess(),
            hStdinWriteTmp,
            ::GetCurrentProcess(),
            &m_hStdinWrite,
            0, FALSE,			// make it uninheritable.
            DUPLICATE_SAME_ACCESS))
            __leave;

        // Close inheritable copies of the handles we do not want to
        // be inherited.
        DestroyHandle(hStdoutReadTmp);
        DestroyHandle(hStdinWriteTmp);

        // launch the child process
        if (!LaunchChildRedir(pszCmdLine, pszStartDir,
            hStdoutWrite, hStdinRead, hStderrWrite))
            __leave;

        // Child is launched. Close the parents copy of those pipe
        // handles that only the child should have open.
        // Make sure that no handles to the write end of the stdout pipe
        // are maintained in this process or else the pipe will not
        // close when the child process exits and ReadFile will hang.
        DestroyHandle(hStdoutWrite);
        DestroyHandle(hStdinRead);
        DestroyHandle(hStderrWrite);

        // Launch a thread to receive output from the child process.
        m_hEvtStop = ::CreateEvent(NULL, TRUE, FALSE, NULL);

        m_hThread = (HANDLE)_beginthreadex( NULL, 0, &OutputThread, this, 0, &m_dwThreadId );
        if (!m_hThread)
            __leave;

        bOK = TRUE;
    }

    __finally
    {
        if (!bOK)
        {
            DWORD dwOsErr = ::GetLastError();
            char szMsg[40];
            sprintf_s(szMsg, "[CONSOLE REDIRECTION] Redirect console error: %x\r\n", dwOsErr);
            tcerr << szMsg;
            DestroyHandle(hStdoutReadTmp);
            DestroyHandle(hStdoutWrite);
            DestroyHandle(hStderrWrite);
            DestroyHandle(hStdinWriteTmp);
            DestroyHandle(hStdinRead);
            Close();
            ::SetLastError(dwOsErr);
        }
    }

    return bOK;
}

void CRedirector::Close()
{
    if (m_hThread != NULL) {
        // this function might be called from redir thread
        if (::GetCurrentThreadId() != m_dwThreadId) {
            _ASSERT(m_hEvtStop != NULL);
            ::SetEvent(m_hEvtStop);
            //The process may be terminated (causing CRedirector::Close() to be called) but
            //the thread that is responsible of processing the stdoutput might not have finished 
            //to proceed. 
            // We cannot kill it because this thread needs to release a mutex when it finishes.
            // So we just wait for it to finish...
            ::WaitForSingleObject(m_hThread, INFINITE);

            /*if (::WaitForSingleObject(m_hThread, 5000) == WAIT_TIMEOUT) {
                ::TerminateThread(m_hThread, -2);
                tcerr << _T("[CONSOLE REDIRECTION] The standard output filter has been interrupted\r\n");
            }*/
        }

        DestroyHandle(m_hThread);
    }

    DestroyHandle(m_hEvtStop);
    DestroyHandle(m_hChildProcess);
    DestroyHandle(m_hStdinWrite);
    DestroyHandle(m_hStdoutRead);
    m_dwThreadId = 0;
}

// write data to the child's stdin
BOOL CRedirector::Printf(LPCTSTR pszFormat, size_t cMaxsize, ...)
{
	if (!m_hStdinWrite)
		return FALSE;

    PTSTR pszInput = new TCHAR[cMaxsize];
	va_list argList;

	va_start(argList, pszFormat);
	DWORD size = _vstprintf_s(pszInput, cMaxsize, pszFormat, argList);
	va_end(argList);

	DWORD dwWritten;
    BOOL bRet = WriteFile(m_hStdinWrite, pszInput, size, &dwWritten, NULL);
    delete pszInput;

    return bRet;
}

// Launch a child without redirection
BOOL CRedirector::LaunchChildNoRedir(LPTSTR pszCmdLine, LPCTSTR pszStartDir)
{
    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    ZeroMemory( &si, sizeof(si) );
    si.cb = sizeof(si);
    //si.lpTitle = "latex";
    //si.wShowWindow = SW_HIDE;
    //si.dwFlags = STARTF_USESHOWWINDOW;
    ZeroMemory( &pi, sizeof(pi) );

    // Start the child process. 
    if( !CreateProcess( NULL,   // No module name (use command line)
        pszCmdLine,     // Command line
        NULL,           // Process handle not inheritable
        NULL,           // Thread handle not inheritable
        FALSE,          // Set handle inheritance to FALSE
        0,//CREATE_NEW_CONSOLE,              // No creation flags
        NULL,           // Use parent's environment block
        pszStartDir,    // starting directory 
        &si,            // Pointer to STARTUPINFO structure
        &pi )           // Pointer to PROCESS_INFORMATION structure
    ) 
        return FALSE;

    m_hChildProcess = pi.hProcess;
    // We don't need the process thread handle. 
    CloseHandle( pi.hThread );

    return TRUE;
}


BOOL CRedirector::LaunchChildRedir(LPTSTR pszCmdLine,
                              LPCTSTR pszStartDir,
                              HANDLE hStdOut,
                              HANDLE hStdIn,
                              HANDLE hStdErr)
{
    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    _ASSERT(pszCmdLine);
    _ASSERT(m_hChildProcess == NULL);

    // Set up the start up info struct.
    ::ZeroMemory(&si, sizeof(STARTUPINFO));
    si.cb = sizeof(STARTUPINFO);
    si.hStdOutput = hStdOut;
    si.hStdInput = hStdIn;
    si.hStdError = hStdErr;
    //si.wShowWindow = SW_HIDE;
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;

    // Note that dwFlags must include STARTF_USESHOWWINDOW if we
    // use the wShowWindow flags. This also assumes that the
    // CreateProcess() call will use CREATE_NEW_CONSOLE.

    // Launch the child process.
    if (!::CreateProcess(
        NULL,
        pszCmdLine,
        NULL, NULL,
        TRUE,
        0,//CREATE_NEW_CONSOLE,
        NULL,
        pszStartDir,
        &si,
        &pi))
        return FALSE;

    m_hChildProcess = pi.hProcess;
    // Close any unuseful handles
    ::CloseHandle(pi.hThread);
    return TRUE;
}

// redirect the child process's stdout:
// return: 1: no more data, 0: child terminated, -1: os error
int CRedirector::RedirectStdout()
{
    char szOutput[BUFF_SIZE];
    _ASSERT(m_hStdoutRead != NULL);
    for (;;) {
        DWORD dwAvail = 0;
        if (!::PeekNamedPipe(m_hStdoutRead, NULL, 0, NULL,
            &dwAvail, NULL))			// error
            break;

        if (!dwAvail)					// not data available
            return 1;

        DWORD dwRead = 0;
        DWORD dwToRead = min(sizeof(szOutput)-1, dwAvail);
        if (!::ReadFile(m_hStdoutRead, szOutput, dwToRead,
            &dwRead, NULL) || !dwRead)	// error, the child might have ended
            break;

        szOutput[dwRead] = 0;
        m_predirout->rdbuf()->sputn(szOutput,dwRead);
    }

    DWORD dwError = ::GetLastError();
    if (dwError == ERROR_BROKEN_PIPE ||	// pipe has been ended
        dwError == ERROR_NO_DATA)		// pipe closing in progress
    {
#ifdef _TEST_REDIR
        *m_predirout << "\r\n[CONSOLE REDIRECTION]: Child process ended\r\n";
#endif
        return 0;	// child process ended
    }

    tcerr << "[CONSOLE REDIRECTION] Read stdout pipe error\r\n";
    return -1;		// os error
}

void CRedirector::DestroyHandle(HANDLE& rhObject)
{
    if (rhObject != NULL) {
        ::CloseHandle(rhObject);
        rhObject = NULL;
    }
}


// thread to receive output of the child process
unsigned __stdcall CRedirector::OutputThread(void *pvThreadParam)
{
    HANDLE aHandles[2];
    int nRet;
    CRedirector* pRedir = (CRedirector*) pvThreadParam;

    _ASSERT(pRedir != NULL);
    aHandles[0] = pRedir->m_hChildProcess;
    aHandles[1] = pRedir->m_hEvtStop;

    EnterCriticalSection(pRedir->m_pcs);
        for (;;) {
            // redirect stdout till there's no more data.
            nRet = pRedir->RedirectStdout();
            if (nRet <= 0)
                break;

            // check if the child process has terminated.
            DWORD dwRc = ::WaitForMultipleObjects(
                2, aHandles, FALSE, pRedir->m_dwWaitTime);
            if (WAIT_OBJECT_0 == dwRc)		// the child process ended
            {
                nRet = pRedir->RedirectStdout();
                if (nRet > 0)
                    nRet = 0;
                break;
            }
            if (WAIT_OBJECT_0+1 == dwRc)	// m_hEvtStop was signalled
            {
                nRet = 1;	// cancelled
                break;
            }
/*            else
            {
                _ASSERT(0);
                break;
            }*/
        }
    LeaveCriticalSection(pRedir->m_pcs);

    // close handles
    //pRedir->Close(); // Commented out by WB (it causes problems)
  _endthreadex( nRet );
    return nRet;
}
