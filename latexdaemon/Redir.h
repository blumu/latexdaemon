//////////////////////////////////////////////////////////////////////
//
// Redirector - to redirect the input / output of a console
//
// Developer: Jeff Lee
// Dec 10, 2001
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_REDIR_H__4FB57DC3_29A3_11D5_BB60_006097553C52__INCLUDED_)
#define AFX_REDIR_H__4FB57DC3_29A3_11D5_BB60_006097553C52__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class CRedirector 
{
public:
    // modif by wb:
    //  - redirout is a pointer to a stream responsible of printing the output
    // - pcs is a pointer to a critical section that controls the console output. It will be entered
    // before printing and left when the output thread finishes.
    CRedirector(std::ostream *redirout, CRITICAL_SECTION *pcs);
	virtual ~CRedirector();

private:
	HANDLE m_hThread;		// thread to receive the output of the child process
	HANDLE m_hEvtStop;		// event to notify the redir thread to exit
	DWORD m_dwThreadId;		// id of the redir thread
	DWORD m_dwWaitTime;		// wait time to check the status of the child process
    CRITICAL_SECTION *m_pcs;


protected:
	HANDLE m_hStdinWrite;	// write end of child's stdin pipe
	HANDLE m_hStdoutRead;	// read end of child's stdout pipe
	HANDLE m_hChildProcess;
    HANDLE m_hChildProcessThread;

    std::ostream *m_predirout;


    BOOL LaunchChildNoRedir(LPTSTR pszCmdLine);
    BOOL LaunchChildRedir(LPTSTR pszCmdLine, HANDLE hStdOut, HANDLE hStdIn, HANDLE hStdErr);
	int RedirectStdout();
	void DestroyHandle(HANDLE& rhObject);

	static DWORD WINAPI OutputThread(LPVOID lpvThreadParam);

public:
	BOOL Open(LPTSTR pszCmdLine);
	virtual void Close();
	BOOL Printf(LPCTSTR pszFormat, size_t cMaxsize, ...);
    HANDLE GetProcessHandle() {
        return m_hChildProcess;
    }
	void SetWaitTime(DWORD dwWaitTime) { m_dwWaitTime = dwWaitTime; }
};

#endif // !defined(AFX_REDIR_H__4FB57DC3_29A3_11D5_BB60_006097553C52__INCLUDED_)
