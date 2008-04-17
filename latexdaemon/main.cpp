// Copyright William Blum 2007 (http://william.famille-blum.org/software/index.html)
// Created in September 2006
#define APP_NAME		_T("LatexDaemon")
#define VERSION_DATE	__DATE__
#define VERSION			0.9
#define BUILD			_T("33")


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

#define _WIN32_WINNT  0x0500
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

#include "tstring.h"


//////////
/// Constants 



#define DEFAULTPREAMBLE2_BASENAME       _T("preamble")
#define DEFAULTPREAMBLE2_FILENAME       DEFAULTPREAMBLE2_BASENAME _T(".tex")
#define DEFAULTPREAMBLE1_EXT            _T("pre")

#define PROMPT_STRING					_T("dameon>")
#define PROMPT_STRING_WATCH				_T("dameon@>")


// Maximal length of an input command line at the prompt
#define PROMPT_MAX_INPUT_LENGTH			2*_MAX_PATH


// result of timestamp comparison
#define OUT_FRESHER	   0x00
#define SRC_FRESHER	   0x01
#define ERR_OUTABSENT  0x02
#define ERR_SRCABSENT  0x04

// constants corresponding to the different possible jobs that can be exectuted
enum JOB { Rest = 0 , Dvips = 1, Compile = 2, FullCompile = 3, DviPng = 4, Custom = 5 } ;



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
void pwd();
bool spawn(int argc, PTSTR *argv);
int loadfile( CSimpleGlob &nglob, JOB initialjob );
bool RestartMakeThread(JOB makejob);
DWORD fullcompile();
DWORD compile();
int dvips(tstring opt);
int dvipng(tstring opt);
int custom(tstring opt);
int shell(tstring cmdline);
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
#define REGKEY_GSVIEW32_PATH _T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\gsview32.exe")
#define REGKEY_GSVIEW64_PATH _T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\gsview64.exe")

#define DEFAULT_GSVIEW32_PATH _T("c:\\program files\\ghostgum\\gsview\\gsview32.exe")
#define DEFAULT_GSVIEW64_PATH _T("c:\\program files\\ghostgum\\gsview\\gsview64.exe")

// path to gsview32.exe ou gsview64.exe
tstring gsview;

// by default, gswin32 is not used as a viewer: the program associated with the output file will be used (adobe reader, gsview, ...)
bool UseGswin32 = false;

// process/thread id of the last instance launched of gsview
PROCESS_INFORMATION piGsview = {NULL, NULL};
// handle of the main window of gsview
HWND hwndGsview = NULL;

// Tex initialization file (can be specified as a command line parameter)
tstring texinifile = _T("latex"); // use latex by default

// preamble file name and basename
tstring preamble_filename = _T("");
tstring preamble_basename = _T("");

// Path where the main tex file resides
tstring texdir = _T("");
tstring texfullpath = _T("");
tstring texbasename = _T("");

// name of the backup file for the .aux file
tstring auxbackupname = _T("");

// by default the output extension is set to ".dvi", it must be changed to ".pdf" if pdflatex is used
tstring output_ext = _T(".dvi");


// Default command line arguments for Tex
tstring texoptions = _T(" -interaction=nonstopmode --src-specials "); // -max-print-line=120 

// custom command line
tstring customcmd = _T("");

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
    TCHAR szPath[_MAX_PATH];
    OVERLAPPED overl;
    BYTE buffer [2][512*sizeof(FILE_NOTIFY_INFORMATION )];
    int curBuffer;
} WATCHDIRINFO ;


// define the ID values to indentify the option
enum { 
	// command line options
	OPT_USAGE, OPT_INI, OPT_WATCH, OPT_FORCE, OPT_PREAMBLE, OPT_AFTERJOB, 
	// prompt commands
	OPT_HELP, OPT_COMPILE, OPT_FULLCOMPILE, OPT_QUIT, OPT_BIBTEX, OPT_DVIPS, OPT_DVIPNG, OPT_RUN,
    OPT_CUSTOM, OPT_PWD,
	OPT_PS2PDF, OPT_EDIT, OPT_VIEWOUTPUT, OPT_OPENFOLDER, OPT_LOAD, OPT_SPAWN,
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
    { OPT_CUSTOM,		_T("-custom"),	    SO_REQ_CMB },
    { OPT_CUSTOM,		_T("--custom"),	    SO_REQ_CMB },
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
    { OPT_DVIPNG,		_T("-dvipng"),	    SO_NONE		},
    { OPT_RUN,		    _T("-r"),	        SO_NONE		},
    { OPT_RUN,		    _T("-run"),	        SO_NONE		},
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
    { OPT_CUSTOM,		_T("-custom"),	    SO_REQ_CMB  },    
    { OPT_GSVIEW,		_T("-gsview"),		SO_REQ_CMB  },
	{ OPT_PWD,			_T("-pwd"),			SO_NONE		},
	{ OPT_QUIT,			_T("-q"),			SO_NONE		},
	{ OPT_QUIT,			_T("-quit"),		SO_NONE		},
	{ OPT_QUIT,			_T("-exit"),		SO_NONE		},
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
	{ OPT_OPENFOLDER,	_T("-x"),			SO_NONE		},
	{ OPT_OPENFOLDER,	_T("-explorer"),	SO_NONE		},
	{ OPT_LOAD,			_T("-l"),			SO_NONE		},
	{ OPT_LOAD,			_T("-load"),		SO_NONE		},
	{ OPT_SPAWN,		_T("-s"),			SO_NONE		},
	{ OPT_SPAWN,		_T("-spawn"),		SO_NONE		},
    SO_END_OF_OPTIONS                   // END
};



////////////////////




// show the usage of this program
void ShowUsage(TCHAR *progname) {
    tcout << "USAGE: latexdameon [options] mainfile.tex [dependencies]" <<endl
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
         << " --afterjob={dvips|dvipng|custom|rest}" << endl 
         << "   Specifies what should be done after a successful compilation of the .tex file." << endl 
         << "     . 'dvips' run dvips on the output dvi file," <<endl
         << "     . 'dvipng' run dvipng on the output dvi file," <<endl
         << "     . 'custom' run a custom command line specified with the '-custom' option," <<endl
         << "     . 'rest' (default) do nothing."<<endl
         << " --custom=\"COMMAND LINE\"" << endl 
         << "   Specifies a command line to execute when afterjob is set to the value 'custom'." << endl 
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
void SetTitle(tstring state)
{
    if( texfullpath != _T("") )
        SetConsoleTitle((state + _T(" - ")+ texinifile + _T("Daemon - ") + texfullpath).c_str());
    else
        SetConsoleTitle((state + _T(" - ") + texinifile + _T("Daemon")).c_str());
}



// Print a string in a given color only if the output is not already occupied, otherwise do nothing
void print_if_possible( std::tostream& color( std::tostream&  stream ) , tstring str)
{
    if( TryEnterCriticalSection(&cs) ) {
        tcout << color << str << fgPrompt;
        LeaveCriticalSection(&cs);
    }
}

// Read a list of filenames from the file 'filename' and store them in the list 'deps'
void ReadDependencies(tstring filename, vector<CFilename> &deps)
{
    tifstream depFile;
    depFile.open(filename.c_str());
    if(depFile) {        
        TCHAR line[_MAX_PATH];
        while(!depFile.eof()){
            depFile.getline(line,_MAX_PATH);
            if( line[0] != '\0' ) {
                if( NULL == GetFileExtPart(line, 0, NULL) ) {
                    _tcscat_s(line,_MAX_PATH, _T(".tex"));
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
        bfound = find( pv2.begin(), pv2.end(), *it1 ) != pv2.end();
    }
    return bfound ? 0 : 1;
}

// Refresh the list of automatic dependencies
void RefreshDependencies(bool bPreamble) {
    bool depChanged = false;

    vector<CFilename> new_deps;
    ReadDependencies(texbasename+_T(".dep"), new_deps);
    // Compare new_deps and auto_deps
    depChanged = 0 != CompareDeps(new_deps,auto_deps);
    
    vector<CFilename> new_preamb_deps;
    if( bPreamble ) {
        ReadDependencies(texbasename+_T("-preamble.dep"), new_preamb_deps);
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

//
bool IsRunning_WatchingThread()
{
    return hWatchingThread && ( WaitForSingleObject(hWatchingThread, 0) == WAIT_TIMEOUT );
}

void StopAndWaitUntilEnd_WatchingThread()
{
    SetEvent(hEvtStopWatching);
    WaitForSingleObject(hWatchingThread, INFINITE);
    CloseHandle(hWatchingThread);
    hWatchingThread = NULL;
}


// Code executed when the -ini option is specified
void ExecuteOptionIni( tstring optionarg )
{
    texinifile = optionarg;
    SetTitle(_T("monitoring"));
    tcout << fgNormal << "-Initialization file set to \"" << texinifile << "\"" << endl;;
    if( texinifile == _T("pdflatex") || texinifile == _T("pdftex") )
        output_ext = _T(".pdf");
    else if ( texinifile == _T("latex") || texinifile == _T("tex") )
        output_ext = _T(".dvi");
}
// Code executed when the -preamble option is specified
void ExecuteOptionPreamble( tstring optionarg )
{
    EnterCriticalSection( &cs );
    LookForExternalPreamble = (optionarg==_T("none")) ? false : true ;
    if( LookForExternalPreamble )
        tcout << fgNormal << "-I will look for an external preamble file next time I load a .tex file." << endl;
    else
        tcout << fgNormal << "-I will not look for an external preamble next time I load a .tex file." << endl;
    LeaveCriticalSection( &cs );
}

// Code executed when the -force option is specified
void ExecuteOptionForce( tstring optionarg, JOB &force )
{
    EnterCriticalSection( &cs );
    if( optionarg==_T("fullcompile") ) {
        force = FullCompile;
        tcout << fgNormal << "-Initial full compilation forced." << endl;
    }
    else {
        force = Compile;
        tcout << fgNormal << "-Initial compilation forced." << endl;
    }
    LeaveCriticalSection( &cs );
}

// code for the -gsview option
void ExecuteOptionGsview( tstring optionarg )
{
	EnterCriticalSection( &cs );
    UseGswin32 = optionarg!=_T("no");
    if( UseGswin32 )
    	tcout << fgNormal << "-PS/PDF viewer set to GhostView." << endl;
	else
    	tcout << fgNormal << "-PS/PDF files will be viewed with the default associated application." << endl;
	LeaveCriticalSection( &cs );
}

// Code executed when the -afterjob option is specified
void ExecuteOptionAfterJob( tstring optionarg )
{
	EnterCriticalSection( &cs );
	if( optionarg==_T("dvips") )
		afterjob = Dvips;
    else if( optionarg==_T("dvipng") )
		afterjob = DviPng;
    else if( optionarg==_T("custom") )
        afterjob = Custom;
	else {
        optionarg = _T("rest");
		afterjob = Rest;
	}
    tcout << fgNormal << "-After-compilation job set to '" << optionarg << "'" << endl;
	LeaveCriticalSection(&cs);
}

// Code executed when the -custom option is specified
void ExecuteOptionCustom( tstring optionarg )
{
	EnterCriticalSection( &cs );
	customcmd = optionarg;
    tcout << fgNormal << "-Custom command set to '" << customcmd << "'" << endl;
	LeaveCriticalSection(&cs);
}

// Code executed when the -watch option is specified
void ExecuteOptionWatch( tstring optionarg )
{
	Watch = optionarg != _T("no");
	if( Watch ) {
		EnterCriticalSection( &cs );
		tcout << fgNormal << "-File modification monitoring activated" << endl;
		LeaveCriticalSection( &cs );
		if( FileLoaded )
			RestartWatchingThread();
	}
	else {
		EnterCriticalSection( &cs );
		tcout << fgNormal << "-File modification monitoring disabled" << endl;
		LeaveCriticalSection( &cs );
		// Stop the watching thread
        StopAndWaitUntilEnd_WatchingThread();
	}
}

// Code executed when the -filter option is specified
void ExecuteOptionOutputFilter( tstring optionarg )
{
    tstring filtermessage;
    if( optionarg == _T("err") ) {
        Filter = ErrOnly;
        filtermessage = _T("show error messages only");
    }
    else if( optionarg == _T("warn") ) {
        Filter = WarnOnly;
        filtermessage = _T("show warning messages only");
    }
    else if( optionarg == _T("err+warn") || optionarg == _T("warn+err") ) {
        Filter = ErrWarnOnly;
        filtermessage = _T("show error and warning messages only");
    }
    else if( optionarg == _T("raw") ) {
        Filter = Raw;
        filtermessage = _T("raw");
    }
    else { // if( optionarg == "highlight" ) {
        Filter = Highlight;
        filtermessage = _T("highlight error and warning messages");
    }

    EnterCriticalSection( &cs );
    tcout << fgNormal << "-Latex output: " << filtermessage << endl;
    LeaveCriticalSection( &cs );
}

// Code executed when the -autodep option is specified
void ExecuteOptionAutoDependencies( tstring optionarg )
{
    Autodep = optionarg != _T("no");
    EnterCriticalSection( &cs );
    if( Autodep )
        tcout << fgNormal << "-Automatic dependency detection activated" << endl;
    else
        tcout << fgNormal << "-Automatic dependency detection disabled" << endl;
    LeaveCriticalSection( &cs );
}


// perform the necessary compilation
DWORD make(JOB makejob)
{
	DWORD ret;

	if( makejob == Rest )
		return 0;
	else if( makejob == Compile ) {
		SetTitle(_T("recompiling..."));
		ret = compile();
	}
	else if ( makejob == FullCompile ) {
		SetTitle(_T("recompiling..."));
		ret = fullcompile();
	}
	if( ret==0 ) {
		if( afterjob == Dvips )
			ret = dvips(_T(""));
        else if ( afterjob == DviPng )
			ret = dvipng(_T(""));
        else if ( afterjob == Custom )
            ret = custom(_T(""));
		
		// has gswin32 been launched?
		if( piGsview.dwProcessId ) {
			// look for the gswin32 window
			if( !hwndGsview )
				EnumThreadWindows(piGsview.dwThreadId, LookForGsviewWindow, NULL);
			// refresh the gsview window 
			if( hwndGsview )
				PostMessage(hwndGsview, WM_KEYDOWN, VK_F5, 0xC03F0001);
		}
	}

	SetTitle( ret ? _T("(errors) monitoring") : _T("monitoring"));
	return ret;
}



// thread responsible of launching the external commands (latex) for compilation
void WINAPI MakeThread( void *param )
{
    //////////////
    // perform the necessary compilations
    MAKETHREADPARAM *p = (MAKETHREADPARAM *)param;

    if( p->makejob == Compile ) {
        // Make a copy of the .aux file (in case the latex compilation aborts and destroy the .aux file)
        CopyFile((texdir+texbasename+_T(".aux")).c_str(), (texdir+auxbackupname).c_str(), FALSE);
    }

    DWORD errcode = make(p->makejob);

    if( p->makejob == Compile && errcode == -1) 
        // Restore the backup of the .aux file
        CopyFile((texdir+auxbackupname).c_str(), (texdir+texbasename+_T(".aux")).c_str(), FALSE);
    
    // restore the prompt    
    print_if_possible(fgPrompt, IsRunning_WatchingThread() ? PROMPT_STRING_WATCH : PROMPT_STRING );

    if( errcode == 0 && Autodep ) 
        // Refresh the list of dependencies
        RefreshDependencies(p->makejob == FullCompile);

    free(p);
}




void RestartWatchingThread(){
    // if the thread already exists then stop it
    if( IsRunning_WatchingThread() )
        StopAndWaitUntilEnd_WatchingThread();

    SetTitle(_T("monitoring"));

    DWORD watchingthreadID;

    // reset the hEvtStopWatching event so that it can be set if
    // some thread requires the watching thread to stop
    ResetEvent(hEvtStopWatching);

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
		 return FALSE;
	 } else {
		 return TRUE;
	 }
}

HWND MyGetConsoleHWND(void)
{
	static HWND hwndConsole = NULL;
    
    if( hwndConsole == NULL )
	    EnumWindows(FindConsoleEnumWndProc, (LPARAM)&hwndConsole);

	return hwndConsole;
}


// Function taken from Microsoft KB124103:
// http://support.microsoft.com/kb/124103
HWND GetConsoleHWND(void)
   {
       #define MY_BUFSIZE 1024 // Buffer size for console window titles.
       static HWND hwndFound = NULL;         // This is what is returned to the caller.
       TCHAR pszNewWindowTitle[MY_BUFSIZE]; // Contains fabricated
                                           // WindowTitle.
       TCHAR pszOldWindowTitle[MY_BUFSIZE]; // Contains original
                                           // WindowTitle.

       if( hwndFound == NULL ) {

           // Fetch current window title.

           GetConsoleTitle(pszOldWindowTitle, MY_BUFSIZE);

           // Format a "unique" NewWindowTitle.

           wsprintf(pszNewWindowTitle,_T("%d/%d"),
                       GetTickCount(),
                       GetCurrentProcessId());

           // Change current window title.

           SetConsoleTitle(pszNewWindowTitle);

           // Ensure window title has been updated.

           Sleep(40);

           // Look for NewWindowTitle.

           hwndFound=FindWindow(NULL, pszNewWindowTitle);

           // Restore original window title.

           SetConsoleTitle(pszOldWindowTitle);
       }
       return(hwndFound);
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
        // wait for the "make" thread to end if it is running
        if(hMakeThread)
            WaitForSingleObject(hMakeThread, INFINITE);

        if( printprompt )
            print_if_possible(fgPrompt, IsRunning_WatchingThread() ? PROMPT_STRING_WATCH : PROMPT_STRING);
        printprompt = true;

        // Read a command from the user
#define DUMMYCMD        _T("dummy -")
        TCHAR cmdline[PROMPT_MAX_INPUT_LENGTH+_countof(DUMMYCMD)] = DUMMYCMD; // add a dummy command name and the option delimiter
        tcin.getline(&cmdline[_countof(DUMMYCMD)-1], PROMPT_MAX_INPUT_LENGTH);

        // Convert the command line into an argv table 
        int argc;
        PTSTR *argv;
        argv = CommandLineToArgv(cmdline, &argc);
        // Parse the command line
        CSimpleOpt args(argc, argv, g_rgPromptOptions, true);

        args.Next(); // get the first command recognized
        if (args.LastError() != SO_SUCCESS) {
            // Set the beginning of the error message
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
            // set the end of error message
            EnterCriticalSection( &cs );
                LPCTSTR optiontext = args.OptionText();
                // remove the extra '-' that we have appened before the command name
                if( args.LastError()== SO_OPT_INVALID && optiontext[0] == '-') 
                    optiontext++;
                // don't show error message for the empty command
                if(  optiontext[0] != 0 ) {
                    tcout << fgErr << pszError << ": '" << optiontext << "' (use help to get command line help)"  << endl;
                }
            LeaveCriticalSection( &cs );
            continue;
        }

        switch( args.OptionId() ) {
        case OPT_USAGE:
            EnterCriticalSection( &cs );
                tcout << fgNormal;
                ShowUsage(NULL);
            LeaveCriticalSection( &cs ); 
            break;
        case OPT_HELP:
            EnterCriticalSection( &cs );
            tcout << fgNormal << "The following commands are available:" << endl
                 << "Latex related commands:" << endl
                 << "  b[ibtex]           run bibtex on the .tex file" << endl
                 << "  c[compile]         compile the .tex file using the precompiled preamble" << endl
                 << "  d[vips]            convert the .dvi file to postscript" << endl
                 << "  dvipng             convert the .dvi file to PNG" << endl
                 << "  r[un] COMMANDLINE  execute a shell command" << endl
                 << "  e[dit]             edit the .tex file" << endl
                 << "  f[ullcompile]      compile the preamble and the .tex file" << endl
                 << "  p[s2pdf]           convert the .ps file to pdf" << endl
                 << "  v[iew]             view the output file (dvi or pdf depending on ini value)" << endl
                 << "  vi|viewdvi         view the .dvi output file" << endl 
                 << "  vs|viewps          view the .ps output file" << endl 
                 << "  vf|viewpdf         view the .pdf output file" << endl 
                 << "File managment commands:" << endl
                 << "  l[oad] [file.tex]  change the active tex document" << endl
                 << "  s[pawn] [file.tex] spawn a new latexdaemon process" << endl
                 << "  pwd                print working document/directory" << endl
                 << "  x|explore          explore the folder containing the .tex file" << endl
                 << "Others:" << endl
                 << "  u[sage]            show the help on command line parameters usage" << endl
                 << "  h[elp]             show this message" << endl
                 << "  q[uit]             quit the program" << endl << endl
                 << "Options can be configured using the following commands:" << endl
                 << "  ini=inifile               set the initial format file" << endl
                 << "  preamble={none,external}  set the preamble mode (reload file to apply)" << endl
                 << "  autodep={yes,no}          activate/deactivate automatic dependency detection" << endl
                 << "  watch={yes,no}            activate/deactivate file modification watching" << endl 
                 << "  gsview={yes,no}           set GSView as the default viewer (will enable auto-refresh)" << endl
                 << "  afterjob={rest,dvips,dvipng,custom} set a job to be executed after latex compilation" << endl
                 << "  custom=\"COMMAND LINE\"     set custom command to execute when afterjob='custom'" << endl
                 << "  filter={highlight|raw|err|warn|err+warn}  set the error messages filter mode (highlight by default)." << endl << endl;
            LeaveCriticalSection( &cs ); 
            break;
        case OPT_INI:			ExecuteOptionIni(args.OptionArg());              break;
        case OPT_PREAMBLE:		ExecuteOptionPreamble(args.OptionArg());         break;
        case OPT_AFTERJOB:		ExecuteOptionAfterJob(args.OptionArg());         break;
        case OPT_CUSTOM:		ExecuteOptionCustom(args.OptionArg());         break;
        case OPT_WATCH:			ExecuteOptionWatch(args.OptionArg());            break;
        case OPT_AUTODEP:		ExecuteOptionAutoDependencies(args.OptionArg()); break;
        case OPT_FILTER:        ExecuteOptionOutputFilter(args.OptionArg());     break;
        case OPT_GSVIEW:        {PTSTR p=args.OptionArg(); ExecuteOptionGsview(p?p:_T(""));} break;
        case OPT_BIBTEX:		bibtex();		break;
        case OPT_PS2PDF:		ps2pdf();		break;
        case OPT_EDIT:			edit();			break;
        case OPT_VIEWOUTPUT:	view();			break;
        case OPT_VIEWDVI:		view_dvi();		break;
        case OPT_VIEWPS:		view_ps();		break;
        case OPT_VIEWPDF:		view_pdf();		break;
        case OPT_OPENFOLDER:	openfolder();	break;
        case OPT_PWD:           pwd();          break;

        case OPT_RUN:
            {
                tstring cmd;
                for(int i=2;i<argc;i++)
                    cmd+=tstring(argv[i]) + _T(" ");
                shell(cmd);
            }
            break;
        case OPT_DVIPS:
            {
                tstring opt;
                for(int i=2;i<argc;i++)
                    opt+=tstring(argv[i]) + _T(" ");
                dvips(opt);
            }
            break;

        case OPT_DVIPNG:
            {
                tstring opt;
                for(int i=2;i<argc;i++)
                    opt+=tstring(argv[i]) + _T(" ");
                dvipng(opt);
            }
            break;

        case OPT_SPAWN: {
            // parse the filenames parameters (tex file name and dependencies)
            CSimpleGlob nglob;
            if (SG_SUCCESS != nglob.Add(args.FileCount(), args.Files()) ) {
                tcout << fgErr << _T("Error while globbing files! Make sure that the given path is correct.\n" << fgNormal) ;
            }
            else
                spawn(argc-1, &argv[1]);
            break;
        }

        case OPT_LOAD:
            {
                // parse the filenames parameters (tex file name and dependencies)
                CSimpleGlob nglob;
                if (SG_SUCCESS != nglob.Add(args.FileCount(), args.Files()) ) {
                    tcout << fgErr << _T("Error while globbing files! Make sure that the given path is correct.\n" << fgNormal) ;
                }
                else {
                    // if no argument is specified then we show a dialogbox to the user to let him select a file
                    if( args.FileCount() == 0 ){
                        OPENFILENAME ofn;       // common dialog box structure
                        TCHAR szFile[260];       // buffer for file name

                        // Initialize OPENFILENAME
                        ZeroMemory(&ofn, sizeof(ofn));
                        ofn.lStructSize = sizeof(ofn);
                        ofn.hwndOwner = GetConsoleHWND();
                        ofn.lpstrFile = szFile;
                        //
                        // Set lpstrFile[0] to '\0' so that GetOpenFileName does not 
                        // use the contents of szFile to initialize itself.
                        //
                        ofn.lpstrFile[0] = '\0';
                        ofn.nMaxFile = sizeof(szFile);
                        ofn.lpstrFilter = _T("All(*.*)\0*.*\0Tex/Latex file (*.tex)\0*.tex\0");
                        ofn.nFilterIndex = 2;
                        ofn.lpstrFileTitle = NULL;
                        ofn.nMaxFileTitle = 0;
                        ofn.lpstrInitialDir = NULL;
                        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

                        // Display the Open dialog box. 

                        if (GetOpenFileName(&ofn)==TRUE) {
                            if (SG_SUCCESS != nglob.Add(szFile) ) {
                                tcout << fgErr << _T("Error while globbing files! Make sure that the given path is correct.\n" << fgNormal) ;
                                goto err_load;
                            }
                        }
                        else {
                            tcout << fgErr << _T("No input file specified!\n" << fgNormal);
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
            tcout << fgNormal;
            break;		

        default:
            EnterCriticalSection( &cs );
            tcout << fgErr << "Unrecognized command: '" << args.OptionText() << "' (use help to get command line help)" << endl ;
            LeaveCriticalSection( &cs );
            printprompt = true;
            break;
        }

    }

    // Preparing to exit the program: ask the children thread to terminate
    HANDLE hp[2];
    int i=0;
    if( hWatchingThread ) {
        // send a message to the stop the watching thread
        SetEvent(hEvtStopWatching);
        hp[i++] = hWatchingThread;
    }
    if( hMakeThread ) {
        // send a message to stop the make thread
        SetEvent(hEvtAbortMake);
        hp[i++] = hMakeThread;
    }
    // wait for the two threads to end
    WaitForMultipleObjects(i, hp, TRUE, INFINITE);
    CloseHandle(hMakeThread);
    hMakeThread = NULL;
    CloseHandle(hWatchingThread);
    hWatchingThread = NULL;

}

typedef BOOL (WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);

LPFN_ISWOW64PROCESS fnIsWow64Process;

BOOL IsWow64()
{
    BOOL bIsWow64 = FALSE;

    fnIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress(
        GetModuleHandle(TEXT("kernel32")),"IsWow64Process");
  
    if (NULL != fnIsWow64Process)
    {
        if (!fnIsWow64Process(GetCurrentProcess(),&bIsWow64))
        {
            // handle error
        }
    }
    return bIsWow64;
}

int _tmain(int argc, TCHAR *argv[])
{

    SetTitle(_T("prompt"));

    tcout << endl << fgNormal << APP_NAME << _T(" ") << VERSION << _T(" Build ") << BUILD << _T(" by William Blum, ") << VERSION_DATE << endl << endl;;

    // look for gsview
    HKEY hkey;
    TCHAR buff[_MAX_PATH];
    DWORD size = _countof(buff);
    if( ERROR_SUCCESS == RegOpenKeyEx(HKEY_LOCAL_MACHINE, REGKEY_GSVIEW32_PATH, 0, KEY_QUERY_VALUE, &hkey) ) {
        RegQueryValueEx(hkey,NULL,NULL,NULL,(LPBYTE)buff,&size);
        RegCloseKey(hkey);
        gsview = buff;
    }
    else if( ERROR_SUCCESS == RegOpenKeyEx(HKEY_LOCAL_MACHINE, REGKEY_GSVIEW64_PATH, 0, KEY_QUERY_VALUE, &hkey) ) {
        RegQueryValueEx(hkey,NULL,NULL,NULL,(LPBYTE)buff,&size);
        RegCloseKey(hkey);
        gsview = buff;
    }
    else { // path by default
        // running a x86 version of windows?
        // if( IsWow64() )
        GetEnvironmentVariable(_T("PROCESSOR_ARCHITECTURE"), buff, _countof(buff));
        if(_tcscmp(buff, _T("x86")) != 0)
            gsview = DEFAULT_GSVIEW64_PATH;
        else
            gsview = DEFAULT_GSVIEW32_PATH;
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
            case OPT_GSVIEW:        ExecuteOptionGsview(_T(""));                        break;
            case OPT_PREAMBLE:      ExecuteOptionPreamble(args.OptionArg());            break;
            case OPT_AFTERJOB:      ExecuteOptionAfterJob(args.OptionArg());            break;
            case OPT_CUSTOM:        ExecuteOptionCustom(args.OptionArg());              break;
            default:                break;
        }
        uiFlags |= (unsigned int) args.OptionId();
    }

    int ret = -1;    
    if( args.FileCount() == 0 ){
        tcout << fgWarning << _T("No input file specified.\n") << fgNormal;
    }
    else {
        CSimpleGlob nglob(uiFlags);
        if (SG_SUCCESS != nglob.Add(args.FileCount(), args.Files()) ) {
            tcout << fgErr << _T("Error while globbing files! Make sure that the given path is correct.\n") << fgNormal ;
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
    struct _stat buffer ;
    return 0 == _tstat( filename, &buffer );
}

// print the current directory and document
void pwd()
{
	EnterCriticalSection( &cs );
    if(texdir!= _T("") && texbasename!=_T("")) {
        tcout << fgNormal << "Current directory: " << texdir << endl
            << "Current document: " << texbasename << ".tex" << endl;
    }
    else {
        TCHAR path[_MAX_PATH];
        GetCurrentDirectory(_countof(path), path);
        tcout << fgNormal << "Current directory: " << path << endl
             << "No document opened." << endl;
    }
    LeaveCriticalSection( &cs );
}

// spawn a new latexdaemon process
// the (argc,argv) pair gives the parameters that must be given to the spawn process. argv[0] can be set to a dummy value.
bool spawn(int argc, PTSTR *argv)
{
    TCHAR exepath[_MAX_PATH];
    GetModuleFileName(NULL, exepath, _countof(exepath));

    tstring cmdline = tstring(exepath);
    cmdline += _T(" -ini=");
    cmdline += texinifile;
    for(int i = 1; i<argc; i++) // skip the dummy program name
        cmdline += tstring(_T(" ")) + argv[i];

    // spawn the process
    LPTSTR szCmdline= _tcsdup(cmdline.c_str());
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    ZeroMemory( &si, sizeof(si) );
    si.cb = sizeof(si);
    ZeroMemory( &pi, sizeof(pi) );
    bool bret = CreateProcess( NULL,   // No module name (use command line)
        szCmdline,      // Command line
        NULL,           // Process handle not inheritable
        NULL,           // Thread handle not inheritable
        FALSE,          // Set handle inheritance to FALSE
        CREATE_NEW_CONSOLE,              // No creation flags
        NULL,           // Use parent's environment block
        NULL,           // Use parent's starting directory 
        &si,            // Pointer to STARTUPINFO structure
        &pi ) ? true : false;          // Pointer to PROCESS_INFORMATION structure
    
    if( bret ) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    else {
        EnterCriticalSection( &cs );
        tcout << fgErr << "CreateProcess failed ("<< GetLastError() << ") : " << cmdline <<".\n" << fgNormal;
        LeaveCriticalSection( &cs ); 
    }

    free(szCmdline);
    return bret;
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

    _tfullpath( fullpath, depglob.File(0), _MAX_PATH );
    _tsplitpath( fullpath, drive, dir, mainfile, ext );

    if(  _tcsncmp(ext, _T(".tex"), 4) )	{
        tcerr << fgErr << "Error: this file does not seem to be a TeX document!\n\n" << fgNormal;
        return 2;
    }

    // set the global variables
    texfullpath = fullpath;
    texdir = tstring(drive) + dir;
    texbasename = mainfile;
    auxbackupname = texbasename+_T(".aux.bak");

    // set console title
    SetTitle(_T("Initialization"));
    tcout << "-Main file: '" << fullpath << "'\n";
    tcout << "-Directory: " << drive << dir << "\n";
    
    FileLoaded = false; // will be set to true when the loading is finished


    // clear the dependency list
    auto_deps.clear();
    auto_preamb_deps.clear();
    static_deps.clear();


    if( depglob.FileCount()>1 ) 
        tcout << "-Dependencies manually added:\n";
    else
        tcout << "-No additional dependency specified.\n";

    // Add the static dependencies. We change all the relative paths to make them relative to the main .tex file dir
    TCHAR tmpfullpath[_MAX_PATH];
          //tmprelpath[_MAX_PATH];
    for(int i=0; i<depglob.FileCount(); i++) {
        if( i > 0 ) _tprintf(_T("  %2d: '%s'\n"), i, depglob.File(i));
        _tfullpath( tmpfullpath, depglob.File(i), _MAX_PATH );
        //Abs2Rel(tmpfullpath, tmprelpath, _MAX_PATH, texdir.c_str());
        static_deps.push_back(CFilename(tmpfullpath));
    }

    // change current directory
    _tchdir(texdir.c_str());

    // check for the presence of the external preamble file
    if( LookForExternalPreamble ) {
        // compare the timestamp of the preamble.tex file and the format file
        preamble_filename = tstring(mainfile) + _T(".") DEFAULTPREAMBLE1_EXT;
        preamble_basename = tstring(mainfile);
        ExternalPreamblePresent = FileExists(preamble_filename.c_str());
        if ( !ExternalPreamblePresent ) {
            // try with the second default preamble name
            preamble_filename = DEFAULTPREAMBLE2_FILENAME;
            preamble_basename = DEFAULTPREAMBLE2_BASENAME;
            ExternalPreamblePresent = FileExists(preamble_filename.c_str());
            if ( !ExternalPreamblePresent ) {
	            tcout << fgWarning << "Warning: Preamble file not found! (I have looked for " << mainfile << "." << DEFAULTPREAMBLE1_EXT << " and " << DEFAULTPREAMBLE2_FILENAME << ")\nPrecompilation mode deactivated!\n";
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
        ReadDependencies(texbasename+_T("-preamble.dep"), auto_preamb_deps);
        tcout << _T("-Preamble file: ") << preamble_filename << _T("\n");

        // compare the timestamp of the preamble file with the format file
        int res = compare_timestamp(preamble_filename.c_str(), (preamble_basename+_T(".fmt")).c_str());

        // If the format file is older than the preamble file
        if( res == SRC_FRESHER ) {
            // then it is necessary to recreate the format file and to perform a full recompilation
            job = FullCompile;
            tcout << fgMsg << "+ " << preamble_filename << " has been modified since last run.\n";
        }
        // if the format file does not exist
        else if ( res & ERR_OUTABSENT ) {
            job = FullCompile;
            tcout << fgMsg << "+ " << preamble_basename << ".fmt does not exist. Let's create it...\n";
        }


        // Check if some preamble dependency has been tooched
        if( job != FullCompile )
            for(vector<CFilename>::iterator it = auto_preamb_deps.begin();
                it!= auto_preamb_deps.end(); it++) {
                res = compare_timestamp(it->c_str(), (preamble_basename+_T(".fmt")).c_str());
                if( res == SRC_FRESHER ) {
                    tcout << fgMsg << "+ Preamble dependency modified since last run: " << it->c_str() << "\n";
                    job = FullCompile;
                }
            }
    }

    
    // Initialize the .tex file dependency list
    ReadDependencies(texbasename+_T(".dep"), auto_deps);

    // If the preamble file does not need to be recompiled
    // and if compilation of the .tex file is not requested by the user
    if( job != Compile && job != FullCompile) {

        ///////////////////
        // Check if the .tex file dependencies have been touched since last compilation
        
        // check if the main file has been modified since the creation of the dvi file
        int maintex_comp = compare_timestamp((texbasename+_T(".tex")).c_str(), (texbasename+output_ext).c_str());

        if ( maintex_comp & ERR_SRCABSENT ) {
            tcout << fgErr << "File " << mainfile << ".tex not found!\n" << fgNormal;
            return 3;
        }
        else if ( maintex_comp & ERR_OUTABSENT ) {
            tcout << fgMsg << "+ " << mainfile << output_ext << " does not exist. Let's create it...\n";
            job = Compile;
        }
        else if( maintex_comp == SRC_FRESHER ) {
            tcout << fgMsg << "+ the main .tex file has been modified since last run. Let's recompile...\n";
            job = Compile;
        }
        else { // maintex_comp == OUT_FRESHER 

            // check if some manual dependency file has been modified since the creation of the dvi file
            for(vector<CFilename>::iterator it = static_deps.begin(); it!=static_deps.end(); it++)                
                if( SRC_FRESHER == compare_timestamp(it->c_str(), (texbasename+output_ext).c_str()) ) {
                    tcout << fgMsg << _T("+ The file ") << it->Relative(texdir.c_str()) << ", on which the main .tex depends has been modified since last run. Let's recompile...\n";
                    job = Compile;
                    break;
                }

            // Check if some automatic dependencies has been touched
            if( job != FullCompile )
                for(vector<CFilename>::iterator it = auto_deps.begin();
                    it!= auto_deps.end(); it++)
                    if( SRC_FRESHER == compare_timestamp(it->c_str(), (texbasename+output_ext).c_str()) ) {
                        tcout << fgMsg << "+ The file " << it->c_str() << ", on which the main .tex depends has been modified since last run. Let's recompile...\n";                        
                        job = Compile;
                    }
        }
    }


    // Perform the job that needs to be done
    if( job != Rest ) {
        if( job == FullCompile )
            tcout << fgMsg << "  Let's recreate the format file and then recompile " << texbasename << ".tex.\n";

        make(job);
    }

    FileLoaded = true;

    return 0;
}



// compare the time stamp of source and target files
int compare_timestamp(LPCTSTR sourcefile, LPCTSTR outputfile)
{
    struct _stat attr_src, attr_out;			// file attribute structures
    int res_src, res_out;

    res_src = _tstat(sourcefile, &attr_src);
    res_out = _tstat(outputfile, &attr_out);
    if( res_src || res_out ) // problem when getting the attributes of the files?
        return  (res_src ? ERR_SRCABSENT : 0) | (res_out ? ERR_OUTABSENT : 0);
    else
        return ( difftime(attr_out.st_mtime, attr_src.st_mtime) > 0 ) ? OUT_FRESHER : SRC_FRESHER;
}



// Launch an external program and wait for its termination or for an abortion
// (this happens when the event hEvtAbortMake is signaled)
DWORD launch_and_wait(LPCTSTR cmdline, FILTER filt)
{
    DWORD dwRet = 0;
    LPTSTR szCmdline= _tcsdup(cmdline);

    tostreamLatexFilter filtstream(tcout.rdbuf(), filt);
    tostream *pRedirStream = (filt != Raw) ? &filtstream : NULL;
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
        if(!pRedirStream) LeaveCriticalSection( &cs );
        dwRet = GetLastError();
        EnterCriticalSection( &cs );
        tcout << fgErr << "CreateProcess failed ("<< dwRet << ") : " << cmdline <<".\n" << fgNormal;
        LeaveCriticalSection( &cs ); 
    }

    free(szCmdline);
    return dwRet;
}

// Check that a file has been loaded
bool CheckFileLoaded()
{
	if( texbasename == _T("") ) {
		EnterCriticalSection( &cs );
		tcout << fgErr << "You first need to load a .tex file!\n" << fgNormal;
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
        tstring latex_pre, latex_post;
        if( Autodep ) {
            latex_pre =
                _T("\\edef\\TheAtCode{\\the\\catcode`\\@} \\catcode`\\@=11")
                _T(" \\newwrite\\preambledepfile")
                _T("  \\immediate\\openout\\preambledepfile = ") + texbasename + _T("-preamble.dep")

                // create a backup of the original \input command
                // (the \ifx test avoids to create a loop in case another hooking has already been set)
                _T(" \\ifx\\TEXDAEMON@@input\\@undefined\\let\\TEXDAEMON@@input\\input\\fi")
                                                                                         
                // same for \include
                _T(" \\ifx\\TEXDAEMON@@include\\@undefined\\let\\TEXDAEMON@@include\\include\\fi")

                // Hook \input (when it is used with the curly bracket {..})
                _T(" \\def\\input{\\@ifnextchar\\bgroup\\Dump@input\\TEXDAEMON@@input}")
                _T(" \\def\\Dump@input#1{ \\immediate\\write\\preambledepfile{#1}\\TEXDAEMON@@input #1}")

                _T(" \\def\\include#1{\\immediate\\write\\preambledepfile{#1}\\TEXDAEMON@@include #1}")
                _T(" \\catcode`\\@=\\TheAtCode\\relax");

            latex_post = _T("\\edef\\TheAtCode{\\the\\catcode`\\@} \\catcode`\\@=11")
                         _T(" \\immediate\\closeout\\preambledepfile")     // Close the dependency file
                         _T(" \\let\\input\\TEXDAEMON@@input")             // Restore the original \input
                         _T(" \\let\\include\\TEXDAEMON@@include");        //    and \include commands.
                         _T(" \\catcode`\\@=\\TheAtCode\\relax");
        }
        else {
            latex_pre = latex_post = _T("");
        }

        EnterCriticalSection( &cs );
        tstring cmdline = tstring(_T("pdftex"))
            + texoptions + _T(" -ini \"&") + texinifile + _T("\"")
            + _T(" \"")
                + latex_pre
                + _T("\\input ") + preamble_filename 
                + latex_post
                + _T(" \\dump\\endinput")
            + _T("\"");
        tcout << fgMsg << "-- Creation of the format file...\n";
        tcout << "[running '" << cmdline << "']\n" << fgLatex;
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


    tstring texengine;  // tex engine to run
    tstring formatfile; // formatfile to preload before reading the main .tex file    
    tstring latex_pre,  // the latex code that will be executed before and after the main .tex file
            latex_post; 

    if( ExternalPreamblePresent ) {
        texengine = _T("pdftex");
        formatfile = preamble_basename;
    }
    else {
        texengine = texinifile; // 'texinifile' is equal to "latex" or "pdflatex" depending on the daemon -ini options
        formatfile = _T(""); // no format file
    }

    if ( Autodep ) {
        latex_pre =
            // change the @ catcode
            _T("\\edef\\TheAtCode{\\the\\catcode`\\@} \\catcode`\\@=11")
                // Create the .dep file
                _T(" \\newwrite\\dependfile")
                _T(" \\openout\\dependfile = ") +texbasename + _T(".dep")

                // Create a backup of the original \include command
                // (the \ifx test avoids to create a loop in case another hooking has already been set)
                _T(" \\ifx\\TEXDAEMON@ORG@include\\@undefined\\let\\TEXDAEMON@ORG@include\\include\\fi") 

                // same for input
                _T(" \\ifx\\TEXDAEMON@ORG@input\\@undefined\\let\\TEXDAEMON@ORG@input\\input\\fi")

                // Install a hook for the \include command
                _T(" \\def\\include#1{\\write\\dependfile{#1}\\TEXDAEMON@ORG@include{#1}}")

                // A hook that write the name of the included file to the dependency file
                _T(" \\def\\TEXDAEMON@DumpDep@input#1{ \\write\\dependfile{#1}\\TEXDAEMON@ORG@input #1}")

                // A hook that does nothing the first time it's being called
                // (the first call to \input{...} corresponds to the inclusion of the preamble)
                // and then behave like the preceeding hook
                _T(" \\def\\TEXDAEMON@HookIgnoreFirst@input#1{ \\let\\input\\TEXDAEMON@DumpDep@input }")

                // Install a hook for the \input command.
                + (ExternalPreamblePresent
                    ?  _T(" \\def\\input{\\@ifnextchar\\bgroup\\TEXDAEMON@HookIgnoreFirst@input\\TEXDAEMON@ORG@input}")
                    :  _T(" \\def\\input{\\@ifnextchar\\bgroup\\TEXDAEMON@DumpDep@input\\TEXDAEMON@ORG@input}"))
            
            // restore the @ original catcode
            + _T(" \\catcode`\\@=\\TheAtCode\\relax");

        latex_post = _T(" \\closeout\\dependfile");

    }
    else if( ExternalPreamblePresent ) {
        // creation of the dependency file is not required...
        // we just need to hook the first call to \input{..} in order to prevent the preamble from being loaded
        latex_pre = _T("\\edef\\TheAtCode{\\the\\catcode`\\@} \\catcode`\\@=11")
                        _T(" \\ifx\\TEXDAEMON@@input\\@undefined\\let\\TEXDAEMON@@input\\input\\fi")
                        _T(" \\def\\input{\\@ifnextchar\\bgroup\\TEXDAEMON@input\\TEXDAEMON@@input}")
                        _T(" \\def\\TEXDAEMON@input#1{\\let\\input\\TEXDAEMON@@input }")
                    _T(" \\catcode`\\@=\\TheAtCode\\relax");
        latex_post = _T("");

    }

    // There is no precompiled preamble and dependency calculation is not required therefore
    // we compile the latex file normally without loading any format file.
    else {
        latex_pre = latex_post = _T(""); // no special code to run before or after the main .tex file
    }


    ///////
    // Create the command line adding some latex code before and after the .tex file if necessary
    tstring cmdline;
    if( latex_pre == _T("") && latex_post == _T("") ) 
        cmdline = texengine + texoptions +  _T(" ") + texbasename + _T(".tex");
    else
        cmdline = texengine +
            texoptions + 
            ( formatfile!=_T("") ? _T(" \"&")+formatfile+_T("\"") : _T("") ) +
            _T(" \"") + latex_pre
                  + _T(" \\input ")+texbasename+_T(".tex ")
                  + latex_post
            + _T("\"");

    // External preamble used? Then compile using the precompiled preamble.


    EnterCriticalSection( &cs );
    tcout << fgMsg << "-- Compilation of " << texbasename << ".tex ...\n";
    tcout << fgMsg << "[running '" << cmdline << "']\n" << fgLatex;
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
        hwndGsview = FindWindowEx(hwnd, NULL, _T("gsview_img_class"), NULL);
    }
    return TRUE;
}

// start gsview, 
int start_gsview(tstring filename)
{
    tstring cmdline = gsview + _T(" ") + filename;

    STARTUPINFO si;
    LPTSTR szCmdline= _tcsdup(cmdline.c_str());
    ZeroMemory( &si, sizeof(si) );
    si.cb = sizeof(si);
    ZeroMemory( &piGsview, sizeof(piGsview) );


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
        &piGsview )           // Pointer to PROCESS_INFORMATION structure
    ) {
        EnterCriticalSection( &cs );
        tcout << fgErr << "CreateProcess failed ("<< GetLastError() << ") : " << cmdline <<".\n" << fgNormal;
        LeaveCriticalSection( &cs ); 
        free(szCmdline);
        piGsview.hProcess = piGsview.hThread = NULL;
        piGsview.dwThreadId = 0;
        return -1;
    }
    else
    {
        CloseHandle(piGsview.hProcess);
        CloseHandle(piGsview.hThread);
        hwndGsview = NULL;
        EnumThreadWindows(piGsview.dwThreadId, LookForGsviewWindow, NULL);
        free(szCmdline);
        return 0;
    }
}

// open a file (located in the same directory as the .tex file) with the program associated with its extension in windows
int shellfile_open(tstring filename)
{
    EnterCriticalSection( &cs );
    tcout << fgMsg << "-- view " << filename << " ...\n";
    LeaveCriticalSection( &cs ); 
    tstring file = texdir+filename;
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
        return start_gsview(texbasename+_T(".ps"));
    else 
        return shellfile_open(texbasename+_T(".ps"));

}

// View the output file
int view()
{
    return ( output_ext == _T(".pdf") ) ? view_pdf() : view_dvi();
}

// View the .dvi file
int view_dvi()
{
    if( !CheckFileLoaded() )
        return 0;

    tstring file=texbasename+_T(".dvi");
    EnterCriticalSection( &cs );
    tcout << fgMsg << _T("-- view ") << file << _T(" ...\n");
    LeaveCriticalSection( &cs ); 

    tstring filepath = texdir+file;
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
        return start_gsview(texbasename+_T(".pdf"));
    else 
        return shellfile_open(texbasename+_T(".pdf"));
}


// Edit the .tex file
int edit()
{
    if( !CheckFileLoaded() )
        return 0;

    EnterCriticalSection( &cs );
    tcout << fgMsg << "-- editing " << texbasename << ".tex...\n";
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
    tcout << fgMsg << "-- open directory " << texdir << " ...\n";
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
    tcout << fgMsg << "-- Converting " << texbasename << ".ps to pdf...\n";
    tstring cmdline = tstring(_T("ps2pdf "))+texbasename+_T(".ps");
    tcout << fgMsg << " Running '" << cmdline << "'\n" << fgLatex;
    LeaveCriticalSection( &cs ); 
    return launch_and_wait(cmdline.c_str());
}

// Convert the dvi file to postscript using dvips
int dvips(tstring opt)
{
    if( !CheckFileLoaded() )
        return 0;

    EnterCriticalSection( &cs );
    tcout << fgMsg << _T("-- Converting ") << texbasename << _T(".dvi to postscript...\n");
    tstring cmdline = tstring(_T("dvips "))+texbasename+_T(".dvi ") + opt + _T("-o ")+texbasename+_T(".ps");
    tcout << fgMsg << _T(" Running '") << cmdline << _T("'\n") << fgLatex;
    LeaveCriticalSection( &cs ); 
    return launch_and_wait(cmdline.c_str());
}

// Convert the dvi file to PNG using dvipng
int dvipng(tstring opt)
{
    if( !CheckFileLoaded() )
        return 0;

    EnterCriticalSection( &cs );
    tcout << fgMsg << "-- Converting " << texbasename << ".dvi to PNG...\n";
    tstring cmdline = tstring(_T("dvipng "))+texbasename+_T(".dvi ") + opt;
    tcout << fgMsg << " Running '" << cmdline << "'\n" << fgLatex;
    LeaveCriticalSection( &cs ); 
    return launch_and_wait(cmdline.c_str());
}

// Start a custom command
int custom(tstring opt)
{
    if( !CheckFileLoaded() )
        return 0;

    EnterCriticalSection( &cs );
    tcout << fgMsg << "-- Run custom command on " << texbasename << ".tex\n";
    tstring cmdline = customcmd+_T(" ") +texbasename+_T(".tex ") + opt;
    tcout << fgMsg << _T(" Running '") << cmdline << _T("'\n") << fgLatex;
    LeaveCriticalSection( &cs ); 
    return launch_and_wait(cmdline.c_str());
}

// Execute a shell command
int shell(tstring cmd)
{
    EnterCriticalSection( &cs );
    tcout << fgMsg << _T(" Running shell command '") << cmd << _T("'\n") << fgLatex;
    LeaveCriticalSection( &cs ); 
    return launch_and_wait(cmd.c_str());
}

// Run bibtex on the tex file
int bibtex()
{
    if( !CheckFileLoaded() )
        return 0;

    EnterCriticalSection( &cs );
    tcout << fgMsg << "-- Bibtexing " << texbasename << "tex...\n";
    tstring cmdline = tstring(_T("bibtex "))+texbasename;
    tcout << fgMsg << " Running '" << cmdline << "'\n" << fgLatex;
    LeaveCriticalSection( &cs ); 
    return launch_and_wait(cmdline.c_str());
}


// Restart the make thread if necessary.
// returns true if a thread has been launched successfuly.
bool RestartMakeThread( JOB makejob ) {
    if( makejob == Rest || !CheckFileLoaded() )
        return false;

    /// abort the current "make" thread if it is already started
    if( hMakeThread ) {
        SetEvent(hEvtAbortMake);
        // wait for the "make" thread to end
        WaitForSingleObject(hMakeThread, INFINITE);
        CloseHandle(hMakeThread);
        hMakeThread = NULL;
    }

    // prepare the abort event to let other threads stop the make thread
    ResetEvent(hEvtAbortMake);

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
         //FILE_NOTIFY_CHANGE_SECURITY|
         FILE_NOTIFY_CHANGE_CREATION|
         // FILE_NOTIFY_CHANGE_LAST_ACCESS|
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
    if( static_deps.size() == 0 ) { // if no file to watch then leave
        return;
    }

    EnterCriticalSection(&cs);
    tcout << fgMsg << "\n-- Watching files for change...\n" << fgNormal;
    LeaveCriticalSection( &cs ); 
 
    // Get current directory and keep as reference directory
    TCHAR sCurrDir[MAX_PATH];
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
                if(n>0) print_if_possible(fgMsg, _T("Dependency detected: ") + deps[n].Relative(sCurrDir) + _T("\n"));
            }
            else
                print_if_possible(fgIgnoredfile, _T("Dependency detected but cannot be opened: ") + deps[n].Relative(sCurrDir) + _T("\n"));
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
                    if(n>0) print_if_possible(fgMsg, _T("Preamble dependency detected: ") + preamb_deps[n].Relative(sCurrDir) + _T("\n"));
                }
                else
                {
                    if( n == 0  ) {
                        print_if_possible(fgErr, _T("The preamble file ") + preamble_filename + _T(" cannot be found or opened!\n") );
                        delete dg_deps;
                        delete dg_preamb_deps;
                        return;
                    }
                    else
                        print_if_possible(fgIgnoredfile, _T("Preamble dependency detected but cannot be opened: ") + preamb_deps[n].Relative(sCurrDir) + _T("\n"));
                }
            }
        }

        // Get the digest of the file containing bibtex bibliography references
        tstring bblfilename = texbasename + _T(".bbl");
        md5 dg_bbl;
        dg_bbl.DigestFile(bblfilename.c_str());
    	
        // Reset the hEvtDependenciesChanged event so that it can be set if
        // some thread requires to notify a dependency change
        ResetEvent(hEvtDependenciesChanged);

        ////// Create the list of dir to be watched
        vector<WATCHDIRINFO *> watchdirs; // info on the directories to be monitored

        for(vector<CFilename>::iterator it = deps.begin(); it!=deps.end();it++) {
            tstring abspath = it->GetDirectory();
            if( !is_wdi_in(watchdirs, abspath.c_str()) )
                watchdirs.push_back(CreateWatchDir(abspath.c_str()));
        }
        for(vector<CFilename>::iterator it = preamb_deps.begin(); it!=preamb_deps.end();it++) {
            tstring abspath = it->GetDirectory();
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
                print_if_possible(fgMsg, _T("\n-- Dependencies have changed\n"));
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
                 FILE_NOTIFY_CHANGE_CREATION|
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
                pFileNotify->FileName[min(pFileNotify->FileNameLength/sizeof(WCHAR), _MAX_FNAME-1)] = 0;

                PTSTR pFilename;
                #ifdef _UNICODE
                    pFilename = pFileNotify->FileName;
                #else
                    // Convert the filename from unicode string to oem string
                    TCHAR oemfilename[_MAX_FNAME];
                    wcstombs( oemfilename, pFileNotify->FileName, _MAX_FNAME );
                    pFilename = oemfilename;
                #endif
                CFilename modifiedfile(watchdirs[iTriggeredDir]->szPath, pFilename);

                if( pFileNotify->Action != FILE_ACTION_MODIFIED ) {
					print_if_possible(fgIgnoredfile, tstring(_T(".\"")) + modifiedfile.Relative(texdir) + _T("\" touched\n") );
                }
                else {
                    md5 dg_new;

                    // is it the bibtex file?
                    if( modifiedfile == CFilename(texdir, bblfilename) ) {
                        // has the digest changed?
                        if( dg_new.DigestFile(modifiedfile) && (dg_bbl != dg_new) ) {
                            dg_bbl = dg_new;
                            print_if_possible(fgDepFile, tstring(_T("+ ")) + modifiedfile.Relative(texdir) + _T("(bibtex) changed\n") );
                            makejob = max(Compile, makejob);
                        }
                        else
                            print_if_possible(fgIgnoredfile, tstring(_T(".\"")) + modifiedfile.Relative(texdir) + _T("\" modified but digest preserved\n") );
                    }
                    else {
                        // is it a dependency of the main .tex file?
                        vector<CFilename>::iterator it = find(deps.begin(),deps.end(), modifiedfile);
                        if(it != deps.end() ) {
                            size_t i = it - deps.begin();
                            if ( dg_new.DigestFile(modifiedfile) && (dg_deps[i]!=dg_new) ) {
	                            dg_deps[i] = dg_new;
	                            print_if_possible(fgDepFile, tstring(_T("+ \"")) + modifiedfile.Relative(texdir) + _T("\" changed (dependency file).\n") );
	                            makejob = max(Compile, makejob);
                            }
                            else
	                            print_if_possible(fgIgnoredfile, tstring(_T(".\"")) + modifiedfile.Relative(texdir) + _T("\" modified but digest preserved\n") );
                        }
                        else if ( ExternalPreamblePresent ) {
                            // is it a dependency of the preamble?
                            vector<CFilename>::iterator it = find(preamb_deps.begin(),preamb_deps.end(), modifiedfile);
                            if(it != preamb_deps.end() ) {
                                size_t i = it - preamb_deps.begin();
                                if ( dg_new.DigestFile(modifiedfile) && (dg_preamb_deps[i]!=dg_new) ) {
                                    dg_preamb_deps[i] = dg_new;
                                    print_if_possible(fgDepFile, tstring(_T("+ \"")) + modifiedfile.Relative(texdir) + _T("\" changed (preamble dependency file).\n") );
                                    makejob = max(FullCompile, makejob);
                                }
                                else
                                    print_if_possible(fgIgnoredfile, tstring(_T(".\"")) + modifiedfile.Relative(texdir) + _T("\" modified but digest preserved\n") );
                            }
                            // not a relevant file ...
                            else
                                print_if_possible(fgIgnoredfile, tstring(_T(".\"")) + modifiedfile.Relative(texdir) + _T("\" modified\n") );
                        }
                        // not a relevant file ...
                        else
                            print_if_possible(fgIgnoredfile, tstring(_T(".\"")) + modifiedfile.Relative(texdir) + _T("\" modified\n") );
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
    }
}

