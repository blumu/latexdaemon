// Copyright William Blum 2007 (http://william.famille-blum.org/software/index.html)
// Created in September 2006
#define APP_NAME		"LatexDaemon"
#define VERSION_DATE	__DATE__
#define VERSION			0.9
#define BUILD			"24"

// See changelog.html for the list of changes:.

////////////////////
// Acknowledgment:
//
// - The MD5 class is a modification of CodeGuru's one: http://www.codeguru.com/Cpp/Cpp/algorithms/article.php/c5087
// - Command line processing routine from The Code Project: http://www.codeproject.com/useritems/SimpleOpt.asp
// - Console color header file (Console.h) from: http://www.codeproject.com/cpp/AddColorConsole.asp
// - Function CommandLineToArgvA and CommandLineToArgvW from http://alter.org.ua/en/docs/win/args/
// - Absolute/Relative path converter module from http://www.codeproject.com/useritems/path_conversion.asp
//
///////////////////

#define _WIN32_WINNT  0x0400
#define _CRT_SECURE_NO_DEPRECATE
#include <windows.h>
#include <Winbase.h>
#include <conio.h>
#include <string>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <vector>
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
#include "path_conv.h"
#include "CFilename.h"
#include "redir.h"
#include "LatexOutputFilters.h"
#include "global.h"
using namespace std;


//////////
/// Constants 



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
DWORD launch_and_wait(LPCTSTR cmdline, FILTER filt=Raw);
void RestartWatchingThread();
void WINAPI CommandPromptThread( void *param );
void WINAPI WatchingThread( void *param );
void WINAPI MakeThread( void *param );

BOOL CALLBACK LookForGsviewWindow(HWND hwnd, LPARAM lparam);

// TODO: move the following function to a proper class. After all we are programming in C++!
int loadfile( CSimpleGlob &nglob, JOB initialjob );
bool RestartMakeThread(JOB makejob);
DWORD fullcompile();
DWORD compile();
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

// watch for file changes ?
bool Watch = true;

// Automatic dependency detection (activated by default)
bool Autodep = true;

// Is some file loaded?
bool FileLoaded = false;

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

// Fire this event to notify the wathing thread that the dependencies have changed
HANDLE hEvtDependenciesChanged = NULL;

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

// by default the output extension is set to ".dvi", it must be changed to ".pdf" if pdflatex is used
string output_ext = ".dvi";


// Default command line arguments for Tex
string texoptions = " -interaction=nonstopmode --src-specials -max-print-line=120 ";

// static dependencies (added by the user) including the main .tex file
vector<CFilename> static_deps;

// dependencies automatically detected
vector<CFilename> auto_deps, auto_preamb_deps;

// output filter mode
FILTER Filter = Highlight;


// type of the parameter passed to the "make" thread
typedef struct {
	JOB			makejob;
} MAKETHREADPARAM  ;

// information concerning a directory being watched
typedef struct { 
    HANDLE  hDir;
    char szPath[_MAX_PATH];
    OVERLAPPED overl;
    BYTE buffer [2][512*sizeof(FILE_NOTIFY_INFORMATION )];
    int curBuffer;
} WATCHDIRINFO ;


// define the ID values to indentify the option
enum { 
	// command line options
	OPT_USAGE, OPT_INI, OPT_WATCH, OPT_FORCE, OPT_PREAMBLE, OPT_AFTERJOB, 
	// prompt commands
	OPT_HELP, OPT_COMPILE, OPT_FULLCOMPILE, OPT_QUIT, OPT_BIBTEX, OPT_DVIPS, 
	OPT_PS2PDF, OPT_EDIT, OPT_VIEWOUTPUT, OPT_OPENFOLDER, OPT_LOAD, 
	OPT_VIEWDVI, OPT_VIEWPS, OPT_VIEWPDF, OPT_GSVIEW, OPT_AUTODEP, OPT_FILTER
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
	{ OPT_AUTODEP,		_T("-autodep"),		SO_REQ_CMB },
    { OPT_AUTODEP,		_T("--autodep"),	SO_REQ_CMB },
    { OPT_FORCE,		_T("-force"),		SO_REQ_SEP },
    { OPT_FORCE,		_T("--force"),		SO_REQ_SEP },
    { OPT_FILTER,		_T("-filter"),		SO_REQ_CMB },
    { OPT_FILTER,		_T("--filter"),		SO_REQ_CMB },
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
    { OPT_WATCH,		_T("-watch"),		SO_REQ_CMB  },
    { OPT_FILTER,		_T("-filter"),		SO_REQ_CMB  },
    { OPT_INI,			_T("-ini"),			SO_REQ_CMB  },
    { OPT_PREAMBLE,		_T("-preamble"),	SO_REQ_CMB  },
    { OPT_AUTODEP,		_T("-autodep"),     SO_REQ_CMB  },
    { OPT_AFTERJOB,		_T("-afterjob"),	SO_REQ_CMB  },
	{ OPT_QUIT,			_T("-q"),			SO_NONE		},
	{ OPT_QUIT,			_T("-quit"),		SO_NONE		},
	{ OPT_EDIT,			_T("-e"),			SO_NONE		},
	{ OPT_EDIT,			_T("-edit"),		SO_NONE		},
	{ OPT_VIEWOUTPUT,	_T("-v"),			SO_NONE		},
	{ OPT_VIEWOUTPUT,	_T("-view"),		SO_NONE		},
	{ OPT_VIEWDVI,		_T("-vi"),			SO_NONE		},
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
         << " --autodep={yes|no}" << endl 
         << "   Activate the automatic detection of dependencies." << endl << endl
         << " --afterjob={dvips|rest}" << endl 
         << "   . 'dvips' specifies that dvips should be run after a successful compilation of the .tex file," <<endl
         << "   . 'rest' (default) specifies that nothing needs to be done after compilation."<<endl
         << " --filter={highlight|raw|err|warn|err+warn}" << endl 
         << "   . Set the latex output filter mode. Default: highlight" <<endl <<endl
         << " --preamble={none|external}" << endl 
         << "   . 'none' specifies that the main .tex file does not use an external preamble file."<<endl
         << "   The current version is not capable of extracting the preamble from the .tex file, therefore if this switch is used, the precompilation feature will be automatically deactivated."<<endl
         << "   . 'external' (default) specifies that the preamble is stored in an external file. The daemon first look for a preamble file called mainfile.pre, if this does not exist it tries preamble.tex and eventually, if neither exists, falls back to the 'none' option."<<endl
         << endl << "   If the files preamble.tex and mainfile.pre exist but do not correspond to the preamble of your latex document (i.e. not included with \\input{mainfile.pre} at the beginning of your .tex file) then you must set the 'none' option to avoid the precompilation of a wrong preamble." <<endl<<endl
         << "* dependencies contains a list of files that your main tex file relies on. You can sepcify list of files using jokers, for example '*.tex *.sty'. However, only the dependencies that resides in the same folder as the main tex file will be watched for changes." <<endl<<endl
         << "INSTRUCTIONS:" << endl
         << "Suppose main.tex is the main file of your Latex document." << endl
         << "  1. Move the preamble from main.tex to a new file named mainfile.pre" << endl 
         << "  2. Insert '\\input{mainfile.pre}' at the beginning of your mainfile.tex file" << endl 
         << "  3. Start the daemon with the command \"latexdaemon main.tex\" " << 
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



// Print a string in a given color only if the output is not already occupied, otherwise do nothing
void print_if_possible( std::ostream& color( std::ostream&  stream ) , string str)
{
    if( TryEnterCriticalSection(&cs) ) {
        cout << color << str << fgPrompt;
        LeaveCriticalSection(&cs);
    }
}

// Read a list of filenames from the file 'filename' and store them in the list 'deps'
void ReadDependencies(string filename, vector<CFilename> &deps)
{
    ifstream depFile;
    depFile.open(filename.c_str());
    if(depFile) {        
        char line[_MAX_PATH];
        while(!depFile.eof()){
            depFile.getline(line,_MAX_PATH);
            if( line[0] != '\0' ) {
                if( NULL == GetFileExtPart(line, 0, NULL) ) {
                    _tcscat_s(line,_MAX_PATH, ".tex");
                }
                deps.push_back(CFilename(texdir, line));
            }
        }
        depFile.close();
    }
}

// Check if one of the directory in the list vec corresponds to the path 'path'
bool is_wdi_in(vector<WATCHDIRINFO *> vec, PCTSTR path)
{
    for(vector<WATCHDIRINFO *>::iterator it = vec.begin(); it!= vec.end(); it++ ) {
        if(0 ==_tcsicmp((*it)->szPath, path ) )
            return true;
    }
    return false;
}

// Compare two set of strings and return 0 if they both contain the same strings.
// the algorithm is in O(n^2) but its ok since the lists are supposed to be small.
int CompareDeps(const vector<CFilename> &pv1, const vector<CFilename> &pv2)
{
    if(pv1.size()!=pv2.size())
        return 1;

    bool bfound = true;
    for(vector<CFilename>::const_iterator it1 = pv1.begin(); bfound && it1!=pv1.end(); it1++) {
        bfound = find( pv2.begin( ), pv2.end( ), *it1 ) != pv2.end();
    }
    return bfound ? 0 : 1;
}

// Refresh the list of automatic dependencies
void RefreshDependencies(bool bPreamble) {
    bool depChanged = false;

    vector<CFilename> new_deps;
    ReadDependencies(texbasename+".dep", new_deps);
    // Compare new_deps and auto_deps
    depChanged = 0 != CompareDeps(new_deps,auto_deps);
    
    vector<CFilename> new_preamb_deps;
    if( bPreamble ) {
        ReadDependencies(texbasename+"-preamble.dep", new_preamb_deps);
        depChanged |= 0!=CompareDeps(new_preamb_deps, auto_preamb_deps);
    }

    // If the dependencies have changed then restart the watching thread
    if( depChanged ) {
        auto_deps = new_deps;
        if( bPreamble ) auto_preamb_deps = new_preamb_deps;
        if(Watch) 
            SetEvent(hEvtDependenciesChanged);
    }
}

// Code executed when the -ini option is specified
void ExecuteOptionIni( string optionarg )
{
    texinifile = optionarg;
    SetTitle("monitoring");
    cout << fgNormal << "-Initialization file set to \"" << texinifile << "\"" << endl;;
    if( texinifile == "pdflatex" || texinifile == "pdftex" )
        output_ext = ".pdf";
    else if ( texinifile == "latex" || texinifile == "tex" )
        output_ext = ".dvi";
}
// Code executed when the -preamble option is specified
void ExecuteOptionPreamble( string optionarg )
{
    EnterCriticalSection( &cs );
    LookForExternalPreamble = (optionarg=="none") ? false : true ;
    if( LookForExternalPreamble )
        cout << fgNormal << "-I will look for an external preamble file next time I load a .tex file." << endl;
    else
        cout << fgNormal << "-I will not look for an external preamble next time I load a .tex file." << endl;
    LeaveCriticalSection( &cs );
}

// Code executed when the -force option is specified
void ExecuteOptionForce( string optionarg, JOB &force )
{
    EnterCriticalSection( &cs );
    if( optionarg=="fullcompile" ) {
        force = FullCompile;
        cout << fgNormal << "-Initial full compilation forced." << endl;
    }
    else {
        force = Compile;
        cout << fgNormal << "-Initial compilation forced." << endl;
    }
    LeaveCriticalSection( &cs );
}

// code for the -gsview option
void ExecuteOptionGsview()
{
	UseGswin32 = true;
	EnterCriticalSection( &cs );
	cout << fgNormal << "-Viewer set to GhostView." << endl;
	LeaveCriticalSection( &cs );
}

// Code executed when the -afterjob option is specified
void ExecuteOptionAfterJob( string optionarg )
{
	EnterCriticalSection( &cs );
	if( optionarg=="dvips" ) {
		afterjob = Dvips;
		cout << fgNormal << "-After-compilation job set to '" << optionarg << "'" << endl;
	}
	else {
		afterjob = Rest;
		cout << fgNormal << "-After-compilation job set to 'rest'" << endl;
	}
	LeaveCriticalSection(&cs);
}

// Code executed when the -watch option is specified
void ExecuteOptionWatch( string optionarg )
{
	Watch = optionarg != "no";
	if( Watch ) {
		EnterCriticalSection( &cs );
		cout << fgNormal << "-File modification monitoring activated" << endl;
		LeaveCriticalSection( &cs );
		if( FileLoaded && !hWatchingThread )
			RestartWatchingThread();
	}
	else {
		EnterCriticalSection( &cs );
		cout << fgNormal << "-File modification monitoring disabled" << endl;
		LeaveCriticalSection( &cs );
		if( hWatchingThread ) {
			// Stop the watching thread
			SetEvent(hEvtStopWatching);
			WaitForSingleObject(hWatchingThread, INFINITE);
		}
	}
}

// Code executed when the -filter option is specified
void ExecuteOptionOutputFilter( string optionarg )
{
    string filtermessage;
    if( optionarg == "err" ) {
        Filter = ErrOnly;
        filtermessage = "show error messages only";
    }
    else if( optionarg == "warn" ) {
        Filter = WarnOnly;
        filtermessage = "show warning messages only";
    }
    else if( optionarg == "err+warn" || optionarg == "warn+err" ) {
        Filter = ErrWarnOnly;
        filtermessage = "show error and warning messages only";
    }
    else if( optionarg == "raw" ) {
        Filter = Raw;
        filtermessage = "raw";
    }
    else { // if( optionarg == "highlight" ) {
        Filter = Highlight;
        filtermessage = "highlight error and warning messages";
    }

    EnterCriticalSection( &cs );
    cout << fgNormal << "-Latex output: " << filtermessage << endl;
    LeaveCriticalSection( &cs );
}

// Code executed when the -autodep option is specified
void ExecuteOptionAutoDependencies( string optionarg )
{
    Autodep = optionarg != "no";
    EnterCriticalSection( &cs );
    if( Autodep )
        cout << fgNormal << "-Automatic dependency detection activated" << endl;
    else
        cout << fgNormal << "-Automatic dependency detection disabled" << endl;
    LeaveCriticalSection( &cs );
}


// perform the necessary compilation
DWORD make(JOB makejob)
{
	DWORD ret;

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

    DWORD errcode = make(p->makejob);

    if( p->makejob == Compile && errcode == -1) 
        // Restore the backup of the .aux file
        CopyFile((texdir+auxbackupname).c_str(), (texdir+texbasename+".aux").c_str(), FALSE);
    
    // restore the prompt
    print_if_possible(fgPrompt, (hWatchingThread ? PROMPT_STRING_WATCH : PROMPT_STRING));

    if( errcode == 0 && Autodep ) 
        // Refresh the list of dependencies
        RefreshDependencies(p->makejob == FullCompile);

    hMakeThread = NULL;
    free(p);
}

// 
void RestartWatchingThread(){
    // stop it first if already started
    if( hWatchingThread ) {
        SetEvent(hEvtStopWatching);
        WaitForSingleObject(hWatchingThread, INFINITE);
    }

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

HWND FindCurrentProcessWindow(void)
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

        if( printprompt )
            print_if_possible(fgPrompt, hWatchingThread ? PROMPT_STRING_WATCH : PROMPT_STRING);
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
                cout << fgNormal;
                ShowUsage(NULL);
            LeaveCriticalSection( &cs ); 
            break;
        case OPT_HELP:
            EnterCriticalSection( &cs );
            cout << fgNormal << "The following commands are available:" << endl
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
                 << "  v[iew]          to view the output file (dvi or pdf depending on ini value)" << endl
                 << "  vi|viewdvi      to view the .dvi output file" << endl 
                 << "  vs|viewps       to view the .ps output file" << endl 
                 << "  vf|viewpdf      to view the .pdf output file" << endl << endl
                 << "You can also configure variables with:" << endl
                 << "  ini=inifile               set the initial format file to inifile" << endl
                 << "  preamble={none,external}  set the preamble mode for the file to be loaded" << endl
                 << "  autodep={yes,no}          activate/deactivate automatic dependency detection" << endl
                 << "  afterjob={rest,dvips}     set the job executed after latex compilation" << endl
                 << "  watch={yes,no}            activate/deactivate file modification watching" << endl 
                 << "  filter={highlight|raw|err|warn|err+warn}  set the filter mode for latex ouput. Default: highlight" << endl << endl;
            LeaveCriticalSection( &cs ); 
            break;
        case OPT_INI:			ExecuteOptionIni(args.OptionArg());              break;
        case OPT_PREAMBLE:		ExecuteOptionPreamble(args.OptionArg());         break;
        case OPT_AFTERJOB:		ExecuteOptionAfterJob(args.OptionArg());         break;
        case OPT_WATCH:			ExecuteOptionWatch(args.OptionArg());            break;
        case OPT_AUTODEP:		ExecuteOptionAutoDependencies(args.OptionArg()); break;
        case OPT_FILTER:        ExecuteOptionOutputFilter(args.OptionArg());     break;
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
                CSimpleGlob nglob(uiFlags);
                if (SG_SUCCESS != nglob.Add(args.FileCount(), args.Files()) ) {
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
                        ofn.hwndOwner = FindCurrentProcessWindow();
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
                            if (SG_SUCCESS != nglob.Add(szFile) ) {
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
                    if( loadfile(nglob, Rest) == 0 ) {
                        // Restart the watching thread
                        if( Watch )
                            RestartWatchingThread();
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

    cout << endl << fgNormal << APP_NAME << " " << VERSION << " Build " << BUILD << " by William Blum, " VERSION_DATE << endl << endl;;

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
    // create the event used to notify the "watching" thread of a dependency changed
    hEvtDependenciesChanged = CreateEvent(NULL,TRUE,FALSE,NULL);
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
            case OPT_USAGE:         ShowUsage(NULL);                                    goto exit;
            case OPT_INI:           ExecuteOptionIni(args.OptionArg());                 break;
            case OPT_AUTODEP:       ExecuteOptionAutoDependencies(args.OptionArg());    break;
            case OPT_WATCH:         ExecuteOptionWatch(args.OptionArg());               break;
            case OPT_FILTER:        ExecuteOptionOutputFilter(args.OptionArg());        break;
            case OPT_FORCE:         ExecuteOptionForce(args.OptionArg(),initialjob);    break;
            case OPT_GSVIEW:        ExecuteOptionGsview();                              break;
            case OPT_PREAMBLE:      ExecuteOptionPreamble(args.OptionArg());            break;
            case OPT_AFTERJOB:      ExecuteOptionAfterJob(args.OptionArg());            break;
            default:                break;
        }
        uiFlags |= (unsigned int) args.OptionId();
    }

    int ret = -1;    
    if( args.FileCount() == 0 ){
        cout << fgWarning << _T("No input file specified.\n") << fgNormal;
    }
    else {
        CSimpleGlob nglob(uiFlags);
        if (SG_SUCCESS != nglob.Add(args.FileCount(), args.Files()) ) {
            cout << fgErr << _T("Error while globbing files! Make sure that the given path is correct.\n") << fgNormal ;
            goto exit;
        }
        ret = loadfile(nglob, initialjob);
    }

    if( Watch ) { // If watching has been requested by the user
        // if some correct file was given as a command line parameter 
        if( ret == 0 )
            RestartWatchingThread(); // then start watching

        // start the prompt loop
        CommandPromptThread(NULL);
    }

exit:	
    DeleteCriticalSection(&cs);
    CloseHandle(hEvtAbortMake);
    CloseHandle(hEvtStopWatching);
    CloseHandle(hEvtDependenciesChanged);
    return ret;
}


// Test if the file 'filename' exists
bool FileExists( LPCTSTR filename ) {
    struct stat buffer ;
    return 0 == stat( filename, &buffer );
}

// Load a .tex file with some additional dependencies. The first file in the depglob must be the main .tex file, the rest being the static dependencies
int loadfile( CSimpleGlob &depglob, JOB initialjob )
{
    if(depglob.FileCount() == 0) {
        return 1;
    }

    TCHAR drive[4];
    TCHAR mainfile[_MAX_FNAME];
    TCHAR ext[_MAX_EXT];
    TCHAR fullpath[_MAX_PATH];
    TCHAR dir[_MAX_DIR];

    _fullpath( fullpath, depglob.File(0), _MAX_PATH );
    _tsplitpath( fullpath, drive, dir, mainfile, ext );

    if(  _tcsncmp(ext, ".tex", 4) )	{
        cerr << fgErr << "Error: this file does not seem to be a TeX document!\n\n" << fgNormal;
        return 2;
    }

    // set the global variables
    texfullpath = fullpath;
    texdir = string(drive) + dir;
    texbasename = mainfile;
    auxbackupname = texbasename+".aux.bak";

    // set console title
    SetTitle("Initialization");
    cout << "-Main file: '" << fullpath << "'\n";
    cout << "-Directory: " << drive << dir << "\n";
    
    FileLoaded = false; // will be set to true when the loading is finished


    // clear the dependency list
    auto_deps.clear();
    auto_preamb_deps.clear();
    static_deps.clear();


    if( depglob.FileCount()>1 ) 
        cout << "-Dependencies manually added:\n";
    else
        cout << "-No additional dependency specified.\n";

    // Add the static dependencies. We change all the relative paths to make them relative to the main .tex file dir
    TCHAR tmpfullpath[_MAX_PATH];
          //tmprelpath[_MAX_PATH];
    for(int i=0; i<depglob.FileCount(); i++) {
        if( i > 0 ) _tprintf(_T("  %2d: '%s'\n"), i, depglob.File(i));
        _fullpath( tmpfullpath, depglob.File(i), _MAX_PATH );
        //Abs2Rel(tmpfullpath, tmprelpath, _MAX_PATH, texdir.c_str());
        static_deps.push_back(CFilename(tmpfullpath));
    }

    // change current directory
    _chdir(texdir.c_str());

    // check for the presence of the external preamble file
    if( LookForExternalPreamble ) {
        // compare the timestamp of the preamble.tex file and the format file
        preamble_filename = string(mainfile) + "." DEFAULTPREAMBLE1_EXT;
        preamble_basename = string(mainfile);
        ExternalPreamblePresent = FileExists(preamble_filename.c_str());
        if ( !ExternalPreamblePresent ) {
            // try with the second default preamble name
            preamble_filename = DEFAULTPREAMBLE2_FILENAME;
            preamble_basename = DEFAULTPREAMBLE2_BASENAME;
            ExternalPreamblePresent = FileExists(preamble_filename.c_str());
            if ( !ExternalPreamblePresent ) {
	            cout << fgWarning << "Warning: Preamble file not found! (I have looked for " << mainfile << "." << DEFAULTPREAMBLE1_EXT << " and " << DEFAULTPREAMBLE2_FILENAME << ")\nPrecompilation mode deactivated!\n";
            }
        }
    }
    else
        ExternalPreamblePresent = false;


    ////////////////////////////////
    //
    // Determine what needs to be recompiled based on the files that have been touched since last compilation.
    //

    // what kind of compilation is needed to update the output file (format & .dvi file) ?
    JOB job = initialjob;  // by default it is the job requested by the user (using command line options)


    ///////////////////
    // Check if the preambles dependencies have been touched since last compilation (if an external preamble file is used)

    if( ExternalPreamblePresent ) {
        ReadDependencies(texbasename+"-preamble.dep", auto_preamb_deps);
        cout << "-Preamble file: " << preamble_filename << "\n";

        // compare the timestamp of the preamble file with the format file
        int res = compare_timestamp(preamble_filename.c_str(), (preamble_basename+".fmt").c_str());

        // If the format file is older than the preamble file
        if( res == SRC_FRESHER ) {
            // then it is necessary to recreate the format file and to perform a full recompilation
            job = FullCompile;
            cout << fgMsg << "+ " << preamble_filename << " has been modified since last run.\n";
        }
        // if the format file does not exist
        else if ( res & ERR_OUTABSENT ) {
            job = FullCompile;
            cout << fgMsg << "+ " << preamble_basename << ".fmt does not exist. Let's create it...\n";
        }


        // Check if some preamble dependency has been tooched
        if( job != FullCompile )
            for(vector<CFilename>::iterator it = auto_preamb_deps.begin();
                it!= auto_preamb_deps.end(); it++) {
                res = compare_timestamp(it->c_str(), (preamble_basename+".fmt").c_str());
                if( res == SRC_FRESHER ) {
                    cout << fgMsg << "+ Preamble dependency modified since last run: " << it->c_str() << "\n";
                    job = FullCompile;
                }
            }
    }

    
    // Initialize the .tex file dependency list
    ReadDependencies(texbasename+".dep", auto_deps);

    // If the preamble file does not need to be recompiled
    // and if compilation of the .tex file is not requested by the user
    if( job != Compile && job != FullCompile) {

        ///////////////////
        // Check if the .tex file dependencies have been touched since last compilation
        
        // check if the main file has been modified since the creation of the dvi file
        int maintex_comp = compare_timestamp((texbasename+".tex").c_str(), (texbasename+output_ext).c_str());

        if ( maintex_comp & ERR_SRCABSENT ) {
            cout << fgErr << "File " << mainfile << ".tex not found!\n" << fgNormal;
            return 3;
        }
        else if ( maintex_comp & ERR_OUTABSENT ) {
            cout << fgMsg << "+ " << mainfile << output_ext << " does not exist. Let's create it...\n";
            job = Compile;
        }
        else if( maintex_comp == SRC_FRESHER ) {
            cout << fgMsg << "+ the main .tex file has been modified since last run. Let's recompile...\n";
            job = Compile;
        }
        else { // maintex_comp == OUT_FRESHER 

            // check if some manual dependency file has been modified since the creation of the dvi file
            for(vector<CFilename>::iterator it = static_deps.begin(); it!=static_deps.end(); it++)                
                if( SRC_FRESHER == compare_timestamp(it->c_str(), (texbasename+output_ext).c_str()) ) {
                    cout << fgMsg << "+ The file " << it->Relative(texdir.c_str()) << ", on which the main .tex depends has been modified since last run. Let's recompile...\n";
                    job = Compile;
                    break;
                }

            // Check if some automatic dependencies has been touched
            if( job != FullCompile )
                for(vector<CFilename>::iterator it = auto_deps.begin();
                    it!= auto_deps.end(); it++)
                    if( SRC_FRESHER == compare_timestamp(it->c_str(), (texbasename+output_ext).c_str()) ) {
                        cout << fgMsg << "+ The file " << it->c_str() << ", on which the main .tex depends has been modified since last run. Let's recompile...\n";                        
                        job = Compile;
                    }
        }
    }


    // Perform the job that needs to be done
    if( job != Rest ) {
        if( job == FullCompile )
            cout << fgMsg << "  Let's recreate the format file and then recompile " << texbasename << ".tex.\n";

        make(job);
    }

    FileLoaded = true;

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
DWORD launch_and_wait(LPCTSTR cmdline, FILTER filt)
{
    DWORD dwRet = 0;
    LPTSTR szCmdline= _tcsdup(cmdline);

    ostreamLatexFilter filtstream(cout.rdbuf(), filt);
    ostream *pRedirStream = (filt != Raw) ? &filtstream : NULL;
    CRedirector redir(pRedirStream , &cs);
    if( !pRedirStream ) EnterCriticalSection(&cs);
    if( redir.Open(szCmdline) ) {
        HANDLE hProc = redir.GetProcessHandle();

        // Wait until child process exits or the make process is aborted.
        HANDLE hp[2] = {hProc, hEvtAbortMake};
        switch( WaitForMultipleObjects(2, hp, FALSE, INFINITE ) ) {
        case WAIT_OBJECT_0:
            // Get the return code
            GetExitCodeProcess( hProc, &dwRet);
            break;
        case WAIT_OBJECT_0+1:
            dwRet = -1;
            TerminateProcess( hProc,1);
            break;
        default:
            break;
        }

        if(!pRedirStream) LeaveCriticalSection( &cs ); 
    }
    else {
        dwRet = GetLastError();
        EnterCriticalSection( &cs );
        cout << fgErr << "CreateProcess failed ("<< dwRet << ") : " << cmdline <<".\n" << fgNormal;
        LeaveCriticalSection( &cs ); 
    }

    free(szCmdline);
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
DWORD fullcompile()
{
    if( !CheckFileLoaded() )
        return 0;

    // Check that external preamble exists
    if( ExternalPreamblePresent ) {
        string latex_pre, latex_post;
        if( Autodep ) {
            latex_pre =
                "\\edef\\TheAtCode{\\the\\catcode`\\@} \\catcode`\\@=11"
                " \\newwrite\\preambledepfile"
                "  \\immediate\\openout\\preambledepfile = " + texbasename + "-preamble.dep"

                // create a backup of the original \input command
                // (the \ifx test avoids to create a loop in case another hooking has already been set)
                " \\ifx\\TEXDAEMON@@input\\@undefined\\let\\TEXDAEMON@@input\\input\\fi" 
                                                                                         
                // same for \include
                " \\ifx\\TEXDAEMON@@include\\@undefined\\let\\TEXDAEMON@@include\\include\\fi" 

                // Hook \input (when it is used with the curly bracket {..})
                " \\def\\input{\\@ifnextchar\\bgroup\\Dump@input\\TEXDAEMON@@input}"
                " \\def\\Dump@input#1{ \\immediate\\write\\preambledepfile{#1}\\TEXDAEMON@@input #1}"

                " \\def\\include#1{\\immediate\\write\\preambledepfile{#1}\\TEXDAEMON@@include #1}"
                " \\catcode`\\@=\\TheAtCode\\relax";

            latex_post = "\\edef\\TheAtCode{\\the\\catcode`\\@} \\catcode`\\@=11"
                         " \\immediate\\closeout\\preambledepfile"     // Close the dependency file
                         " \\let\\input\\TEXDAEMON@@input"             // Restore the original \input
                         " \\let\\include\\TEXDAEMON@@include";        //    and \include commands.
                         " \\catcode`\\@=\\TheAtCode\\relax";
        }
        else {
            latex_pre = latex_post = "";
        }

        EnterCriticalSection( &cs );
        string cmdline = string("pdftex")
            + texoptions + " -ini \"&" + texinifile + "\""
            + " \"" 
                + latex_pre
                + "\\input "+ preamble_filename 
                + latex_post
                +" \\dump\\endinput" 
            + "\"";
        cout << fgMsg << "-- Creation of the format file...\n";
        cout << "[running '" << cmdline << "']\n" << fgLatex;
        LeaveCriticalSection( &cs ); 
        DWORD ret = launch_and_wait(cmdline.c_str(), Filter);
        if( ret )
            return ret;
    }

    return compile();
}


// Compile the final tex file using the precompiled preamble
DWORD compile()
{
    if( !CheckFileLoaded() )
        return false;


    string texengine;  // tex engine to run
    string formatfile; // formatfile to preload before reading the main .tex file    
    string latex_pre,  // the latex code that will be executed before and after the main .tex file
           latex_post; 

    if( ExternalPreamblePresent ) {
        texengine = "pdftex";
        formatfile = preamble_basename;
    }
    else {
        texengine = texinifile; // 'texinifile' is equal to "latex" or "pdflatex" depending on the daemon -ini options
        formatfile = ""; // no format file
    }

    if ( Autodep ) {
        latex_pre =
            // change the @ catcode
            "\\edef\\TheAtCode{\\the\\catcode`\\@} \\catcode`\\@=11"
                // Create the .dep file
                " \\newwrite\\dependfile"
                " \\openout\\dependfile = " +texbasename + ".dep"

                // Create a backup of the original \include command
                // (the \ifx test avoids to create a loop in case another hooking has already been set)
                " \\ifx\\TEXDAEMON@ORG@include\\@undefined\\let\\TEXDAEMON@ORG@include\\include\\fi" 

                // same for input
                " \\ifx\\TEXDAEMON@ORG@input\\@undefined\\let\\TEXDAEMON@ORG@input\\input\\fi" 

                // Install a hook for the \include command
                " \\def\\include#1{\\write\\dependfile{#1}\\TEXDAEMON@ORG@include{#1}}"

                // A hook that write the name of the included file to the dependency file
                " \\def\\TEXDAEMON@DumpDep@input#1{ \\write\\dependfile{#1}\\TEXDAEMON@ORG@input #1}"

                // A hook that does nothing the first time it's being called
                // (the first call to \input{...} corresponds to the inclusion of the preamble)
                // and then behave like the preceeding hook
                " \\def\\TEXDAEMON@HookIgnoreFirst@input#1{ \\let\\input\\TEXDAEMON@DumpDep@input }"

                // Install a hook for the \input command.
                + (ExternalPreamblePresent
                    ?  " \\def\\input{\\@ifnextchar\\bgroup\\TEXDAEMON@HookIgnoreFirst@input\\TEXDAEMON@ORG@input}"
                    :  " \\def\\input{\\@ifnextchar\\bgroup\\TEXDAEMON@DumpDep@input\\TEXDAEMON@ORG@input}")
            
            // restore the @ original catcode
            + " \\catcode`\\@=\\TheAtCode\\relax";

        latex_post = " \\closeout\\dependfile";

    }
    else if( ExternalPreamblePresent ) {
        // creation of the dependency file is not required...
        // we just need to hook the first call to \input{..} in order to prevent the preamble from being loaded
        latex_pre = "\\edef\\TheAtCode{\\the\\catcode`\\@} \\catcode`\\@=11"
                        " \\ifx\\TEXDAEMON@@input\\@undefined\\let\\TEXDAEMON@@input\\input\\fi"
                        " \\def\\input{\\@ifnextchar\\bgroup\\TEXDAEMON@input\\TEXDAEMON@@input}"
                        " \\def\\TEXDAEMON@input#1{\\let\\input\\TEXDAEMON@@input }"
                    " \\catcode`\\@=\\TheAtCode\\relax";
        latex_post = "";

    }

    // There is no precompiled preamble and dependency calculation is not required therefore
    // we compile the latex file normally without loading any format file.
    else {
        latex_pre = latex_post = ""; // no special code to run before or after the main .tex file
    }


    ///////
    // Create the command line adding some latex code before and after the .tex file if necessary
    string cmdline;
    if( latex_pre == "" && latex_post == "" ) 
        cmdline = texengine + texoptions +  " " + texbasename + ".tex";
    else
        cmdline = texengine +
            texoptions + 
            ( formatfile!="" ? " \"&"+formatfile+"\"" : "" ) +
            " \"" + latex_pre
                  + " \\input "+texbasename+".tex "
                  + latex_post
            + "\"";

    // External preamble used? Then compile using the precompiled preamble.


    EnterCriticalSection( &cs );
    cout << fgMsg << "-- Compilation of " << texbasename << ".tex ...\n";
    cout << fgMsg << "[running '" << cmdline << "']\n" << fgLatex;
    LeaveCriticalSection( &cs ); 

    return launch_and_wait(cmdline.c_str(), Filter);
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
        free(szCmdline);
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
    string file = texdir+filename;
    SHELLEXECUTEINFO shei = {0};
    shei.cbSize = sizeof(SHELLEXECUTEINFO);
    shei.fMask = 0;
    shei.hwnd = NULL;
    shei.lpVerb = _T("open");
    shei.lpFile = file.c_str();
    shei.lpParameters = NULL;
    shei.lpDirectory = texdir.c_str();
    shei.nShow = SW_SHOWNORMAL;
    shei.hInstApp = NULL;
    return ShellExecuteEx(&shei) ? 0 : GetLastError();
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

    string filepath = texdir+file;
    SHELLEXECUTEINFO shei = {0};
    shei.cbSize = sizeof(SHELLEXECUTEINFO);
    shei.fMask = 0;
    shei.hwnd = NULL;
    shei.lpVerb = _T("open");
    shei.lpFile = filepath.c_str();
    shei.lpParameters = NULL;
    shei.lpDirectory = texdir.c_str();
    shei.nShow = SW_SHOWNORMAL;
    shei.hInstApp = NULL;
    return ShellExecuteEx(&shei) ? 0 : GetLastError();
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

    SHELLEXECUTEINFO shei = {0};
    shei.cbSize = sizeof(SHELLEXECUTEINFO);
    shei.fMask = 0;
    shei.hwnd = NULL;
    shei.lpVerb = _T("open");
    shei.lpFile = texfullpath.c_str();
    shei.lpParameters = NULL;
    shei.lpDirectory = texdir.c_str();
    shei.nShow = SW_SHOWNORMAL;
    shei.hInstApp = NULL;
    return ShellExecuteEx(&shei) ? 0 : GetLastError();
}

// Open the folder containing the .tex file
int openfolder()
{
    if( !CheckFileLoaded() )
        return 0;

    EnterCriticalSection( &cs );
    cout << fgMsg << "-- open directory " << texdir << " ...\n";
    LeaveCriticalSection( &cs ); 

    SHELLEXECUTEINFO shei = {0};
    shei.cbSize = sizeof(SHELLEXECUTEINFO);
    shei.fMask = 0;
    shei.hwnd = NULL;
    shei.lpVerb = NULL;
    shei.lpFile = texdir.c_str();
    shei.lpParameters = NULL;
    shei.lpDirectory = NULL;
    shei.nShow = SW_SHOWNORMAL;
    shei.hInstApp = NULL;
    return ShellExecuteEx(&shei) ? 0 : GetLastError();
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


// Allocate and initialize a WATCHDIRINFO structure
WATCHDIRINFO *CreateWatchDir(PCTSTR dirpath)
{
    WATCHDIRINFO *pwdi = new WATCHDIRINFO;


    _tcscpy_s(pwdi->szPath, _countof(pwdi->szPath), dirpath);

    pwdi->hDir = CreateFile(
        dirpath, // pointer to the directory containing the tex files
        FILE_LIST_DIRECTORY,                // access (read-write) mode
        FILE_SHARE_READ|FILE_SHARE_DELETE|FILE_SHARE_WRITE,  // share mode
        NULL, // security descriptor
        OPEN_EXISTING, // how to create
        FILE_FLAG_BACKUP_SEMANTICS  | FILE_FLAG_OVERLAPPED , // file attributes
        NULL // file with attributes to copy 
      );
  
    memset(&pwdi->overl, 0, sizeof(pwdi->overl));
    pwdi->overl.hEvent = CreateEvent(NULL,FALSE,FALSE,NULL);

    pwdi->curBuffer = 0;

    // watch the directory
    DWORD BytesReturned;
    ReadDirectoryChangesW(
         pwdi->hDir, /* handle to directory */
         &pwdi->buffer[pwdi->curBuffer], /* read results buffer */
         sizeof(pwdi->buffer[pwdi->curBuffer]), /* length of buffer */
         FALSE, /* monitoring option */
         //FILE_NOTIFY_CHANGE_SECURITY|FILE_NOTIFY_CHANGE_CREATION| FILE_NOTIFY_CHANGE_LAST_ACCESS|
         FILE_NOTIFY_CHANGE_LAST_WRITE
         //|FILE_NOTIFY_CHANGE_SIZE |FILE_NOTIFY_CHANGE_ATTRIBUTES |FILE_NOTIFY_CHANGE_DIR_NAME |FILE_NOTIFY_CHANGE_FILE_NAME
         , /* filter conditions */
         &BytesReturned, /* bytes returned */
         &pwdi->overl, /* overlapped buffer */
         NULL); /* completion routine */

    return pwdi;
}

// Thread responsible of watching the directory and launching compilation when a change is detected
void WINAPI WatchingThread( void *param )
{
    if( static_deps.size() == 0 ) {
        hWatchingThread = NULL;
        return;
    }

    EnterCriticalSection(&cs);
    cout << fgMsg << "\n-- Watching files for change...\n" << fgNormal;
    LeaveCriticalSection( &cs ); 
 
    // reset the hEvtStopWatching event so that it can be set if
    // some thread requires to stop this thread
    ResetEvent(hEvtStopWatching);

    // Get current directory and keep as reference directory
    char sCurrDir[MAX_PATH];
    GetCurrentDir(sCurrDir);

    // Iterate this loop every time the dependencies change
    bool bContinue = true;
    while( bContinue ) {
        bContinue = false;

        ///// Dependencies of the main .tex file
        vector<CFilename> deps;
        // Add the manual dependencies (the first one is the main tex file)
        for (vector<CFilename>::iterator it = static_deps.begin(); it != static_deps.end(); it++)
            deps.push_back(*it);
        // load the depencies automatically generated by the last compilation of the main tex file
        if( Autodep )
            for(vector<CFilename>::iterator it = auto_deps.begin(); it!=auto_deps.end();it++)
                deps.push_back(*it);
       
        ////// get the digest of the dependcy files
        md5 *dg_deps = new md5 [deps.size()];
        for (size_t n = 0; n < deps.size(); n++) {
            if( dg_deps[n].DigestFile(deps[n].c_str()) ) {
                if(n>0) print_if_possible(fgMsg, "Dependency detected: " + deps[n].Relative(sCurrDir) + "\n");
            }
            else
                print_if_possible(fgIgnoredfile, "Dependency detected but cannot be opened: " + deps[n].Relative(sCurrDir) + "\n");
        }

        ///// Dependencies of the preamble file
        vector<CFilename> preamb_deps;
        md5 *dg_preamb_deps = NULL;
        if( ExternalPreamblePresent ) {
            preamb_deps.push_back(CFilename(sCurrDir,preamble_filename));
            // load the depencies automatically generated by the last compilation of the preamble
            if( Autodep )
                for(vector<CFilename>::iterator it = auto_preamb_deps.begin();
                    it!=auto_preamb_deps.end();it++)
                    preamb_deps.push_back(CFilename(sCurrDir,*it));

            dg_preamb_deps = new md5 [preamb_deps.size()];
            for (size_t n = 0; n < preamb_deps.size(); n++) {
                if( dg_preamb_deps[n].DigestFile(preamb_deps[n].c_str()) ) {
                    if(n>0) print_if_possible(fgMsg, "Preamble dependency detected: " + preamb_deps[n].Relative(sCurrDir) + "\n");
                }
                else
                {
                    if( n == 0  ) {
                        print_if_possible(fgErr, "The preamble file " + preamble_filename + " cannot be found or opened!\n" );
                        hWatchingThread = NULL;
                        delete dg_deps;
                        delete dg_preamb_deps;
                        return;
                    }
                    else
                        print_if_possible(fgIgnoredfile, "Preamble dependency detected but cannot be opened: " + preamb_deps[n].Relative(sCurrDir) + "\n");
                }
            }
        }

        // Get the digest of the file containing bibtex bibliography references
        string bblfilename = texbasename + ".bbl";
        md5 dg_bbl;
        dg_bbl.DigestFile(bblfilename.c_str());
    	
        // Reset the hEvtDependenciesChanged event so that it can be set if
        // some thread requires to notify a dependency change
        ResetEvent(hEvtDependenciesChanged);

        ////// Create the list of dir to be watched
        vector<WATCHDIRINFO *> watchdirs; // info on the directories to be monitored

        for(vector<CFilename>::iterator it = deps.begin(); it!=deps.end();it++) {
            string abspath = it->GetDirectory();
            if( !is_wdi_in(watchdirs, abspath.c_str()) )
                watchdirs.push_back(CreateWatchDir(abspath.c_str()));
        }
        for(vector<CFilename>::iterator it = preamb_deps.begin(); it!=preamb_deps.end();it++) {
            string abspath = it->GetDirectory();
            if( !is_wdi_in(watchdirs, abspath.c_str()) )
                watchdirs.push_back(CreateWatchDir(abspath.c_str()));
        }

        // Number of directories to be monitored
        size_t nWdi = watchdirs.size();

        DWORD nHdReserved = 2; // number of handle reserved for message events
        HANDLE *hp = new HANDLE[nWdi+nHdReserved];
        hp[0] = hEvtStopWatching;
        hp[1] = hEvtDependenciesChanged;
        for(size_t i=0;i<nWdi;i++)
            hp[i+nHdReserved] = watchdirs[i]->overl.hEvent;

        int iTriggeredDir; // index of the last directory in which a change has been detected
        while( 1 ) {
            DWORD dwRet = 0;
            DWORD dwObj = WaitForMultipleObjects(2+(DWORD)nWdi, hp, FALSE, INFINITE ) - WAIT_OBJECT_0;
            _ASSERT( dwObj >= 0 && dwObj <= nWdi+nHdReserved );
            if( dwObj == 0 ) { // the user asked to quit the program
                bContinue = false;
                goto clean;
            }
            else if ( dwObj == 1 ) { // notification of depend change
                print_if_possible(fgMsg, "\n-- Dependencies have changed\n" );
                bContinue = true;
                goto clean;
            }
            else if ( dwObj >= nHdReserved && dwObj < nWdi+nHdReserved) {
                iTriggeredDir = dwObj-nHdReserved;
            }            
            else {
                // BUG!
                bContinue = false;
                goto clean;
            }

            // Read the asyncronous result
            DWORD dwNumberbytes;
            GetOverlappedResult(watchdirs[iTriggeredDir]->hDir, &watchdirs[iTriggeredDir]->overl, &dwNumberbytes, FALSE);

            // Switch the 2 buffers
            watchdirs[iTriggeredDir]->curBuffer =  1- watchdirs[iTriggeredDir]->curBuffer;

            // continue to watch the directory in which a change has just been detected
            DWORD BytesReturned;
            ReadDirectoryChangesW(
                 watchdirs[iTriggeredDir]->hDir, /* handle to directory */
                 &watchdirs[iTriggeredDir]->buffer[watchdirs[iTriggeredDir]->curBuffer], /* read results buffer */
                 sizeof(watchdirs[iTriggeredDir]->buffer[watchdirs[iTriggeredDir]->curBuffer]), /* length of buffer */
                 FALSE, /* monitoring option */
                 //FILE_NOTIFY_CHANGE_SECURITY|FILE_NOTIFY_CHANGE_CREATION| FILE_NOTIFY_CHANGE_LAST_ACCESS|
                 FILE_NOTIFY_CHANGE_LAST_WRITE
                 //|FILE_NOTIFY_CHANGE_SIZE |FILE_NOTIFY_CHANGE_ATTRIBUTES |FILE_NOTIFY_CHANGE_DIR_NAME |FILE_NOTIFY_CHANGE_FILE_NAME
                 , /* filter conditions */
                 &BytesReturned, /* bytes returned */
                 &watchdirs[iTriggeredDir]->overl, /* overlapped buffer */
                 NULL); /* completion routine */


            //////////////
            // Check if some source file has changed and prepare the compilation requirement accordingly
            JOB makejob = Rest;
            FILE_NOTIFY_INFORMATION *pFileNotify;
            pFileNotify = (PFILE_NOTIFY_INFORMATION)&watchdirs[iTriggeredDir]->buffer[1-watchdirs[iTriggeredDir]->curBuffer];
            while( pFileNotify ) { 

                // Convert the filename from unicode string to oem string
                char tmp[_MAX_FNAME];
                pFileNotify->FileName[min(pFileNotify->FileNameLength/2, _MAX_FNAME-1)] = 0;
                wcstombs( tmp, pFileNotify->FileName, _MAX_FNAME );
                CFilename modifiedfile(watchdirs[iTriggeredDir]->szPath, tmp);

                if( pFileNotify->Action != FILE_ACTION_MODIFIED ) {
					print_if_possible(fgIgnoredfile, string(".\"") + modifiedfile.Relative(texdir) + "\" touched\n" );
                }
                else {
                    md5 dg_new;

                    // is it the bibtex file?
                    if( modifiedfile == CFilename(texdir, bblfilename) ) {
                        // has the digest changed?
                        if( dg_new.DigestFile(modifiedfile) && (dg_bbl != dg_new) ) {
                            dg_bbl = dg_new;
                            print_if_possible(fgDepFile, string("+ ") + modifiedfile.Relative(texdir) + "(bibtex) changed\n" );
                            makejob = max(Compile, makejob);
                        }
                        else
                            print_if_possible(fgIgnoredfile, string(".\"") + modifiedfile.Relative(texdir) + "\" modified but digest preserved\n" );
                    }
                    else {
                        // is it a dependency of the main .tex file?
                        vector<CFilename>::iterator it = find(deps.begin(),deps.end(), modifiedfile);
                        if(it != deps.end() ) {
                            size_t i = it - deps.begin();
                            if ( dg_new.DigestFile(modifiedfile) && (dg_deps[i]!=dg_new) ) {
	                            dg_deps[i] = dg_new;
	                            print_if_possible(fgDepFile, string("+ \"") + modifiedfile.Relative(texdir) + "\" changed (dependency file).\n" );
	                            makejob = max(Compile, makejob);
                            }
                            else
	                            print_if_possible(fgIgnoredfile, string(".\"") + modifiedfile.Relative(texdir) + "\" modified but digest preserved\n" );
                        }
                        else if ( ExternalPreamblePresent ) {
                            // is it a dependency of the preamble?
                            vector<CFilename>::iterator it = find(preamb_deps.begin(),preamb_deps.end(), modifiedfile);
                            if(it != preamb_deps.end() ) {
                                size_t i = it - preamb_deps.begin();
                                if ( dg_new.DigestFile(modifiedfile) && (dg_preamb_deps[i]!=dg_new) ) {
                                    dg_preamb_deps[i] = dg_new;
                                    print_if_possible(fgDepFile, string("+ \"") + modifiedfile.Relative(texdir) + "\" changed (preamble dependency file).\n" );
                                    makejob = max(FullCompile, makejob);
                                }
                                else
                                    print_if_possible(fgIgnoredfile, string(".\"") + modifiedfile.Relative(texdir) + "\" modified but digest preserved\n" );
                            }
                            // not a relevant file ...
                            else
                                print_if_possible(fgIgnoredfile, string(".\"") + modifiedfile.Relative(texdir) + "\" modified\n" );
                        }
                        // not a relevant file ...
                        else
                            print_if_possible(fgIgnoredfile, string(".\"") + modifiedfile.Relative(texdir) + "\" modified\n" );
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

    clean:
        for(size_t i=0; i<nWdi;i++) {
            CloseHandle(watchdirs[i]->overl.hEvent);
            CloseHandle(watchdirs[i]->hDir);
            delete watchdirs[i];
        }
        delete hp;


        delete dg_deps;
        delete dg_preamb_deps;
        hWatchingThread = NULL;
    }
}

