// By william blum (http://william.famille-blum.org/software/index.html)
// Created in September 2006
// Last modfification 25 feb 2007
#define APP_NAME		"LatexDaemon"
#define VERSION			0.901

// See changelog.html for the list of changes:.

// TODO:
//  At the moment, messages reporting that some watched file has been modified are not shown while the "make" 
//  thread is running. This is done in order to avoid printf interleaving. Another solution would 
//  be to delay the printing of these messages until the end of the execution of the make "thread".

// Acknowledgment:
// - The MD5 class is a modification of CodeGuru's one: http://www.codeguru.com/Cpp/Cpp/algorithms/article.php/c5087
// - Command line processing routine from The Code Project: http://www.codeproject.com/useritems/SimpleOpt.asp
// - Console color header file (Console.h) from: http://www.codeproject.com/cpp/AddColorConsole.asp

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
using namespace std;

//////////
/// Prototypes
void WatchTexFiles(LPCTSTR texpath, LPCTSTR mainfilebase, CSimpleGlob &glob);
DWORD launch(LPCTSTR cmdline);
bool fullcompile(LPCTSTR texbasename);
bool compile(LPCTSTR texbasename);
int compare_timestamp(LPCTSTR sourcefile, LPCTSTR outputfile);

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

#define DEFAULTPREAMBLE2_BASENAME       "preamble"
#define DEFAULTPREAMBLE2_FILENAME       DEFAULTPREAMBLE2_BASENAME ".tex"
#define DEFAULTPREAMBLE1_EXT            "pre"

#define MAXCMDLINE				1024

// result of timestamp comparison
#define OUT_FRESHER	   0x00
#define SRC_FRESHER	   0x01
#define ERR_OUTABSENT  0x02
#define ERR_SRCABSENT  0x04

// constants determining which kind of recompilation is required 
enum MAKETYPE { Unecessary = 0 , Partial = 1, Full =2} ;

//////////
/// Global variables

// critical section for handling printf across the different threads
CRITICAL_SECTION cs;

// is the preamble stored in an external file? (this option can be changed with a command line parameter)
bool UseExternalPreamble = true;

// preamble file name and basename
string preamble_filename = "";
string preamble_basename = "";

// handle du "make" thread
HANDLE hMakeThread = NULL;

// Event fired when the make thread needs to be aborted
HANDLE hEvtAbortMake = NULL;

// Tex initialization file (can be specified as a command line parameter)
string texinifile = "latex"; // use latex by default

// String append to the end of the command box title
string title_suffix = "- " APP_NAME;


// type of the parameter passed to the "make" thread
typedef struct {
	MAKETYPE	maketype;
	LPCTSTR		mainfilebasename;
} MAKETHREADPARAM  ;


// define the ID values to indentify the option
enum { OPT_HELP, OPT_INI, OPT_NOWATCH, OPT_COMPILE, OPT_FULLCOMPILE, OPT_PREAMBLE };

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
CSimpleOpt::SOption g_rgOptions[] = {
    { OPT_HELP, _T("-?"),     SO_NONE   },
    { OPT_HELP, _T("--help"), SO_NONE   },
    { OPT_HELP, _T("-help"), SO_NONE    },
    { OPT_INI,  _T("-ini"), SO_REQ_SEP  },
    { OPT_INI,  _T("--ini"), SO_REQ_SEP },
    { OPT_PREAMBLE,  _T("-preamble"), SO_REQ_CMB  },
    { OPT_PREAMBLE,  _T("--preamble"), SO_REQ_CMB },
	{ OPT_NOWATCH,  _T("-nowatch"), SO_NONE  },
    { OPT_NOWATCH,  _T("--nowatch"), SO_NONE  },
    { OPT_COMPILE,  _T("-forcecompile"), SO_NONE  },
    { OPT_COMPILE,  _T("--forcecompile"), SO_NONE },
    { OPT_FULLCOMPILE,  _T("-forcefullcompile"), SO_NONE  },
    { OPT_FULLCOMPILE,  _T("--forcefullcompile"), SO_NONE },
    SO_END_OF_OPTIONS                   // END
};


////////////////////



// show the usage of this program
void ShowUsage(int argc, TCHAR *argv[]) {
	cout << "USAGE: latexdaemon [options] mainfile.tex [dependencies]" <<endl
		 << "where" << endl
		 << "* options can be:" << endl
		 << " --help" << endl 
		 << "   Show this help message." <<endl<<endl
		 << " --ini inifile" << endl 
		 << "   Set inifile  as the initialization format file that will be used to compile the preamble." <<endl<<endl
		 << " --forcecompile" << endl
		 << "   Force the compilation of the .tex file at the start even when no modification is detected." << endl<<endl
		 << " --forcefullcompile" << endl
		 << "   Force the compilation of the preamble and the .tex file at the start even when no modification is detected." <<endl<<endl
	     << " --nowatch" << endl 
		 << "   Launch the compilation if necessary and then exit without watching for file changes."<<endl<<endl
		 << " --preamble=none|external" << endl 
		 << "   Set to 'none', it specifies that the main .tex file does not use an external preamble file."<<endl
		 << "   The current version is not capable of extracting the preamble from the .tex file, therefore if this switch is used the precompilation feature will be automatically desactivated."<<endl
		 << "   Set to 'external' (default), it specifies that the preamble is stored in an external file. The daemon first look for a preamble file called mainfile.pre, if this does not exists it tries preamble.tex and eventually, if neither exists, falls back to the 'none' option."<<endl
		 << "   If these files exist but do not correspond to the preamble of your latex document (i.e. not included with \\input{mainfile.pre} at the beginning of your .tex file) then you must set the 'none' option to avoid the precompilation of a wrong preamble." <<endl<<endl
		 << "* dependencies contains a list of files that your main tex file relies on. You can sepcify list of files using jokers, for example '*.tex ..\\*.tex'." <<endl<<endl
	     << "INSTRUCTIONS:" << endl
	     << "  1. Move the preamble from your .tex file to a new file named mainfile.pre" << endl 
	     << "  and insert '\\input{mainfile.pre}' at the beginning of your mainfile.tex file," << endl << endl
	     << "  2. start the daemon with the command \"latexdaemon mainfile.tex *.tex\" " << 
		    "(or \"latexdaemon -ini pdflatex mainfile.tex *.tex\" if you want to use pdflatex"<<
			"instead of latex) where main.tex is the main file of your latex project. "  << endl;
}

// perform the necessary compilation
void make(MAKETYPE maketype, LPCSTR mainfilebasename)
{
	if( maketype != Unecessary ) {
		bool bCompOk = true;
		if( maketype == Partial ) {
			SetConsoleTitle(("recompiling..." + title_suffix).c_str());
			bCompOk = compile(mainfilebasename);
		}
		else if ( maketype == Full ) {
			SetConsoleTitle(("recompiling..." + title_suffix).c_str());
			bCompOk = fullcompile(mainfilebasename);
		}
		SetConsoleTitle(((bCompOk ? "monitoring" : "(errors) monitoring") + title_suffix).c_str());
	}
}

void WINAPI MakeThread( void *param )
{
	ResetEvent(hEvtAbortMake);

	//////////////
	// perform the necessary compilations
	MAKETHREADPARAM *p = (MAKETHREADPARAM *)param;
	make(p->maketype, p->mainfilebasename);	
	free(p);

	cout <<	flush;
	hMakeThread = NULL;
}


int _tmain(int argc, TCHAR *argv[])
{
	InitializeCriticalSection(&cs);

	// create the event used to abort the "make" thread
	hEvtAbortMake = CreateEvent(NULL,TRUE,FALSE,NULL);

	cout << APP_NAME << " " << VERSION << " by William Blum, December 2006" << endl << endl;;

    unsigned int uiFlags = 0;

	if (argc <= 1 ) {
		ShowUsage(argc,argv);
		return 1;
	}

	// default options, can be overwritten by command line parameters
	bool ForceFullCompile = false, ForceCompile = false, Watch = true;

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
                break;
            case SO_ARG_INVALID:
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
		case OPT_HELP:
            ShowUsage(argc,argv);
            return 0;
		case OPT_INI:
			texinifile = args.OptionArg();
			cout << "-Intiatialization file set to \"" << texinifile << "\"" << endl;;
			break;
		case OPT_NOWATCH:
			Watch = false;
			break;
		case OPT_COMPILE:
			cout << "-Initial compilation required." << endl;;
			ForceCompile = true;
			break;
		case OPT_FULLCOMPILE:
			cout << "-Initial full compilation required." << endl;;
			ForceFullCompile = true;
			break;
		case OPT_PREAMBLE:
			UseExternalPreamble = strcmp(args.OptionArg(),"none") ? true : false ;
			if( UseExternalPreamble )
				cout << "-Use external preamble." << endl;
			else
				cout << "-No preamble." << endl;
			break;
		default:
			break;
        }

        uiFlags |= (unsigned int) args.OptionId();
    }

    CSimpleGlob glob(uiFlags);
    if (SG_SUCCESS != glob.Add(args.FileCount(), args.Files()) ) {
        _tprintf(_T("Error while globbing files! Make sure the paths given as parameters are correct.\n"));
        return 1;
    }
	if( args.FileCount() == 0 ){
        _tprintf(_T("No input file specified!\n"));
        return 1;
    }

	cout << "-Main file: '" << glob.File(0) << "'\n";

	TCHAR drive[4];
	TCHAR dir[_MAX_DIR];
	TCHAR mainfile[_MAX_FNAME];
	TCHAR ext[_MAX_EXT];
	TCHAR fullpath[_MAX_PATH];

	_fullpath( fullpath, glob.File(0), _MAX_PATH );
	_tsplitpath(fullpath, drive, dir, mainfile, ext);

	// set console title
	title_suffix = " - "+ texinifile +"Daemon - " + fullpath;
	SetConsoleTitle(("Initialization " + title_suffix).c_str());

	cout << "-Directory: " << dir << "\n";

	if(  _tcsncmp(ext, ".tex", 4) )	{
		cerr << fgErr << "Error: the file has not the .tex extension!\n\n";
		return 1;
	}
	if( glob.FileCount()>1 ) {
		cout << "-Dependencies:\n";
		for (int n = 1; n < glob.FileCount(); ++n)
			_tprintf(_T("  %2d: '%s'\n"), n, glob.File(n));
	}
	else
		cout << "-No dependency.\n";
		
	if(glob.FileCount() == 0)
		return 1;

	// change current directory
	string path = string(drive) +  dir;
	_chdir(path.c_str());

	int res; // will contain the result of the comparison of the timestamp of the preamble.tex file with the format file

	// check for the presence of the external preamble file
	if( UseExternalPreamble ) {
		// compare the timestamp of the preamble.tex file and the format file
		preamble_filename = string(mainfile) + "." DEFAULTPREAMBLE1_EXT;
		preamble_basename = string(mainfile);
		res = compare_timestamp(preamble_filename.c_str(), (string(mainfile)+".fmt").c_str());
		UseExternalPreamble = !(res & ERR_SRCABSENT);
		if ( !UseExternalPreamble ) {
			// try with the second default preamble name
			preamble_filename = DEFAULTPREAMBLE2_FILENAME;
			preamble_basename = DEFAULTPREAMBLE2_BASENAME;
			res = compare_timestamp(preamble_filename.c_str(), (string(mainfile)+".fmt").c_str());
			UseExternalPreamble = !(res & ERR_SRCABSENT);
			if ( !UseExternalPreamble ) {
				cout << fgWarning << "Warning: Preamble file not found! I have tried to look for " << mainfile << "." << DEFAULTPREAMBLE1_EXT << " and then " << DEFAULTPREAMBLE2_FILENAME << ")\nPrecompilation mode desactivated!\n";
			}
		}
	}

	if( ForceFullCompile ) {
		make(Full, mainfile);
	}
	else if ( ForceCompile ) {
		make(Partial, mainfile);
	}
	// Determine what needs to be recompiled based on the files that have been touched since last compilation.
	else {
		// The external preamble file is used and the format file does not exist or has a timestamp
		// older than the preamble file : then recreate the format file and recompile the .tex file.
		if( UseExternalPreamble && (res == SRC_FRESHER || (res & ERR_OUTABSENT)) ) {
			if( res == SRC_FRESHER ) {
				 cout << fgMsg << "+ " << preamble_filename << " has been modified since last run.\n";
				 cout << fgMsg << "  Let's recreate the format file and recompile " << mainfile << ".tex.\n";
			}
			else {		
				cout << fgMsg << "+ " << preamble_basename << ".fmt does not exists. Let's create it...\n";
			}
			make(Full, mainfile);
		}
		
		// either the preamble file exists and the format file is up-to-date  or  there is no preamble file
		else {
			// check if the main file has been modified since the creation of the dvi file
			int maintex_comp = compare_timestamp((string(mainfile)+".tex").c_str(), (string(mainfile)+".dvi").c_str());

			// check if a dependency file has been modified since the creation of the dvi file
			bool dependency_fresher = false;
			for(int i=1; !dependency_fresher && i<glob.FileCount(); i++)
				dependency_fresher = SRC_FRESHER == compare_timestamp(glob.File(i), (string(mainfile)+".dvi").c_str()) ;

			if ( maintex_comp & ERR_SRCABSENT ) {
				cout << fgErr << "File " << mainfile << ".tex not found!\n";
				return 2;
			}
			else if ( maintex_comp & ERR_OUTABSENT ) {
				cout << fgMsg << "+ " << mainfile << ".dvi does not exists. Let's create it...\n";
				make(Partial, mainfile);
			}
			else if( dependency_fresher || (maintex_comp == SRC_FRESHER) ) {
				cout << fgMsg << "+ the main file or some dependency file has been modified since last run. Let's recompile...\n";
				make(Partial, mainfile);
			}
			// else 
			//   We have maintex_comp == OUT_FRESHER and dependency_fresher =false therfore no need to recompile.
		}
	}

	if( Watch )
		WatchTexFiles(path.c_str(), mainfile, glob);

	CloseHandle(hEvtAbortMake);
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



// Launch an external program
DWORD launch(LPCTSTR cmdline)
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
		EnterCriticalSection( &cs );
		cout << "CreateProcess failed ("<< GetLastError() << ") : " << cmdline <<".\n";
        LeaveCriticalSection( &cs ); 
		return -1;
	}
	free(szCmdline);

	// Wait until child process exits or the make process is aborted.
	//WaitForSingleObject( pi.hProcess, INFINITE );	
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

	return dwRet;
}


// Recompile the preamble into the format file "texfile.fmt" and then compile the main file
bool fullcompile(LPCTSTR texbasename)
{
	EnterCriticalSection( &cs );
	string cmdline = string("pdftex -interaction=nonstopmode --src-specials -ini \"&" + texinifile + "\" \"\\input ")+preamble_filename+" \\dump\\endinput \"";
	cout << fgMsg << "-- Creation of the format file...\n";
	cout << "[running '" << cmdline << "']\n" << fgLatex;
    LeaveCriticalSection( &cs ); 
	DWORD dw = launch(cmdline.c_str());

	if( dw )
		return false;
	else
		return compile(texbasename);
}


// Compile the final tex file using the precompiled preamble
bool compile(LPCTSTR texbasename)
{
	EnterCriticalSection( &cs );
	cout << fgMsg << "-- Compilation of " << texbasename << ".tex ...\n";

	// External preamble used? Then compile using the precompiled preamble.
	if( UseExternalPreamble ) {
		// Remark on the latex code included in the following command line:
		//	 % Install a hook for the \input command ...
		///  \let\TEXDAEMONinput\input
		//   % which ignores the first file inclusion (the one inserting the preamble)
		//   \def\input#1{\let\input\TEXDAEMONinput}
		string cmdline = string("pdftex -interaction=nonstopmode --src-specials \"&")+preamble_basename+"\" \"\\let\\TEXDAEMONinput\\input\\def\\input#1{\\let\\input\\TEXDAEMONinput} \\TEXDAEMONinput "+texbasename+".tex \"";
		
		//// Old version requiring the user to insert a conditional at the start of the main .tex file
		//string cmdline = string("pdftex -interaction=nonstopmode --src-specials \"&")+preamble_basename+"\" \"\\def\\incrcompilation{} \\input "+texbasename+".tex \"";
		
		cout << fgMsg << "[running '" << cmdline << "']\n" << fgLatex;
	    LeaveCriticalSection( &cs ); 
		return 0==launch(cmdline.c_str());
	}
	// no preamble: compile the latex file without the standard latex format file.
	else {
		string cmdline = string("latex -interaction=nonstopmode -src-specials ")+texbasename+".tex";
		cout << fgMsg << " Running '" << cmdline << "'\n" << fgLatex;
	    LeaveCriticalSection( &cs ); 
		return 0==launch(cmdline.c_str());
	}
}



void WatchTexFiles(LPCTSTR texpath, LPCTSTR mainfilebase, CSimpleGlob &glob)
{
	// get the digest of the dependcy files
	md5 *dg_deps = new md5 [glob.FileCount()];
    for (int n = 0; n < glob.FileCount(); ++n)
	{
		if( !dg_deps[n].DigestFile(glob.File(n)) ) {
			cerr << "File " << glob.File(n) << " cannot be found or opened!\n";
			return;
		}
	}

	// get the digest of the main tex file
	string maintexfilename = string(mainfilebase) + ".tex";
	md5 dg_tex = dg_deps[0];

	// get the digest of the preamble file
	md5 dg_preamble;
	if( UseExternalPreamble && !dg_preamble.DigestFile(preamble_filename.c_str()) ) {
		cerr << "File " << preamble_filename << " cannot be found or opened!\n" << fgLatex;
		return;
	}

	// open the directory to be monitored
	HANDLE hDir = CreateFile(
		texpath, /* pointer to the directory containing the tex files */
		FILE_LIST_DIRECTORY,                /* access (read-write) mode */
		FILE_SHARE_READ|FILE_SHARE_DELETE|FILE_SHARE_WRITE,  /* share mode */
		NULL, /* security descriptor */
		OPEN_EXISTING, /* how to create */
		FILE_FLAG_BACKUP_SEMANTICS  , //| FILE_FLAG_OVERLAPPED, /* file attributes */
		NULL /* file with attributes to copy */
	  );

    cout << fgMsg << "-- Watching directory " << texpath << " for changes...\n" << fgNormal;
	SetConsoleTitle(("monitoring" + title_suffix).c_str());
	
	BYTE buffer [1024*sizeof(FILE_NOTIFY_INFORMATION )];

	while( 1 )
	{
		FILE_NOTIFY_INFORMATION *pFileNotify;
		DWORD BytesReturned;

		//GetOverlappedResult(hDir, &overlapped, &BytesReturned, TRUE);
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
			 NULL, /* overlapped buffer */
			 NULL); /* completion routine */

		//EnterCriticalSection( &cs );
		//cout << "                         \r";

		//////////////
		// Check if some source files have changed and compute the according compilation requirement 
		MAKETYPE maketype = Unecessary;
		pFileNotify = (PFILE_NOTIFY_INFORMATION)&buffer;
		do { 
			// Convert the filename from unicode string to oem string
			char filename[_MAX_FNAME];
			pFileNotify->FileName[min(pFileNotify->FileNameLength/2, _MAX_FNAME-1)] = 0;
			wcstombs( filename, pFileNotify->FileName, _MAX_FNAME );

			if( pFileNotify->Action != FILE_ACTION_MODIFIED ) {
				if(!hMakeThread) cout << fgIgnoredfile << ".\"" << filename << "\" touched\n" ;
			}
			else
			{
				md5 dg_new;
				// modification of the tex file?
				if( !_tcscmp(filename,maintexfilename.c_str())  ) {
					// has the digest changed?
					if( dg_new.DigestFile(maintexfilename.c_str()) && (dg_tex != dg_new) ) {
 						dg_tex = dg_new;
						if(!hMakeThread) cout << fgDepFile << "+ " << maintexfilename << " changed\n";
						maketype = max(Partial, maketype);
					}
					else {
						if(!hMakeThread) cout << fgIgnoredfile << ".\"" << filename << "\" modified but digest preserved\n" ;
					}
				}
				
				// modification of the preamble file?
				else if( UseExternalPreamble && !_tcscmp(filename,preamble_filename.c_str())  ) {
					if( dg_new.DigestFile(preamble_filename.c_str()) && (dg_preamble!=dg_new) ) {
						dg_preamble = dg_new;
						if(!hMakeThread) cout << fgDepFile << "+ \"" << preamble_filename << "\" changed (preamble file).\n";
						maketype = max(Full, maketype);
					}
					else {
						if(!hMakeThread) cout << fgIgnoredfile << ".\"" << filename << "\" modified but digest preserved\n" ;
					}
				}
				
				// another file
				else {
					// is it a dependency file?
					int i;
					for(i=1; i<glob.FileCount(); i++)
						if(!_tcscmp(filename,glob.File(i))) break;

					if( i<glob.FileCount() ) {
						if ( dg_new.DigestFile(glob.File(i)) && (dg_deps[i]!=dg_new) ) {
							dg_deps[i] = dg_new;
							if(!hMakeThread) cout << fgDepFile << "+ \"" << glob.File(i) << "\" changed (dependency file).\n";
							maketype = max(Partial, maketype);
						}
						else {
							if(!hMakeThread) cout << fgIgnoredfile << ".\"" << filename << "\" modified but digest preserved\n" ;
						}
					}
					// not a revelant file ...				
					else {
						if(!hMakeThread) cout << fgIgnoredfile << ".\"" << filename << "\" modified\n";
					}
				}
			}

			pFileNotify = (FILE_NOTIFY_INFORMATION*) ((PBYTE)pFileNotify + pFileNotify->NextEntryOffset);
		}
		while( pFileNotify->NextEntryOffset );
		//LeaveCriticalSection( &cs ); 

		if( maketype != Unecessary ) {
			/// abort the current "make" thread if it is already started
			if( hMakeThread ) {
				SetEvent(hEvtAbortMake);
				// wait for the "make" thread to end
				WaitForSingleObject(hMakeThread, INFINITE);
			}			
			
			// Create a new "make" thread.
			//  note: it is necessary to dynamically allocate a MAKETHREADPARAM structure
			//  otherwise, if we pass the address of a locally defined variable as a parameter to 
			//  CreateThrear, the content of the structure may change
			//  by the time the make thread is created (since the current thread runs concurrently).
			MAKETHREADPARAM *p = new MAKETHREADPARAM;
			p->maketype = maketype;
			p->mainfilebasename = mainfilebase;
			DWORD makethreadID;
			hMakeThread = CreateThread( NULL,
                    0,
                    (LPTHREAD_START_ROUTINE) MakeThread,
                    (LPVOID)p,
                    0,
                    &makethreadID);
		}

		/*EnterCriticalSection( &cs );
		if(!hMakeThread) cout << fgMsg << "-- waiting for changes...\r";
		LeaveCriticalSection( &cs ); */

	}
//	CloseHandle(overlapped.hEvent);
    CloseHandle(hDir);

}

