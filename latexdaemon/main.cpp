// Copyright William Blum 2007 (http://william.famille-blum.org/software/index.html)
// Created in September 2006
#define APP_NAME		"LatexDaemon"
#define VERSION_DATE	"29 August 2007"
#define VERSION			0.9
#define BUILD			"10"

// See changelog.html for the list of changes:.

// TODO:
//  At the moment, messages reporting that some watched file has been modified are not shown while the "make" 
//  thread is running. This is done in order to avoid printf interleaving. A solution would 
//  be to delay the printing of these messages until the end of the execution of the make "thread".
//  Another solution is to implement a separate thread responsible of the output of all other threads.

// Acknowledgment:
// - The MD5 class is a modification of CodeGuru's one: http://www.codeguru.com/Cpp/Cpp/algorithms/article.php/c5087
// - Command line processing routine from The Code Project: http://www.codeproject.com/useritems/SimpleOpt.asp
// - Console color header file (Console.h) from: http://www.codeproject.com/cpp/AddColorConsole.asp
// - Function CommandLineToArgvA and CommandLineToArgvW from http://alter.org.ua/en/docs/win/args/

#define _WIN32_WINNT  0x0400
#define _CRT_SECURE_NO_DEPRECATE
#include <windows.h>
#include <Winbase.h>
#include <conio.h>
#include <string>
#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <tchar.h>
#include <direct.h>
#include <sys/stat.h>
#include <time.h>
#include "md5.h"
#include "console.h"
#include "SimpleOpt.h"
#include "SimpleGlob.h"
#include "CommandLineToArgv.h"
using namespace std;


//////////
/// Constants 

// console colors used
#define fgMsg			JadedHoboConsole::fg_green
#define fgErr			JadedHoboConsole::fg_red
#define fgWarning		JadedHoboConsole::fg_yellow
#define fgLatex			JadedHoboConsole::fg_lowhite
#define fgIgnoredfile	JadedHoboConsole::fg_gray
#define fgNormal		JadedHoboConsole::fg_lowhite
#define fgDepFile		JadedHoboConsole::fg_cyan
#define fgPrompt		JadedHoboConsole::fg_locyan

#define DEFAULTPREAMBLE2_BASENAME       "preamble"
#define DEFAULTPREAMBLE2_FILENAME       DEFAULTPREAMBLE2_BASENAME ".tex"
#define DEFAULTPREAMBLE1_EXT            "pre"

#define PROMPT_STRING					"dameon>"
#define PROMPT_STRING_WATCH				"dameon@>"


// Maximal length of an input command line at the prompt
#define PROMPT_MAX_INPUT_LENGTH			2*_MAX_PATH


// result of timestamp comparison
#define OUT_FRESHER	   0x00
#define SRC_FRESHER	   0x01
#define ERR_OUTABSENT  0x02
#define ERR_SRCABSENT  0x04

// constants corresponding to the different possible jobs that can be exectuted
enum JOB { Rest = 0 , Dvips = 1, Compile = 2, FullCompile = 3 } ;


//////////
/// Prototypes
int compare_timestamp(LPCTSTR sourcefile, LPCTSTR outputfile);
DWORD launch_and_wait(LPCTSTR cmdline);
void createWatchingThread();
void WINAPI CommandPromptThread( void *param );
void WINAPI WatchingThread( void *param );
void WINAPI MakeThread( void *param );

BOOL CALLBACK LookForGsviewWindow(HWND hwnd, LPARAM lparam);

// TODO: move the following function to a proper class. After all this is a C++ file!
int loadfile( CSimpleGlob *pnglob, JOB initialjob );
bool RestartMakeThread(JOB makejob);
int fullcompile();
int compile();
int dvips();
int ps2pdf();
int bibtex();
int edit();
int view();
int view_dvi();
int view_ps();
int view_pdf();
int openfolder();


//////////
/// Global variables

// critical section for handling printf across the different threads
CRITICAL_SECTION cs;

// set to true by default, can be overwritten by a command line switch
bool LookForExternalPreamble = true;

// is there an external preamble file for the currently openend .tex file?
bool ExternalPreamblePresent = true;

// Set to true when an external command is running (function launch_and_wait())
bool ExecutingExternalCommand = false;

// watch for file changes ?
bool Watch = true;

// what to do after compilation? (can be change using the -afterjob command argument)
JOB afterjob = Rest;

// handle of the "make" thread
HANDLE hMakeThread = NULL;

// handle of the watching thread
HANDLE hWatchingThread = NULL;

// Event fired when the make thread needs to be aborted
HANDLE hEvtAbortMake = NULL;

// Fire this event to request the watching thread to abort
HANDLE hEvtStopWatching = NULL;

// Reg key where to find the path to gswin32
#define REG_KEY_GWIN32PATH _T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\gsview32.exe")

// path to gsview32.exe
string gsview32 = "c:\\program files\\ghostgum\\gsview\\gsview32.exe";

// by default, gswin32 is not used as a viewer: the program associated with the output file will be used (adobe reader, gsview, ...)
bool UseGswin32 = false;

// process/thread id of the last launched gsview32.exe
PROCESS_INFORMATION piGsview32 = {NULL, NULL};
// handle of the main window of gsview32
HWND hwndGsview32 = NULL;

// Tex initialization file (can be specified as a command line parameter)
string texinifile = "latex"; // use latex by default

// preamble file name and basename
string preamble_filename = "";
string preamble_basename = "";

// Path where the main tex file resides
string texdir = "";
string texfullpath = "";
string texbasename = "";

// name of the backup file for the .aux file
string auxbackupname = "";


// This glob object contains the list of dependencies
CSimpleGlob *pglob = NULL;

// by default the output extension is set to ".dvi", it must be changed to ".pdf" if pdflatex is used
string output_ext = ".dvi";

// type of the parameter passed to the "make" thread
typedef struct {
	JOB			makejob;
} MAKETHREADPARAM  ;

// type of the parameter passed to the "watching" thread
typedef struct {
	//CSimpleGlob  *glob;
} WATCHTHREADPARAM  ;


// define the ID values to indentify the option
enum { 
	// command line options
	OPT_USAGE, OPT_INI, OPT_WATCH, OPT_FORCE, OPT_PREAMBLE, OPT_AFTERJOB, 
	// prompt commands
	OPT_HELP, OPT_COMPILE, OPT_FULLCOMPILE, OPT_QUIT, OPT_BIBTEX, OPT_DVIPS, 
	OPT_PS2PDF, OPT_EDIT, OPT_VIEWOUTPUT, OPT_OPENFOLDER, OPT_LOAD, 
	OPT_VIEWDVI, OPT_VIEWPS, OPT_VIEWPDF, OPT_GSVIEW
};

// declare a table of CSimpleOpt::SOption structures. See the SimpleOpt.h header
// for details of each entry in this structure. In summary they are:
//  1. ID for this option. This will be returned from OptionId() during processing.
//     It may be anything >= 0 and may contain duplicates.
//  2. Option as it should be written on the command line
//  3. Type of the option. See the header file for details of all possible types.
//     The SO_REQ_SEP type means an argument is required and must be supplied
//     separately, e.g. "-f FILE"
//  4. The last entry must be SO_END_OF_OPTIONS.
//

// command line argument options
CSimpleOpt::SOption g_rgOptions[] = {
    { OPT_USAGE,		_T("-?"),			SO_NONE    },
    { OPT_USAGE,		_T("--help"),		SO_NONE    },
    { OPT_USAGE,		_T("-help"),		SO_NONE    },
    { OPT_INI,			_T("-ini"),			SO_REQ_CMB },
    { OPT_INI,			_T("--ini"),		SO_REQ_CMB },
    { OPT_PREAMBLE,		_T("-preamble"),	SO_REQ_CMB },
    { OPT_PREAMBLE,		_T("--preamble"),	SO_REQ_CMB },
    { OPT_AFTERJOB,		_T("-afterjob"),	SO_REQ_CMB },
    { OPT_AFTERJOB,		_T("--afterjob"),	SO_REQ_CMB },
	{ OPT_WATCH,		_T("-watch"),		SO_REQ_CMB },
    { OPT_WATCH,		_T("--watch"),		SO_REQ_CMB },
    { OPT_FORCE,		_T("-force"),		SO_REQ_SEP },
    { OPT_FORCE,		_T("--force"),		SO_REQ_SEP },
    { OPT_GSVIEW,		_T("--gsview"),		SO_NONE },
    { OPT_GSVIEW,		_T("-gsview"),		SO_NONE },	
    SO_END_OF_OPTIONS                   // END
};

// prompt commands options
CSimpleOpt::SOption g_rgPromptOptions[] = {
    { OPT_HELP,			_T("-?"),			SO_NONE		},
    { OPT_HELP,			_T("-h"),			SO_NONE		},
    { OPT_HELP,			_T("-help"),		SO_NONE		},
    { OPT_USAGE,		_T("-u"),			SO_NONE		},
    { OPT_USAGE,		_T("-usage"),		SO_NONE		},
    { OPT_BIBTEX,		_T("-b"),			SO_NONE		},
    { OPT_BIBTEX,		_T("-bibtex"),		SO_NONE		},
    { OPT_DVIPS,		_T("-d"),			SO_NONE		},
    { OPT_DVIPS,		_T("-dvips"),		SO_NONE		},
    { OPT_PS2PDF,		_T("-p"),			SO_NONE		},
    { OPT_PS2PDF,		_T("-ps2pdf"),		SO_NONE		},
    { OPT_COMPILE,		_T("-c"),			SO_NONE		},
    { OPT_COMPILE,		_T("-compile"),		SO_NONE		},
    { OPT_FULLCOMPILE,	_T("-f"),			SO_NONE		},
    { OPT_FULLCOMPILE,	_T("-fullcompile"),	SO_NONE		},
	{ OPT_WATCH,		_T("-watch"),		SO_REQ_CMB	},
    { OPT_INI,			_T("-ini"),			SO_REQ_CMB  },
    { OPT_PREAMBLE,		_T("-preamble"),	SO_REQ_CMB  },
    { OPT_AFTERJOB,		_T("-afterjob"),	SO_REQ_CMB  },
	{ OPT_QUIT,			_T("-q"),			SO_NONE		},
	{ OPT_QUIT,			_T("-quit"),		SO_NONE		},
	{ OPT_EDIT,			_T("-e"),			SO_NONE		},
	{ OPT_EDIT,			_T("-edit"),		SO_NONE		},
	{ OPT_VIEWOUTPUT,	_T("-v"),			SO_NONE		},
	{ OPT_VIEWOUTPUT,	_T("-view"),		SO_NONE		},
	{ OPT_VIEWDVI,		_T("-vd"),			SO_NONE		},
	{ OPT_VIEWDVI,		_T("-viewdvi"),		SO_NONE		},
	{ OPT_VIEWPS,		_T("-vs"),			SO_NONE		},
	{ OPT_VIEWPS,		_T("-viewps"),		SO_NONE		},
	{ OPT_VIEWPDF,		_T("-vf"),			SO_NONE		},
	{ OPT_VIEWPDF,		_T("-viewpdf"),		SO_NONE		},
	{ OPT_OPENFOLDER,	_T("-o"),			SO_NONE		},
	{ OPT_OPENFOLDER,	_T("-open"),		SO_NONE		},
	{ OPT_LOAD,			_T("-l"),			SO_NONE		},
	{ OPT_LOAD,			_T("-load"),		SO_NONE		},
    SO_END_OF_OPTIONS                   // END
};



////////////////////



// show the usage of this program
void ShowUsage(TCHAR *progname) {
	cout << "USAGE: latexdameon [options] mainfile.tex [dependencies]" <<endl
		 << "where" << endl
		 << "* options can be:" << endl
		 << " --help" << endl 
		 << "   Show this help message." <<endl<<endl
		 << " --watch={yes|no}" << endl 
		 << "   If set to yes then it will run the necessary compilation and then exit without watching for file changes."<<endl<<endl
		 << " --force {compile|fullcompile}" << endl
		 << "   . 'compile' forces the compilation of the .tex file at the start even when no modification is detected." << endl<<endl
		 << "   . 'fullcompile' forces the compilation of the preamble and the .tex file at the start even when no modification is detected." <<endl<<endl
		 << " --gsview " << endl 
		 << "   Use Ghostview as a .pdf and .ps viewer (with autorefresh)" << endl << endl
		 << " --ini=inifile" << endl 
		 << "   Set 'inifile' as the initialization format file that will be used to compile the preamble." <<endl<<endl
		 << " --afterjob={dvips|rest}" << endl 
		 << "   . 'dvips' specifies that dvips should be run after a successful compilation of the .tex file," <<endl
		 << "   . 'rest' (default) specifies that nothing needs to be done after compilation."<<endl
		 << " --preamble={none|external}" << endl 
		 << "   . 'none' specifies that the main .tex file does not use an external preamble file."<<endl
		 << "   The current version is not capable of extracting the preamble from the .tex file, therefore if this switch is used, the precompilation feature will be automatically desactivated."<<endl
		 << "   . 'external' (default) specifies that the preamble is stored in an external file. The daemon first look for a preamble file called mainfile.pre, if this does not exist it tries preamble.tex and eventually, if neither exists, falls back to the 'none' option."<<endl
		 << endl << "   If the files preamble.tex and mainfile.pre exist but do not correspond to the preamble of your latex document (i.e. not included with \\input{mainfile.pre} at the beginning of your .tex file) then you must set the 'none' option to avoid the precompilation of a wrong preamble." <<endl<<endl
		 << "* dependencies contains a list of files that your main tex file relies on. You can sepcify list of files using jokers, for example '*.tex *.sty'. However, only the dependencies that resides in the same folder as the main tex file will be watched for changes." <<endl<<endl
	     << "INSTRUCTIONS:" << endl
		 << "Suppose main.tex is the main file of your Latex document then:" << endl
	     << "  1. move the preamble from main.tex to a new file named mainfile.pre" << endl 
	     << "  2. insert '\\input{mainfile.pre}' at the beginning of your mainfile.tex file" << endl 
	     << "  3. start the daemon with the command \"latexdaemon main.tex *.tex\" " << 
		    "(if you use pdflatex then add the option \"-ini=pdflatex\")" << endl << endl;
}

// update the title of the console
void SetTitle(string state)
{
	if( texfullpath != "" )
		SetConsoleTitle((state + " - "+ texinifile + "Daemon - " + texfullpath).c_str());
	else
		SetConsoleTitle((state + " - " + texinifile + "Daemon").c_str());
}

// Code executed when the -ini option is specified
void ExecuteOptionIni( string optionarg )
{
	texinifile = optionarg;
	SetTitle("monitoring");
	cout << "-Intiatialization file set to \"" << texinifile << "\"" << endl;;
	if( (texinifile == "pdflatex") || (texinifile == "pdftex") )
		output_ext = ".pdf";
	else if ( (texinifile == "latex") || (texinifile == "tex") )
		output_ext = ".dvi";
}
// Code executed when the -preamble option is specified
void ExecuteOptionPreamble( string optionarg )
{
	EnterCriticalSection( &cs );
	LookForExternalPreamble = (optionarg=="none") ? false : true ;
	if( LookForExternalPreamble )
		cout << "-I will look for an external preamble file next time I load a .tex file." << endl;
	else
		cout << "-I will not look for an external preamble next time I load a .tex file." << endl;
	LeaveCriticalSection( &cs );
}

// Code executed when the -force option is specified
void ExecuteOptionForce( string optionarg, JOB &force )
{
	EnterCriticalSection( &cs );
	if( optionarg=="fullcompile" ) {
		force = FullCompile;
		cout << "-Initial full compilation forced." << endl;
	}
	else {
		force = Compile;
		cout << "-Initial compilation forced." << endl;
	}
	LeaveCriticalSection( &cs );
}

// code for the -gsview option
void ExecuteOptionGsview()
{
	UseGswin32 = true;
	EnterCriticalSection( &cs );
	cout << "-Viewer set to GhostView." << endl;
	LeaveCriticalSection( &cs );
}

// Code executed when the -afterjob option is specified
void ExecuteOptionAfterJob( string optionarg )
{
	EnterCriticalSection( &cs );
	if( optionarg=="dvips" ) {
		afterjob = Dvips;
		cout << "-After-compilation job set to '" << optionarg << "'" << endl;
	}
	else {
		afterjob = Rest;
		cout << "-After-compilation job set to 'rest'" << endl;
	}
	LeaveCriticalSection(&cs);
}

// Code executed when the -watch option is specified
void ExecuteOptionWatch( string optionarg )
{
	Watch = optionarg != "no";
	BOOL b = Watch;
	if( Watch ) {
		EnterCriticalSection( &cs );
		cout << "-Watch for file modifications activated" << endl;
		LeaveCriticalSection( &cs );
		if( pglob && !hWatchingThread)
			createWatchingThread();
	}
	else {
		EnterCriticalSection( &cs );
		cout << "-Watch for file modification desactivated" << endl;
		LeaveCriticalSection( &cs );
		if( hWatchingThread ) {
			// Stop the watching thread
			SetEvent(hEvtStopWatching);
			WaitForSingleObject(hWatchingThread, INFINITE);
		}
	}
}

// perform the necessary compilation
int make(JOB makejob)
{
	int ret;

	if( makejob == Rest )
		return 0;
	else if( makejob == Compile ) {
		SetTitle("recompiling...");
		ret = compile();
	}
	else if ( makejob == FullCompile ) {
		SetTitle("recompiling...");
		ret = fullcompile();
	}
	if( ret==0 ) {
		if( afterjob == Dvips )
			ret = dvips();
		
		// has gswin32 been launched?
		if( piGsview32.dwProcessId ) {
			// look for the gswin32 window
			if( !hwndGsview32 )
				EnumThreadWindows(piGsview32.dwThreadId, LookForGsviewWindow, NULL);
			// refresh the gsview32 window 
			if( hwndGsview32 )
				PostMessage(hwndGsview32, WM_KEYDOWN, VK_F5, 0xC03F0001);
		}
	}

	SetTitle( ret ? "(errors) monitoring" : "monitoring" );
	return ret;
}

// thread responsible of launching the external commands (latex) for compilation
void WINAPI MakeThread( void *param )
{
	// prepare the abort event so that other thread can stop this one
	ResetEvent(hEvtAbortMake);

	//////////////
	// perform the necessary compilations
	MAKETHREADPARAM *p = (MAKETHREADPARAM *)param;

	if( p->makejob == Compile ) {
		// Make a copy of the .aux file (in case the latex compilation aborts and destroy the .aux file)
		CopyFile((texdir+texbasename+".aux").c_str(), (texdir+auxbackupname).c_str(), FALSE);
	}

	int errcode = make(p->makejob);

	if( p->makejob == Compile && errcode == -1) {
		// Restore the backup of the .aux file
		CopyFile((texdir+auxbackupname).c_str(), (texdir+texbasename+".aux").c_str(), FALSE);

	}
	// restore the prompt		
	EnterCriticalSection( &cs );
	cout << fgPrompt << (hWatchingThread ? PROMPT_STRING_WATCH : PROMPT_STRING);
	LeaveCriticalSection( &cs );

	free(p);
	hMakeThread = NULL;
}

// 
void createWatchingThread(){
	EnterCriticalSection(&cs);
	cout << fgMsg << "\n-- Watching directory " << texdir << " for changes...\n" << fgNormal;
	LeaveCriticalSection( &cs ); 
	SetTitle("monitoring");

	DWORD watchingthreadID;
	hWatchingThread = CreateThread( NULL, 0,
		(LPTHREAD_START_ROUTINE) WatchingThread,
		0,
		0,
		&watchingthreadID);
}

BOOL CALLBACK FindConsoleEnumWndProc(HWND hwnd, LPARAM lparam)
{
	 DWORD dwProcessId = 0;
	 GetWindowThreadProcessId(hwnd, &dwProcessId);
	 if (dwProcessId == GetCurrentProcessId()) {
		 *((HWND *)lparam) = hwnd;
		 return 0;
	 } else {
		 return 1;
	 }
}

HWND FindMyWindow(void)
{
	HWND hwndConsole = 0;
	EnumWindows(FindConsoleEnumWndProc, (LPARAM)&hwndConsole);
	return hwndConsole;
}


// thread responsible for parsing the commands send by the user.
void WINAPI CommandPromptThread( void *param )
{
	//////////////
	// perform the necessary compilations

	//////////////
	// input loop
	bool wantsmore = true, printprompt=true;
	while(wantsmore)
	{
		if(hMakeThread) {
			// wait for the "make" thread to end
			WaitForSingleObject(hMakeThread, INFINITE);
		}

		if( printprompt ) {
			EnterCriticalSection( &cs );
			cout << fgPrompt << (hWatchingThread ? PROMPT_STRING_WATCH : PROMPT_STRING);
			LeaveCriticalSection( &cs );
		}
		printprompt = true;

		// Read a command from the user
		TCHAR cmdline[PROMPT_MAX_INPUT_LENGTH+5] = _T("cmd -"); // add a dummy command name and the option delimiter
		cin.getline(&cmdline[5], PROMPT_MAX_INPUT_LENGTH);

		// Convert the command line into an argv table 
		int argc;
		LPTSTR *argvw;
		argvw = CommandLineToArgv(cmdline, &argc);

		// Parse the command line
		CSimpleOpt args(argc, argvw, g_rgPromptOptions, true);
		unsigned int uiFlags = 0;

		args.Next(); // get the first command recognized
		if (args.LastError() != SO_SUCCESS) {
			TCHAR *pszError = _T("Unknown error");
			switch (args.LastError()) {
			case SO_OPT_INVALID:
				pszError = _T("Unrecognized command");
				break;
			case SO_OPT_MULTIPLE:
				pszError = _T("This command takes multiple arguments");
				break;
			case SO_ARG_INVALID:
				pszError = _T("This command does not accept argument");
				break;
			case SO_ARG_INVALID_TYPE:
				pszError = _T("Invalid argument format");
				break;
			case SO_ARG_MISSING:
				pszError = _T("Required argument is missing");
				break;
			}
			EnterCriticalSection( &cs );
				LPCTSTR optiontext = args.OptionText();
				// remove the extra '-' that we appened before the command name
				if( args.LastError()== SO_OPT_INVALID && optiontext[0] == '-') 
					optiontext++;
				// don't show error message for the empty command
				if( !(args.LastError()== SO_OPT_INVALID && optiontext[0] == 0) ) {
					cout << fgErr << pszError << ": '" << optiontext << "' (use help to get command line help)"  << endl;
				}
			LeaveCriticalSection( &cs );
			continue;
		}

		uiFlags |= (unsigned int) args.OptionId();
		switch( args.OptionId() ) {
		case OPT_USAGE:
			EnterCriticalSection( &cs );
				ShowUsage(NULL);
			LeaveCriticalSection( &cs ); 
			break;
		case OPT_HELP:
			EnterCriticalSection( &cs );
			cout << fgPrompt << "The following commands are available:" << endl
				 << "  b[ibtex]        to run bibtex on the .tex file" << endl
				 << "  c[compile]      to compile the .tex file using the precompiled preamble" << endl
				 << "  d[vips]         to convert the .dvi file to postscript" << endl
				 << "  e[dit]          to edit the .tex file" << endl
				 << "  f[ullcompile]   to compile the preamble and the .tex file" << endl
				 << "  h[elp]          to show this message" << endl
				 << "  l[oad] file.tex to change the active .tex file" << endl
				 << "  o[pen]          to open the folder containing the .tex file" << endl
				 << "  p[s2pdf]        to convert the .ps file to pdf" << endl
				 << "  q[uit]          to quit the program" << endl 
				 << "  u[sage]         to show the help on command line parameters usage" << endl
				 << "  v[iew]          to view the output file (dvi or pdf depending on ini value)" << endl				 << "  vd|viewdvi      to view the pdf file output" << endl 
				 << "  vs|viewps      to view the pdf file output" << endl 
				 << "  vf|viewpdf      to view the pdf file output" << endl << endl
				 << "You can also configure variables with:" << endl
				 << "  ini=inifile               set the initial format file to inifile" << endl
				 << "  preamble={none,external}  set the preamble mode for the file to be loaded" << endl
				 << "  afterjob={rest,dvips}     set the job executed after latex compilation" << endl
				 << "  watch={yes,no}            to activate/desactive file modification watching" << endl << endl;
			LeaveCriticalSection( &cs ); 
			break;
		case OPT_INI:			ExecuteOptionIni(args.OptionArg());			break;
		case OPT_PREAMBLE:		ExecuteOptionPreamble(args.OptionArg());	break;
		case OPT_AFTERJOB:		ExecuteOptionAfterJob(args.OptionArg());	break;
		case OPT_WATCH:			ExecuteOptionWatch(args.OptionArg());		break;
		case OPT_BIBTEX:		bibtex();		break;
		case OPT_DVIPS:			dvips();		break;
		case OPT_PS2PDF:		ps2pdf();		break;
		case OPT_EDIT:			edit();			break;
		case OPT_VIEWOUTPUT:	view();			break;
		case OPT_VIEWDVI:		view_dvi();		break;
		case OPT_VIEWPS:		view_ps();		break;
		case OPT_VIEWPDF:		view_pdf();		break;
		case OPT_OPENFOLDER:	openfolder();	break;		

		case OPT_LOAD:
			// Read the remaining parameters
			while (args.Next())
				uiFlags |= (unsigned int) args.OptionId();

			{
				// parse the parameters (tex file name and dependencies)
				CSimpleGlob *npglob = new CSimpleGlob(uiFlags);
				if (SG_SUCCESS != npglob->Add(args.FileCount(), args.Files()) ) {
					cout << fgErr << _T("Error while globbing files! Make sure that the given path is correct.\n" << fgNormal) ;
				}				
				else {
					// if no argument is specified then we show a dialogbox to the user to let him select a file
					if( args.FileCount() == 0 ){
						OPENFILENAME ofn;       // common dialog box structure
						char szFile[260];       // buffer for file name

						// Initialize OPENFILENAME
						ZeroMemory(&ofn, sizeof(ofn));
						ofn.lStructSize = sizeof(ofn);
						ofn.hwndOwner = FindMyWindow();
						ofn.lpstrFile = szFile;
						//
						// Set lpstrFile[0] to '\0' so that GetOpenFileName does not 
						// use the contents of szFile to initialize itself.
						//
						ofn.lpstrFile[0] = '\0';
						ofn.nMaxFile = sizeof(szFile);
						ofn.lpstrFilter = "All(*.*)\0*.*\0Tex/Latex file (*.tex)\0*.tex\0";
						ofn.nFilterIndex = 2;
						ofn.lpstrFileTitle = NULL;
						ofn.nMaxFileTitle = 0;
						ofn.lpstrInitialDir = NULL;
						ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

						// Display the Open dialog box. 

						if (GetOpenFileName(&ofn)==TRUE) {
							if (SG_SUCCESS != npglob->Add(szFile) ) {
								cout << fgErr << _T("Error while globbing files! Make sure that the given path is correct.\n" << fgNormal) ;
								goto err_load;
							}
						}
						else {
							cout << fgErr << _T("No input file specified!\n" << fgNormal);
							goto err_load;
						}

					}

					// Load the file
					if( loadfile(npglob, Rest) == 0 ) {
						// Restart the watching thread
						if( Watch ) {
							// stop it first if already started
							if( hWatchingThread ) {
								SetEvent(hEvtStopWatching);
								WaitForSingleObject(hWatchingThread, INFINITE);
							}
							createWatchingThread();
						}
					}
				}
			}
err_load:

			break;
		case OPT_COMPILE:
			{
				bool started = RestartMakeThread(Compile);
				// wait for the "make" thread to end
				if( started )
					WaitForSingleObject(hMakeThread, INFINITE);
				printprompt = !started;
			}
			break;
		case OPT_FULLCOMPILE:
			{
				bool started = RestartMakeThread(FullCompile);
				// wait for the "make" thread to end
				if( started )
					WaitForSingleObject(hMakeThread, INFINITE);
				printprompt = !started;
			}
			break;
		case OPT_QUIT:
			wantsmore = false;
			cout << fgNormal;
			break;		

		default:
			EnterCriticalSection( &cs );
			cout << fgErr << "Unrecognized command: '" << args.OptionText() << "' (use help to get command line help)" << endl ;
			LeaveCriticalSection( &cs );
			printprompt = true;
			break;
		}

	}

	// Preparing to exit the program: ask the children thread to terminate
	HANDLE hp[2];
	int i=0;
	if( hWatchingThread ) {
		// Stop the watching thread
		SetEvent(hEvtStopWatching);
		hp[i++] = hWatchingThread;
	}
	if( hMakeThread ) {
		// Stop the make thread
		SetEvent(hEvtAbortMake);
		hp[i++] = hMakeThread;
	}
	WaitForMultipleObjects(i, hp, TRUE, INFINITE);

}

int _tmain(int argc, TCHAR *argv[])
{
	SetTitle("prompt");

	cout << endl << APP_NAME << " " << VERSION << " Build " << BUILD << " by William Blum, " VERSION_DATE << endl << endl;;

	// look for gsview32
	HKEY hkey;
	if( ERROR_SUCCESS == RegOpenKeyEx(HKEY_LOCAL_MACHINE, REG_KEY_GWIN32PATH, 0, KEY_QUERY_VALUE, &hkey) ) {	
		TCHAR buff[_MAX_PATH];
		DWORD size = _MAX_PATH;
		RegQueryValueEx(hkey,NULL,NULL,NULL,(LPBYTE)buff,&size);
		RegCloseKey(hkey);
		gsview32 = buff;
	}

	// create the event used to abort the "make" thread
	hEvtAbortMake = CreateEvent(NULL,TRUE,FALSE,NULL);
	// create the event used to abort the "watching" thread
	hEvtStopWatching = CreateEvent(NULL,TRUE,FALSE,NULL);
	// initialize the critical section used to sequentialized console output
	InitializeCriticalSection(&cs);


	// default options, can be overwritten by command line parameters
	JOB initialjob = Rest;

	unsigned int uiFlags = 0;
    CSimpleOpt args(argc, argv, g_rgOptions, true);
    while (args.Next()) {
        if (args.LastError() != SO_SUCCESS) {
            TCHAR * pszError = _T("Unknown error");
            switch (args.LastError()) {
            case SO_OPT_INVALID:
                pszError = _T("Unrecognized option");
                break;
            case SO_OPT_MULTIPLE:
                pszError = _T("Option matched multiple strings");
                break;case SO_ARG_INVALID:
                pszError = _T("Option does not accept argument");
                break;
            case SO_ARG_INVALID_TYPE:
                pszError = _T("Invalid argument format");
                break;
            case SO_ARG_MISSING:
                pszError = _T("Required argument is missing");
                break;
            }
            _tprintf(
                _T("%s: '%s' (use --help to get command line help)\n"),
                pszError, args.OptionText());
            continue;
        }

		switch( args.OptionId() ) {
			case OPT_USAGE:         ShowUsage(NULL);							goto exit;
			case OPT_INI:			ExecuteOptionIni(args.OptionArg());			break;
			case OPT_WATCH:			ExecuteOptionWatch(args.OptionArg()); 		break;
			case OPT_FORCE:			ExecuteOptionForce(args.OptionArg(),initialjob); break;
			case OPT_GSVIEW:        ExecuteOptionGsview(); break;
			case OPT_PREAMBLE:		ExecuteOptionPreamble(args.OptionArg());	break;
			case OPT_AFTERJOB:		ExecuteOptionAfterJob(args.OptionArg());	break;
			default:				break;
		}
        uiFlags |= (unsigned int) args.OptionId();
    }


		int ret = -1;
		CSimpleGlob *nglob = new CSimpleGlob(uiFlags);
		if( args.FileCount() == 0 ){
			cout << fgWarning << _T("No input file specified.\n") << fgNormal;
		}
		else {
			if (SG_SUCCESS != nglob->Add(args.FileCount(), args.Files()) ) {
				cout << fgErr << _T("Error while globbing files! Make sure that the given path is correct.\n") << fgNormal ;
				goto exit;
			}
			ret = loadfile(nglob, initialjob);
		}

		if( Watch ) { // If watching has been requested by the user
			// if some correct file was given as a command line parameter 
			if( ret == 0 )				
				createWatchingThread(); // then start watching
			
			// start the prompt loop
			CommandPromptThread(NULL);
		}
	
	if(pglob)
		delete pglob;
exit:	
	DeleteCriticalSection(&cs);
	CloseHandle(hEvtAbortMake);
	CloseHandle(hEvtStopWatching);
	return ret;
}


// Load the .tex file specified in the first argument of args, the remainings arguements are
// the dependencies of the .tex file.
int loadfile( CSimpleGlob *pnewglob, JOB initialjob )
{
	cout << "-Main file: '" << pnewglob->File(0) << "'\n";

	TCHAR drive[4];
	TCHAR mainfile[_MAX_FNAME];
	TCHAR ext[_MAX_EXT];
	TCHAR fullpath[_MAX_PATH];
	TCHAR dir[_MAX_DIR];

	_fullpath( fullpath, pnewglob->File(0), _MAX_PATH );
	_tsplitpath( fullpath, drive, dir, mainfile, ext );

	// set the global variables
	texfullpath = fullpath;
	texdir = string(drive) + dir;
	texbasename = mainfile;
	auxbackupname = texbasename+".aux.bak";


	// set console title
	SetTitle("Initialization");

	cout << "-Directory: " << drive << dir << "\n";

	if(  _tcsncmp(ext, ".tex", 4) )	{
		cerr << fgErr << "Error: the file has not the .tex extension!\n\n" << fgNormal;
		delete pnewglob;
		return 1;
	}
	if( pnewglob->FileCount()>1 ) {
		cout << "-Dependencies:\n";
		for (int n = 1; n < pnewglob->FileCount(); ++n)
			_tprintf(_T("  %2d: '%s'\n"), n, pnewglob->File(n));
	}
	else
		cout << "-No dependency.\n";
		
	if(pnewglob->FileCount() == 0) {
		delete pnewglob;
		return 1;
	}

	// change current directory
	_chdir(texdir.c_str());

	int res; // will contain the result of the comparison of the timestamp of the preamble file with the format file

	// check for the presence of the external preamble file
	if( LookForExternalPreamble ) {
		// compare the timestamp of the preamble.tex file and the format file
		preamble_filename = string(mainfile) + "." DEFAULTPREAMBLE1_EXT;
		preamble_basename = string(mainfile);
		res = compare_timestamp(preamble_filename.c_str(), (preamble_basename+".fmt").c_str());
		ExternalPreamblePresent = !(res & ERR_SRCABSENT);
		if ( !ExternalPreamblePresent ) {
			// try with the second default preamble name
			preamble_filename = DEFAULTPREAMBLE2_FILENAME;
			preamble_basename = DEFAULTPREAMBLE2_BASENAME;
			res = compare_timestamp(preamble_filename.c_str(), (preamble_basename+".fmt").c_str());
			ExternalPreamblePresent = !(res & ERR_SRCABSENT);
			if ( !ExternalPreamblePresent ) {
				cout << fgWarning << "Warning: Preamble file not found! (I have tried " << mainfile << "." << DEFAULTPREAMBLE1_EXT << " and " << DEFAULTPREAMBLE2_FILENAME << ")\nPrecompilation mode desactivated!\n";
			}
		}
	}
	else
		ExternalPreamblePresent = false;

	if( ExternalPreamblePresent )
		cout << "-Preamble file: " << preamble_filename << "\n";

	if( initialjob != Rest ) {
		make(initialjob);
	}
	// Determine what needs to be recompiled based on the files that have been touched since last compilation.
	else {
		// The external preamble file is used and the format file does not exist or has a timestamp
		// older than the preamble file : then recreate the format file and recompile the .tex file.
		if( ExternalPreamblePresent && ((res == SRC_FRESHER) || (res & ERR_OUTABSENT)) ) {
			if( res == SRC_FRESHER ) {
				 cout << fgMsg << "+ " << preamble_filename << " has been modified since last run.\n";
				 cout << fgMsg << "  Let's recreate the format file and recompile " << texbasename << ".tex.\n";
			}
			else {		
				cout << fgMsg << "+ " << preamble_basename << ".fmt does not exist. Let's create it...\n";
			}
			make(FullCompile);
		}
		
		// either the preamble file exists and the format file is up-to-date  or  there is no preamble file
		else {
			// check if the main file has been modified since the creation of the dvi file
			int maintex_comp = compare_timestamp((texbasename+".tex").c_str(), (texbasename+output_ext).c_str());

			// check if a dependency file has been modified since the creation of the dvi file
			bool dependency_fresher = false;
			for(int i=1; !dependency_fresher && i<pnewglob->FileCount(); i++)
				dependency_fresher = SRC_FRESHER == compare_timestamp(pnewglob->File(i), (texbasename+output_ext).c_str()) ;

			if ( maintex_comp & ERR_SRCABSENT ) {
				cout << fgErr << "File " << mainfile << ".tex not found!\n" << fgNormal;
				delete pnewglob;
				return 2;
			}
			else if ( maintex_comp & ERR_OUTABSENT ) {
				cout << fgMsg << "+ " << mainfile << output_ext << " does not exist. Let's create it...\n";
				make(Compile);
			}
			else if( dependency_fresher || (maintex_comp == SRC_FRESHER) ) {
				cout << fgMsg << "+ the main file or some dependency file has been modified since last run. Let's recompile...\n";
				make(Compile);
			}
			// else 
			//   We have maintex_comp == OUT_FRESHER and dependency_fresher=false therefore 
			//   there is no need to recompile.
		}
	}
	
	// replace the current active file
	if( pglob )
		delete pglob;
	pglob = pnewglob;

	return 0;
}



// compare the time stamp of source and target files
int compare_timestamp(LPCTSTR sourcefile, LPCTSTR outputfile)
{
	struct stat attr_src, attr_out;			// file attribute structures
	int res_src, res_out;

	res_src = stat(sourcefile, &attr_src);
	res_out = stat(outputfile, &attr_out);
	if( res_src || res_out ) // problem when getting the attributes of the files?
		return  (res_src ? ERR_SRCABSENT : 0) | (res_out ? ERR_OUTABSENT : 0);
	else
		return ( difftime(attr_out.st_mtime, attr_src.st_mtime) > 0 ) ? OUT_FRESHER : SRC_FRESHER;
}



// Launch an external program and wait for its termination or for an abortion (set with the 
// hEvtAbortMake event)
DWORD launch_and_wait(LPCTSTR cmdline)
{
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	LPTSTR szCmdline= _tcsdup(cmdline);

	ZeroMemory( &si, sizeof(si) );
	si.cb = sizeof(si);
	//si.lpTitle = "latex";
	//si.wShowWindow = SW_HIDE;
	//si.dwFlags = STARTF_USESHOWWINDOW;
	ZeroMemory( &pi, sizeof(pi) );

	EnterCriticalSection( &cs );
	ExecutingExternalCommand = true;
	
	// Start the child process. 
	if( !CreateProcess( NULL,   // No module name (use command line)
		szCmdline,      // Command line
		NULL,           // Process handle not inheritable
		NULL,           // Thread handle not inheritable
		FALSE,          // Set handle inheritance to FALSE
		0,
		//CREATE_NEW_CONSOLE,              // No creation flags
		NULL,           // Use parent's environment block
		NULL,           // Use parent's starting directory 
		&si,            // Pointer to STARTUPINFO structure
		&pi )           // Pointer to PROCESS_INFORMATION structure
	) {
		cout << fgErr << "CreateProcess failed ("<< GetLastError() << ") : " << cmdline <<".\n" << fgNormal;
		ExecutingExternalCommand = false;
		LeaveCriticalSection( &cs ); 
		return -1;
	}
	free(szCmdline);

	// Wait until child process exits or the make process is aborted.
	HANDLE hp[2] = {pi.hProcess, hEvtAbortMake};
	DWORD dwRet = 0;
	switch( WaitForMultipleObjects(2, hp, FALSE, INFINITE ) ) {
	case WAIT_OBJECT_0:
		// Get the return code
		GetExitCodeProcess( pi.hProcess, &dwRet);
		break;
	case WAIT_OBJECT_0+1:
		dwRet = -1;
		TerminateProcess( pi.hProcess,1);
		break;
	default:
		break;
	}

	// Close process and thread handles. 
	CloseHandle( pi.hProcess );
	CloseHandle( pi.hThread );

	ExecutingExternalCommand = false;
    LeaveCriticalSection( &cs ); 

	return dwRet;
}

// Check that a file has been loaded
bool CheckFileLoaded()
{
	if( texbasename == "" ) {
		EnterCriticalSection( &cs );
		cout << fgErr << "You first need to load a .tex file!\n" << fgNormal;
		LeaveCriticalSection( &cs ); 
		return false;
	}
	else
		return true;
}

// Recompile the preamble into the format file "texfile.fmt" and then compile the main file
int fullcompile()
{
	if( !CheckFileLoaded() )
		return 0;

	// Check that external preamble exists
	if( ExternalPreamblePresent ) {
		EnterCriticalSection( &cs );
		string cmdline = string("pdftex -interaction=nonstopmode --src-specials -ini \"&" + texinifile + "\" \"\\input ")+preamble_filename+" \\dump\\endinput \"";
		cout << fgMsg << "-- Creation of the format file...\n";
		cout << "[running '" << cmdline << "']\n" << fgLatex;
		LeaveCriticalSection( &cs ); 
		int ret = launch_and_wait(cmdline.c_str());
		if( ret )
			return ret;
	}
	return compile();
}


// Compile the final tex file using the precompiled preamble
int compile()
{
	if( !CheckFileLoaded() )
		return false;

	string cmdline;

	EnterCriticalSection( &cs );
	cout << fgMsg << "-- Compilation of " << texbasename << ".tex ...\n";

	// External preamble used? Then compile using the precompiled preamble.
	if( ExternalPreamblePresent ) {
		// Remark on the latex code included in the following command line:
		//	 % Install a hook for the \input command ...
		///  \let\TEXDAEMONinput\input
		//   % which ignores the first file inclusion (the one inserting the preamble)
		//   \def\input#1{\let\input\TEXDAEMONinput}
		cmdline = string("pdftex -interaction=nonstopmode --src-specials \"&")+preamble_basename+"\" \"\\let\\TEXDAEMONinput\\input\\def\\input#1{\\let\\input\\TEXDAEMONinput} \\TEXDAEMONinput "+texbasename+".tex \"";
		
		//// Old version requiring the user to insert a conditional at the start of the main .tex file
		//string cmdline = string("pdftex -interaction=nonstopmode --src-specials \"&")+preamble_basename+"\" \"\\def\\incrcompilation{} \\input "+texbasename+".tex \"";
		
		cout << fgMsg << "[running '" << cmdline << "']\n" << fgLatex;
	}
	// no preamble: compile the latex file without the standard latex format file.
	else {
		cmdline = string("latex -interaction=nonstopmode -src-specials ")+texbasename+".tex";
		cout << fgMsg << " Running '" << cmdline << "'\n" << fgLatex;
	}
    LeaveCriticalSection( &cs ); 
	return launch_and_wait(cmdline.c_str());
}



BOOL CALLBACK LookForGsviewWindow(HWND hwnd, LPARAM lparam)
{
	DWORD pid;
	TCHAR szClass[15];
	GetWindowThreadProcessId(hwnd, &pid);
	RealGetWindowClass(hwnd, szClass, sizeof(szClass));
	if( _tcscmp(szClass, _T("gsview_class")) == 0 ) {
		hwndGsview32 = FindWindowEx(hwnd, NULL, "gsview_img_class", NULL);
	}
	return TRUE;
}

// start gsview, 
int start_gsview32(string filename)
{
	string cmdline = gsview32 + " " + filename;

	STARTUPINFO si;
	LPTSTR szCmdline= _tcsdup(cmdline.c_str());
	ZeroMemory( &si, sizeof(si) );
	si.cb = sizeof(si);
	ZeroMemory( &piGsview32, sizeof(piGsview32) );
	
	// Start the child process. 
	if( !CreateProcess( NULL,   // No module name (use command line)
		szCmdline,      // Command line
		NULL,           // Process handle not inheritable
		NULL,           // Thread handle not inheritable
		FALSE,          // Set handle inheritance to FALSE
		0,
		//CREATE_NEW_CONSOLE,              // No creation flags
		NULL,           // Use parent's environment block
		NULL,           // Use parent's starting directory 
		&si,            // Pointer to STARTUPINFO structure
		&piGsview32 )           // Pointer to PROCESS_INFORMATION structure
	) {
		EnterCriticalSection( &cs );
		cout << fgErr << "CreateProcess failed ("<< GetLastError() << ") : " << cmdline <<".\n" << fgNormal;
		LeaveCriticalSection( &cs ); 
		return -1;
	}
	else
	{
		hwndGsview32 = NULL;
		EnumThreadWindows(piGsview32.dwThreadId, LookForGsviewWindow, NULL);
	}
	free(szCmdline);


	return 0;
}

// open a file (located in the same directory as the .tex file) with the program associated with its extension in windows
int shellfile_open(string filename)
{
	EnterCriticalSection( &cs );
	cout << fgMsg << "-- view " << filename << " ...\n";
    LeaveCriticalSection( &cs ); 
	int ret = (int)(HINSTANCE)ShellExecute(NULL, _T("open"),
		_T((texdir+filename).c_str()),
		NULL,
		texdir.c_str(),
		SW_SHOWNORMAL);
	return ret>32 ? 0 : ret;
}

// View the .ps file
int view_ps()
{
	if( !CheckFileLoaded() )
		return 0;
	
	if( UseGswin32 )
		return start_gsview32(texbasename+".ps");
	else 
		return shellfile_open(texbasename+".ps");

}

// View the output file
int view()
{
	return ( output_ext == ".pdf" ) ? view_pdf() : view_dvi();
}

// View the .dvi file
int view_dvi()
{
	if( !CheckFileLoaded() )
		return 0;

	string file=texbasename+".dvi";
	EnterCriticalSection( &cs );
	cout << fgMsg << "-- view " << file << " ...\n";
    LeaveCriticalSection( &cs ); 
	int ret = (int)(HINSTANCE)ShellExecute(NULL, _T("open"),
		_T((texdir+file).c_str()),
		NULL,
		texdir.c_str(),
		SW_SHOWNORMAL);
	return ret>32 ? 0 : ret;
}

// View the .pdf file
int view_pdf()
{
	if( !CheckFileLoaded() )
		return 0;
	
	if( UseGswin32 )
		return start_gsview32(texbasename+".pdf");
	else 
		return shellfile_open(texbasename+".pdf");
}


// Edit the .tex file
int edit()
{
	if( !CheckFileLoaded() )
		return 0;

	EnterCriticalSection( &cs );
	cout << fgMsg << "-- editing " << texbasename << ".tex...\n";
    LeaveCriticalSection( &cs ); 
	int ret = (int)(HINSTANCE)ShellExecute(NULL, _T("open"),
		_T(texfullpath.c_str()),
		NULL,
		texdir.c_str(),
		SW_SHOWNORMAL);
	return ret>32 ? 0 : ret;
}

// Open the folder containing the .tex file
int openfolder()
{
	if( !CheckFileLoaded() )
		return 0;

	EnterCriticalSection( &cs );
	cout << fgMsg << "-- open directory " << texdir << " ...\n";
    LeaveCriticalSection( &cs ); 
	int ret = (int)(HINSTANCE)ShellExecute(NULL, NULL,
		_T(texdir.c_str()),
		NULL,
		NULL,
		SW_SHOWNORMAL);
	return ret>32 ? 0 : ret;
}


// Convert the postscript file to pdf
int ps2pdf()
{
	if( !CheckFileLoaded() )
		return 0;

	EnterCriticalSection( &cs );
	cout << fgMsg << "-- Converting " << texbasename << ".ps to pdf...\n";
	string cmdline = string("ps2pdf ")+texbasename+".ps";
	cout << fgMsg << " Running '" << cmdline << "'\n" << fgLatex;
    LeaveCriticalSection( &cs ); 
	return launch_and_wait(cmdline.c_str());
}

// Convert the dvi file to postscript using dvips
int dvips()
{
	if( !CheckFileLoaded() )
		return 0;

	EnterCriticalSection( &cs );
	cout << fgMsg << "-- Converting " << texbasename << ".dvi to postscript...\n";
	string cmdline = string("dvips ")+texbasename+".dvi -o "+texbasename+".ps";
	cout << fgMsg << " Running '" << cmdline << "'\n" << fgLatex;
    LeaveCriticalSection( &cs ); 
	return launch_and_wait(cmdline.c_str());
}

// Run bibtex on the tex file
int bibtex()
{
	if( !CheckFileLoaded() )
		return 0;

	EnterCriticalSection( &cs );
	cout << fgMsg << "-- Bibtexing " << texbasename << "tex...\n";
	string cmdline = string("bibtex ")+texbasename;
	cout << fgMsg << " Running '" << cmdline << "'\n" << fgLatex;
    LeaveCriticalSection( &cs ); 
	return launch_and_wait(cmdline.c_str());
}


// Restart the make thread if necessary.
// returns true if a thread has been launched.
bool RestartMakeThread( JOB makejob ) {
	if( makejob == Rest || !CheckFileLoaded() )
		return false;

	/// abort the current "make" thread if it is already started
	if( hMakeThread ) {
		SetEvent(hEvtAbortMake);
		// wait for the "make" thread to end
		WaitForSingleObject(hMakeThread, INFINITE);
	}

	// Create a new "make" thread.
	//  note: it is necessary to dynamically allocate a MAKETHREADPARAM structure
	//  otherwise, if we pass the address of a locally defined variable as a parameter to 
	//  CreateThread, the content of the structure may change
	//  by the time the make texbasenamethread is created (since the current thread runs concurrently).
	MAKETHREADPARAM *p = new MAKETHREADPARAM;
	p->makejob = makejob;
	DWORD makethreadID;
	hMakeThread = CreateThread( NULL,
			0,
			(LPTHREAD_START_ROUTINE) MakeThread,
			(LPVOID)p,
			0,
			&makethreadID);

	return hMakeThread!= NULL;
}

// Print a string in a given color only if the output is not already occupied, otherwise do nothing
void print_if_possible( std::ostream& color( std::ostream&  stream ) , string str)
{
	if( TryEnterCriticalSection(&cs) ){
		// do not print things if an external program is running				
		if(!hMakeThread && !ExecutingExternalCommand ) { 			
			cout << color << str;
		}
		LeaveCriticalSection(&cs);
	}
}


// Thread responsible of watching the directory and launching compilation when a change is detected
void WINAPI WatchingThread( void *param )
{
	if( !pglob ) {
		hWatchingThread = NULL;
		return;
	}

	// get the digest of the dependcy files
	md5 *dg_deps = new md5 [pglob->FileCount()];
    for (int n = 0; n < pglob->FileCount(); ++n)
	{
		if( !dg_deps[n].DigestFile(pglob->File(n)) ) {
			cerr << "File " << pglob->File(n) << " cannot be found or opened!\n";
			hWatchingThread = NULL;
			return;
		}
	}

	// get the digest of the main tex file
	string maintexfilename = texbasename + ".tex";
	md5 dg_tex = dg_deps[0];

	// get the digest of the file containing bibtex bibliography references
	string bblfilename = texbasename + ".bbl";
	md5 dg_bbl;
	dg_bbl.DigestFile(bblfilename.c_str());
	
	// get the digest of the preamble file
	md5 dg_preamble;
	if( ExternalPreamblePresent && !dg_preamble.DigestFile(preamble_filename.c_str()) ) {
		cerr << "File " << preamble_filename << " cannot be found or opened!\n" << fgLatex;
		hWatchingThread = NULL;
		return;
	}

	// reset the stop event so that it can be set if
	// some thread requires this one to stop
	ResetEvent(hEvtStopWatching);

	// open the directory to be monitored
	HANDLE hDir = CreateFile(
		texdir.c_str(), /* pointer to the directory containing the tex files */
		FILE_LIST_DIRECTORY,                /* access (read-write) mode */
		FILE_SHARE_READ|FILE_SHARE_DELETE|FILE_SHARE_WRITE,  /* share mode */
		NULL, /* security descriptor */
		OPEN_EXISTING, /* how to create */
		FILE_FLAG_BACKUP_SEMANTICS  | FILE_FLAG_OVERLAPPED , /* file attributes */
		NULL /* file with attributes to copy */
	  );
	 

	BYTE buffer [1024*sizeof(FILE_NOTIFY_INFORMATION )];

	OVERLAPPED overl;
	overl.hEvent = CreateEvent(NULL,FALSE,FALSE,NULL);

	while( 1 )
	{		
		DWORD BytesReturned;

		ReadDirectoryChangesW(
			 hDir, /* handle to directory */
			 &buffer, /* read results buffer */
			 sizeof(buffer), /* length of buffer */
			 FALSE, /* monitoring option */
			 //FILE_NOTIFY_CHANGE_SECURITY|FILE_NOTIFY_CHANGE_CREATION| FILE_NOTIFY_CHANGE_LAST_ACCESS|
			 FILE_NOTIFY_CHANGE_LAST_WRITE
			 //|FILE_NOTIFY_CHANGE_SIZE |FILE_NOTIFY_CHANGE_ATTRIBUTES |FILE_NOTIFY_CHANGE_DIR_NAME |FILE_NOTIFY_CHANGE_FILE_NAME
			 , /* filter conditions */
			 &BytesReturned, /* bytes returned */
			 &overl, /* overlapped buffer */
			 NULL); /* completion routine */
		
		DWORD dwNumberbytes;
		GetOverlappedResult(hDir, &overl, &dwNumberbytes, FALSE);
		HANDLE hp[2] = {overl.hEvent, hEvtStopWatching};
		DWORD dwRet = 0;
		switch( WaitForMultipleObjects(2, hp, FALSE, INFINITE ) ) {
			case WAIT_OBJECT_0:
				break;
			case WAIT_OBJECT_0+1: // the user asked to quit the program
				hWatchingThread = NULL;
				return;
			default:
				break;
			}

		//////////////
		// Check if some source file has changed and prepare the compilation requirement accordingly
		JOB makejob = Rest;
		FILE_NOTIFY_INFORMATION *pFileNotify;
		pFileNotify = (PFILE_NOTIFY_INFORMATION)&buffer;
		while( pFileNotify )
		{ 
			// Convert the filename from unicode string to oem string
			char filename[_MAX_FNAME];
			pFileNotify->FileName[min(pFileNotify->FileNameLength/2, _MAX_FNAME-1)] = 0;
			wcstombs( filename, pFileNotify->FileName, _MAX_FNAME );

			if( pFileNotify->Action != FILE_ACTION_MODIFIED ) {
				print_if_possible(fgIgnoredfile, string(".\"") + filename + "\" touched\n" );
			}
			else
			{
				md5 dg_new;
				// modification of the tex file?
				if( !_tcsicmp(filename,maintexfilename.c_str()) ) {
					// has the digest changed?
					if( dg_new.DigestFile(maintexfilename.c_str()) && (dg_tex != dg_new) ) {
 						dg_tex = dg_new;
						print_if_possible(fgDepFile, string("+ ") + maintexfilename + " changed\n" );
						makejob = max(Compile, makejob);
					}
					else {
						print_if_possible(fgIgnoredfile, string(".\"") + filename + "\" modified but digest preserved\n" );
					}
				}
				// modification of the bibtex file?
				else if( !_tcsicmp(filename,bblfilename.c_str()) ) {
					// has the digest changed?
					if( dg_new.DigestFile(filename) && (dg_bbl != dg_new) ) {
 						dg_bbl = dg_new;
						print_if_possible(fgDepFile, string("+ ") + filename + "(bibtex) changed\n" );
						makejob = max(Compile, makejob);
					}
					else {
						print_if_possible(fgIgnoredfile, string(".\"") + filename + "\" modified but digest preserved\n" );
					}
				}
				
				// modification of the preamble file?
				else if( ExternalPreamblePresent && !_tcsicmp(filename,preamble_filename.c_str())  ) {
					if( dg_new.DigestFile(preamble_filename.c_str()) && (dg_preamble!=dg_new) ) {
						dg_preamble = dg_new;
						print_if_possible(fgDepFile, string(".\"") + filename + "\" changed (preamble file).\n" );
						makejob = max(FullCompile, makejob);
					}
					else {
						print_if_possible(fgIgnoredfile, string(".\"") + filename + "\" modified but digest preserved\n" );
					}
				}
				
				// another file
				else {
					// is it a dependency file?
					int i;
					for(i=1; i<pglob->FileCount(); i++)
						if(!_tcsicmp(filename,pglob->File(i))) break;

					if( i<pglob->FileCount() ) {
						if ( dg_new.DigestFile(pglob->File(i)) && (dg_deps[i]!=dg_new) ) {
							dg_deps[i] = dg_new;
							print_if_possible(fgDepFile, string("+ \"") + pglob->File(i) + "\" changed (dependency file).\n" );
							makejob = max(Compile, makejob);
						}
						else {
							print_if_possible(fgIgnoredfile, string(".\"") + filename + "\" modified but digest preserved\n" );
						}
					}
					// not a revelant file ...				
					else {
						print_if_possible(fgIgnoredfile, string(".\"") + filename + "\" modified\n" );
					}
				}
			}			
			
			// step to the next entry if there is one
			if( pFileNotify->NextEntryOffset )
				pFileNotify = (FILE_NOTIFY_INFORMATION*) ((PBYTE)pFileNotify + pFileNotify->NextEntryOffset) ;
			else
				pFileNotify = NULL;
		}
	
		RestartMakeThread(makejob);
	}

	CloseHandle(overl.hEvent);
    CloseHandle(hDir);

	hWatchingThread = NULL;
}

