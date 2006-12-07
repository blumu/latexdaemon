// By william blum (http://web.comlab.ox.ac.uk/oucl/people/william.blum.html)
// december 2006
#define VERSION			0.5

// List of changes:
// 0.5
//  . indicates if there are errors during Latex compilation in the title of the console
// 0.4
//  . Change: compute the MD5 digest instead of the CRC
//  . New: it is now possible to specify additional dependency files at the command line using globling (*.tex)
//    The first file being the main tex file.
//  . New: console colors to distinguish Latex output from Latexdaemon output
// 0.1: first version, September 2006

// Acknowledgment:
// - The MD5 class is a modification of CodeGuru's one: http://www.codeguru.com/Cpp/Cpp/algorithms/article.php/c5087
// - Command line processing routine from The Code Project: http://www.codeproject.com/useritems/SimpleOpt.asp
// - Console color header file (Console.h) from: http://www.codeproject.com/cpp/AddColorConsole.asp

#define _WIN32_WINNT  0x0400
#define _CRT_SECURE_NO_DEPRECATE
#include <windows.h>
#include <Winbase.h>
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

// console colors used
#define fgMsg			JadedHoboConsole::fg_green
#define fgErr			JadedHoboConsole::fg_red
#define fgWarning		JadedHoboConsole::fg_yellow
#define fgLatex			JadedHoboConsole::fg_lowhite
#define fgIgnoredfile	JadedHoboConsole::fg_gray
#define fgNormal		JadedHoboConsole::fg_lowhite
#define fgDepFile		JadedHoboConsole::fg_cyan

void WatchTexFiles(LPCTSTR texpath, LPCTSTR mainfilebase, CSimpleGlob &glob);
DWORD launch(LPCTSTR cmdline);
bool precompile(LPCTSTR texbasename);
bool compile(LPCTSTR texbasename);
int compare_timestamp(LPCTSTR sourcefile, LPCTSTR outputfile);

#define PREAMBLE_BASENAME       "preamble"
#define PREAMBLE_FILENAME       PREAMBLE_BASENAME ".tex"
#define MAXCMDLINE				1024


#define OUT_FRESHER	   0x00
#define SRC_FRESHER	   0x01
#define ERR_OUTABSENT  0x02
#define ERR_SRCABSENT  0x04

// recompilation required
enum compilationreq { Unecessary = 0 , Partial = 1, Full =2} ;


bool preamble_present = true;

using namespace std;

// define the ID values to indentify the option
enum { OPT_HELP };

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
    { OPT_HELP, _T("-?"),     SO_NONE    }, // "-?"
    { OPT_HELP, _T("--help"), SO_NONE    }, // "--help"
    SO_END_OF_OPTIONS                       // END
};

// show the usage of this program
void ShowUsage(int argc, TCHAR *argv[]) {
	cout << "Usage: latexdaemon [--help] MAINTEXFILE DEPENDENCYFILES\n\n";
	cout << "Instructions:" << endl;;
	cout << "  1 Move the preamble from your .tex file to a new file named preamble.tex." << endl << endl;;
	cout << "  2 Insert the following line at the beginning of your .tex file:" << endl;;
	cout << "      \\ifx\\incrcompilation\\undefined \\input preamble.tex \\fi" << endl << endl ;;
	cout << "  3 Launch the compiler daemon with the command \"" << argv[0] << " main.tex *.tex\"" << endl;;
	cout << "    where main.tex is the main file of your latex project." << endl;;

}

// perform the necessary compilation
void make(compilationreq compreq, LPCSTR mainfilebase)
{
	if( compreq != Unecessary ) {
		bool bCompOk = true;
		if( compreq == Partial ) {
			SetConsoleTitle("recompiling... - LatexDaemon");
			bCompOk = compile(mainfilebase);
		}
		else if ( compreq == Full ) {
			SetConsoleTitle("recompiling... - LatexDaemon");
			bCompOk = precompile(mainfilebase) && compile(mainfilebase);
		}
		SetConsoleTitle(bCompOk ? "LatexDaemon" : "(errors) - LatexDaemon");
	}
}


int _tmain(int argc, TCHAR *argv[])
{
	cout << "LatexDaemon " << VERSION << " by William Blum, December 2006" << endl << endl;;


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

        if (args.OptionId() == OPT_HELP) {
            ShowUsage(argc,argv);
            return 0;
        }

        uiFlags |= (unsigned int) args.OptionId();
    }

    CSimpleGlob glob(uiFlags);
    if (SG_SUCCESS != glob.Add(args.FileCount(), args.Files())) {
        _tprintf(_T("Error while globbing files\n"));
        return 1;
    }

    cout << "Main file: '" << glob.File(0) << "'\n";

	TCHAR drive[4];
	TCHAR dir[_MAX_DIR];
	TCHAR mainfile[_MAX_FNAME];
	TCHAR ext[_MAX_EXT];
	TCHAR fullpath[_MAX_PATH];

	_fullpath( fullpath, glob.File(0), _MAX_PATH );
	_tsplitpath(fullpath, drive, dir, mainfile, ext);
	cout << "Directory: " << dir << "\n";

	if(  _tcsncmp(ext, ".tex", 4) )	{
		cerr << fgErr << "Error: the file has not the .tex extension!\n\n";
		return 1;
	}

	cout << "Dependencies:\n";
    for (int n = 1; n < glob.FileCount(); ++n)
		_tprintf(_T("  %2d: '%s'\n"), n, glob.File(n));

	if(glob.FileCount() == 0)
		return 1;

	// change current directory
	string path = string(drive) +  dir;
	_chdir(path.c_str());

	// compare the timestamp of the preamble.tex file and the format file
	int res = compare_timestamp(PREAMBLE_FILENAME, (string(PREAMBLE_BASENAME)+".fmt").c_str());
	
	preamble_present = !(res & ERR_SRCABSENT);
	if ( !preamble_present ) {
		cout << fgWarning << "Warning: Preamble file " << PREAMBLE_FILENAME << " not found. Precompilation mode desactivated!\n";
	}

	// The preamble file is present and the format file does not exist or has a timestamp
	// older than the preamble file : then recreate the format file and recompile the .tex file.
	if( preamble_present &&  (res == SRC_FRESHER || (res & ERR_OUTABSENT)) ) {
		if( res == SRC_FRESHER ) {
			 cout << fgMsg << "+ " << PREAMBLE_FILENAME << " has been modified since last run.\n";
			 cout << fgMsg << "  Let's recreate the format file and recompile " << mainfile << ".tex.\n";
		}
		else {		
			cout << fgMsg << "+ " << PREAMBLE_BASENAME << ".fmt does not exists. Let's create it...\n";
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

	// watch for changes
    WatchTexFiles(path.c_str(), mainfile, glob);

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
	ZeroMemory( &pi, sizeof(pi) );

	// Start the child process. 
	if( !CreateProcess( NULL,   // No module name (use command line)
		szCmdline,      // Command line
		NULL,           // Process handle not inheritable
		NULL,           // Thread handle not inheritable
		FALSE,          // Set handle inheritance to FALSE
		0,              // No creation flags
		NULL,           // Use parent's environment block
		NULL,           // Use parent's starting directory 
		&si,            // Pointer to STARTUPINFO structure
		&pi )           // Pointer to PROCESS_INFORMATION structure
	) {
		cout << "CreateProcess failed ("<< GetLastError() << ") : " << cmdline <<".\n";
		return -1;
	}
	free(szCmdline);

	// Wait until child process exits.
	WaitForSingleObject( pi.hProcess, INFINITE );
	
	// Get the return code
	DWORD dwRet;
	GetExitCodeProcess( pi.hProcess, &dwRet);
	
	// Close process and thread handles. 
	CloseHandle( pi.hProcess );
	CloseHandle( pi.hThread );

	return dwRet;
}

// Precompile the preamble into the format file "texfile.fmt"
bool precompile(LPCTSTR texbasename)
{
	string cmdline = string("pdftex -interaction=nonstopmode --src-specials -ini \"&latex\"  \"\\input ")+PREAMBLE_FILENAME+" \\dump\\endinput \"";
	cout << fgMsg << "-- Creation of the format file...\n";
	cout << "running '" << cmdline << "'\n" << fgLatex;
	return 0==launch(cmdline.c_str());
}


// Compile the final tex file using the precompiled preamble
bool compile(LPCTSTR texbasename)
{
	cout << fgMsg << "-- Compilation of " << texbasename << ".tex.\n";

	// preamble present? Then compile using the precompiled preamble.
	if(  preamble_present ) {
		string cmdline = string("pdftex -interaction=nonstopmode --src-specials \"&")+PREAMBLE_BASENAME+"\" \"\\def\\incrcompilation{} \\input "+texbasename+".tex \"";
		cout << fgMsg << " Running '" << cmdline << "'\n" << fgLatex;
		return 0==launch(cmdline.c_str());
	}
	// no preamble: compile the latex file without the standard latex format file.
	else {
		string cmdline = string("latex -interaction=nonstopmode -src-specials ")+texbasename+".tex";
		cout << fgMsg << " Running '" << cmdline << "'\n" << fgLatex;
		return 0==launch(cmdline.c_str());
	}
}

VOID CALLBACK FileIOCompletionRoutine(
  DWORD dwErrorCode,
  DWORD dwNumberOfBytesTransfered,
  LPOVERLAPPED lpOverlapped
)
{
	int i;
	i=0;
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
	md5 &dg_tex = dg_deps[0];

	// get the digest of the preamble file
	md5 dg_preamble;
	if( preamble_present && !dg_preamble.DigestFile(PREAMBLE_FILENAME) ) {
		cerr << "File " << PREAMBLE_FILENAME << " cannot be found or opened!\n" << fgLatex;
		return;
	}


	HANDLE hDir = CreateFile(
		texpath, /* pointer to the directory containing the tex files */
		FILE_LIST_DIRECTORY,                /* access (read-write) mode */
		FILE_SHARE_READ|FILE_SHARE_DELETE|FILE_SHARE_WRITE,  /* share mode */
		NULL, /* security descriptor */
		OPEN_EXISTING, /* how to create */
		FILE_FLAG_BACKUP_SEMANTICS  | FILE_FLAG_OVERLAPPED, /* file attributes */
		NULL /* file with attributes to copy */
	  );

	BYTE buffer [2][1024*sizeof(FILE_NOTIFY_INFORMATION )];
	FILE_NOTIFY_INFORMATION *pFileNotify;
	DWORD BytesReturned;
	char filename[_MAX_FNAME];
    cout << fgMsg << "-- Watching directory " << texpath << " for changes...\n";
	SetConsoleTitle("LatexDaemon");
	
	int curBuff = 0;
	OVERLAPPED overlapped;
	overlapped.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	ReadDirectoryChangesW(
			 hDir, /* handle to directory */
			 &buffer[curBuff], /* read results buffer */
			 sizeof(buffer[0]), /* length of buffer */
			 FALSE, /* monitoring option */
			 //FILE_NOTIFY_CHANGE_SECURITY|FILE_NOTIFY_CHANGE_CREATION| FILE_NOTIFY_CHANGE_LAST_ACCESS|
			 FILE_NOTIFY_CHANGE_LAST_WRITE 
			 //|FILE_NOTIFY_CHANGE_SIZE |FILE_NOTIFY_CHANGE_ATTRIBUTES |FILE_NOTIFY_CHANGE_DIR_NAME |FILE_NOTIFY_CHANGE_FILE_NAME
			 , /* filter conditions */
			 NULL, /* bytes returned */
			 &overlapped, /* overlapped buffer */
			 FileIOCompletionRoutine); /* completion routine */

	while( 1 ) ;

	{
		//GetOverlappedResult(hDir, &overlapped, &BytesReturned, TRUE);
		ReadDirectoryChangesW(
			 hDir, /* handle to directory */
			 &buffer[curBuff], /* read results buffer */
			 sizeof(buffer[0]), /* length of buffer */
			 FALSE, /* monitoring option */
			 //FILE_NOTIFY_CHANGE_SECURITY|FILE_NOTIFY_CHANGE_CREATION| FILE_NOTIFY_CHANGE_LAST_ACCESS|
			 FILE_NOTIFY_CHANGE_LAST_WRITE
			 //|FILE_NOTIFY_CHANGE_SIZE |FILE_NOTIFY_CHANGE_ATTRIBUTES |FILE_NOTIFY_CHANGE_DIR_NAME |FILE_NOTIFY_CHANGE_FILE_NAME
			 , /* filter conditions */
			 NULL, /* bytes returned */
			 &overlapped, /* overlapped buffer */
			 FileIOCompletionRoutine); /* completion routine */

		cout << "\r                                                       \r";
		//////////////
		// Check if some source files have changed and compute the according compilation requirement 
		compilationreq compreq = Unecessary;
		pFileNotify = (PFILE_NOTIFY_INFORMATION)&buffer[curBuff];
		do { 
			// Convert the filename from unicode string to oem string
			pFileNotify->FileName[min(pFileNotify->FileNameLength/2, _MAX_FNAME-1)] = 0;
			wcstombs( filename, pFileNotify->FileName, _MAX_FNAME );

			if( pFileNotify->Action != FILE_ACTION_MODIFIED )
				cout << fgIgnoredfile << ".\"" << filename << "\" touched\n" ;
			else
			{
				md5 dg_new;
				// modification of the tex file?
				if( !_tcscmp(filename,maintexfilename.c_str())  ) {
					// has the digest changed?
					if( dg_new.DigestFile(maintexfilename.c_str()) && (dg_tex != dg_new) ) {
						dg_tex = dg_new;
						cout << fgDepFile << "+ " << maintexfilename << " changed\n";
						compreq = max(Partial, compreq);
					}
					else
						cout << fgIgnoredfile << ".\"" << filename << "\" modified but digest preserved\n" ;
				}
				
				// modification of the preamble file?
				else if( preamble_present && !_tcscmp(filename,PREAMBLE_FILENAME)  ) {
					if( dg_new.DigestFile(PREAMBLE_FILENAME) && (dg_preamble!=dg_new) ) {
						dg_preamble = dg_new;
						cout << fgDepFile << "+ \"" << PREAMBLE_FILENAME << "\" changed (preamble file).\n";
						compreq = max(Full, compreq);
					}
					else
						cout << fgIgnoredfile << ".\"" << filename << "\" modified but digest preserved\n" ;
				}
				
				// another file
				else {
					// is it a dependency file?
					int i = 1;
					for(i=1; i<glob.FileCount(); i++)
						if(!_tcscmp(filename,glob.File(i))) break;

					if( i<glob.FileCount() ) {
						if ( dg_new.DigestFile(glob.File(i)) && (dg_deps[i]!=dg_new) ) {
							dg_deps[i] = dg_new;
							cout << fgDepFile << "+ \"" << glob.File(i) << "\" changed (dependency file).\n";
							compreq = max(Partial, compreq);
						}
						else
							cout << fgIgnoredfile << ".\"" << filename << "\" modified but digest preserved\n" ;
					}
					// not a revelant file ...				
					else
						cout << fgIgnoredfile << ".\"" << filename << "\" modified\n";
				}
			}

			pFileNotify = (FILE_NOTIFY_INFORMATION*) ((PBYTE)pFileNotify + pFileNotify->NextEntryOffset);
		}
		while( pFileNotify->NextEntryOffset );

		//////////////
		// perform the necessary compilations
		make(compreq, mainfilebase);

		cout << fgMsg << "-- waiting for changes...";
	}
	CloseHandle(overlapped.hEvent);
    CloseHandle(hDir);
}

