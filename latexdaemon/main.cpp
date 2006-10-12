// By william blum (http://web.comlab.ox.ac.uk/oucl/people/william.blum.html)
// September 2006
#define VERSION			0.2

// The Crc32Dynamic class was developped by Brian Friesen and can be downloaded from
// http://www.codeproject.com/cpp/crc32.asp

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
#include "Crc32Dynamic.h"


void WatchTexFiles(LPCTSTR texpath, LPCTSTR texbasename);
void launch(LPCTSTR cmdline);
void precompile(LPCTSTR texbasename);
void compile(LPCTSTR texbasename);
void fullcompile(LPCTSTR texbasename);
int compare_timestamp(LPCTSTR sourcefile, LPCTSTR outputfile);

#define PREAMBLE_BASENAME       "preamble"
#define PREAMBLE_FILENAME       PREAMBLE_BASENAME ".tex"
#define MAXCMDLINE				1024


#define ERR_SRCABSENT -1
#define ERR_OUTABSENT -2
#define SRC_FRESHER	   1
#define OUT_FRESHER	   0


using namespace std;


int _tmain(int argc, TCHAR *argv[])
{
	cout << "LatexDaemon " << VERSION << " by William Blum, September 2006" << endl << endl;;
    if(argc <= 1)
	{
		cout << "Instructions:" << endl;;
		cout << "  1 Move the preamble from your .tex file to a new file named preamble.tex." << endl << endl;;
		cout << "  2 Insert the following line at the beginning of your .tex file:" << endl;;
		cout << "      \\ifx\\incrcompilation\\undefined \\input preamble.tex \\fi" << endl << endl ;;
		cout << "  3 Launch the compiler daemon with the command \"" << argv[0] << " file.tex\"" << endl;;
		cout << "    where file.tex is your tex file."<< endl;;
		return 1;
	}

	TCHAR drive[4];
	TCHAR dir[_MAX_DIR];
	TCHAR file[_MAX_FNAME];
	TCHAR ext[_MAX_EXT];
	TCHAR fullpath[_MAX_PATH];

	_fullpath( fullpath, argv[1], _MAX_PATH );
	cout << "The full path is " << fullpath << "\n";
	_tsplitpath(fullpath, drive, dir, file, ext);

	if(  _tcsncmp(ext, ".tex", 4) )	{
		cerr << "Error: the file has not the .tex extension!\n\n";
		return 1;
	}

	// change current directory
	string path = string(drive) +  dir;
	_chdir(path.c_str());

	int res = compare_timestamp(PREAMBLE_FILENAME, (string(PREAMBLE_BASENAME)+".fmt").c_str());
	if ( res == ERR_SRCABSENT ) {
	  cout << "File " << PREAMBLE_FILENAME << " not found!\n";
	  return 2;
	}
	else if( res == SRC_FRESHER || res == ERR_OUTABSENT ) {
	   if( res == SRC_FRESHER ) {
		 cout << "+ " << PREAMBLE_FILENAME << " has been modified since last run.\n";
		 cout << "  Let's recreate the format file and recompile " << file << ".tex.\n";
	   }
	   else {		
		 cout << "+ " << PREAMBLE_BASENAME << ".fmt does not exists. Let's create it...\n";
	   }
	   precompile(file);
	   cout << "...................................\n";
	   compile(file);
	   cout << "-----------------------------------\n";
	}
	else // OUT_FRESHER: no need to update the format file
	{
		int res = compare_timestamp((string(file)+".tex").c_str(), (string(file)+".dvi").c_str());
		if ( res == ERR_SRCABSENT ) {
			cout << "File " << file << ".tex not found!\n";
			return 2;
		}
		else if( res == SRC_FRESHER || res == ERR_OUTABSENT ) {
			if( res == SRC_FRESHER )
				cout << "+ " << file << ".tex has been modified since last run. Let's recompile it...\n";
			else
				cout << "+ " << file << ".dvi does not exists. Let's create it...\n";
			compile(file);
			cout << "-----------------------------------\n";
		}
	}

	// watch for changes
    WatchTexFiles(path.c_str(), file);

	return 0;
}

// compare the time stamp of source and target files
int compare_timestamp(LPCTSTR sourcefile, LPCTSTR outputfile)
{
	struct stat attr_src, attr_out;			// file attribute structures
	int res;

	res = stat(sourcefile, &attr_src);
	if( res ) // problem when getting the attributes of the source file?
	  return ERR_SRCABSENT;

	res = stat(outputfile, &attr_out);
	if( res ) // problem when getting the attributes of the output file?
	  return ERR_OUTABSENT;

	return ( difftime(attr_out.st_mtime, attr_src.st_mtime) > 0 ) ? OUT_FRESHER : SRC_FRESHER;
}


// Launch an external program
void launch(LPCTSTR cmdline)
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
		return;
	}
	free(szCmdline);

	// Wait until child process exits.
	WaitForSingleObject( pi.hProcess, INFINITE );

	// Close process and thread handles. 
	CloseHandle( pi.hProcess );
	CloseHandle( pi.hThread );
}

// Precompile the preamble into the format file "texfile.fmt"
void precompile(LPCTSTR texbasename)
{
	string cmdline = string("pdftex -interaction=nonstopmode -src-specials -ini \"&latex\"  \"\\input ")+PREAMBLE_FILENAME+" \\dump\\endinput \"";
	cout << " Running '" << cmdline << "'\n";
	launch(cmdline.c_str());	
}


// Compile the final tex file using the precompiled preamble
void compile(LPCTSTR texbasename)
{
	string cmdline = string("pdftex -interaction=nonstopmode -src-specials \"&")+PREAMBLE_BASENAME+"\" \"\\def\\incrcompilation{} \\input "+texbasename+" \"";
	cout << " Running '" << cmdline << "'\n";
	launch(cmdline.c_str());
}

// Compile the latex file without using the precompiled preamble
void fullcompile(LPCTSTR texbasename)
{
	string cmdline = string("latex -interaction=nonstopmode -src-specials ")+texbasename+".tex";
	cout << " Running '" << cmdline << "'\n";
	launch(cmdline.c_str());
}

void WatchTexFiles(LPCTSTR texpath, LPCTSTR texbasename)
{
	string texfilename = string(texbasename) + ".tex";


	// CRC of the tex file and the preamble file
	DWORD crc_tex, crc_preamble;
	
	// Compute the CRC of the tex file and the preamble file
	CCrc32Dynamic crc;
	crc.Init();
	if( NO_ERROR != crc.FileCrc32Assembly(texfilename.c_str(), crc_tex) ) {
		cerr << "File " << texfilename << " cannot be found or opened!\n";
		return;
	}

	if( NO_ERROR != crc.FileCrc32Assembly(PREAMBLE_FILENAME, crc_preamble) ) {
		cerr << "File " << PREAMBLE_FILENAME << " cannot be found or opened!\n";
		return;
	}


	HANDLE hDir = CreateFile(
		texpath, /* pointer to the directory containing the tex files */
		FILE_LIST_DIRECTORY,                /* access (read-write) mode */
		FILE_SHARE_READ|FILE_SHARE_DELETE|FILE_SHARE_WRITE,  /* share mode */
		NULL, /* security descriptor */
		OPEN_EXISTING, /* how to create */
		FILE_FLAG_BACKUP_SEMANTICS, /* file attributes */
		NULL /* file with attributes to copy */
	  );

	BYTE buffer [1024*sizeof(FILE_NOTIFY_INFORMATION )];
	FILE_NOTIFY_INFORMATION *pFileNotify;
	DWORD BytesReturned;
	char filename[_MAX_FNAME];
    cout << "- Watching directory " << texpath << " for changes...\n";
	SetConsoleTitle("Latex daemon");
	while( ReadDirectoryChangesW(
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
			 NULL)) /* completion routine */
	{
		cout << "\r";
		pFileNotify = (PFILE_NOTIFY_INFORMATION)&buffer;
		do { 
			// Convert the filename from unicode string to oem string
			pFileNotify->FileName[min(pFileNotify->FileNameLength/2, _MAX_FNAME-1)] = 0;
			wcstombs( filename, pFileNotify->FileName, _MAX_FNAME );

			DWORD newcrc;
			// modification of the tex file?
			if( !_tcscmp(filename,texfilename.c_str()) && ( pFileNotify->Action == FILE_ACTION_MODIFIED) ) {
				// has the CRC changed?
				if( (NO_ERROR == crc.FileCrc32Assembly(texfilename.c_str(), newcrc)) &&
					(crc_tex != newcrc) ) {
					crc_tex = newcrc;
					SetConsoleTitle("recompiling... - Latex daemon");
					cout << "+ changes detected in " << texfilename << ", let's recompile it...\n";
					compile(texbasename);
					cout << "-----------------------------------\n";
				}
				else
					cout << ".\"" << filename << "\" has been touched (CRC preserved)\n" ;
			}
			
			// modification of the preamble file?
			else if( !_tcscmp(filename,PREAMBLE_FILENAME) && ( pFileNotify->Action == FILE_ACTION_MODIFIED) ) {
				if( (NO_ERROR == crc.FileCrc32Assembly(PREAMBLE_FILENAME, newcrc)) &&
					(crc_preamble != newcrc) ) {
					crc_preamble = newcrc;
					SetConsoleTitle("recompiling... - Latex daemon");
					cout << "+ changes detected in the preamble file " << PREAMBLE_FILENAME << ".\n";
					cout << "  Let us recreate the format file and recompile " << texbasename << ".tex.\n";
					precompile(texbasename);
					cout << "...................................\n";
					compile(texbasename);
					cout << "-----------------------------------\n";
				}
				else
					cout << ".\"" << filename << "\" has been touched (CRC preserved)\n" ;
			}
			else
				cout << ".\"" << filename << "\" touched or modified\n" ;

			pFileNotify = (FILE_NOTIFY_INFORMATION*) ((PBYTE)pFileNotify + pFileNotify->NextEntryOffset);
			SetConsoleTitle("Latex daemon");
		}
		while( pFileNotify->NextEntryOffset );

		cout << "- waiting for changes...";
	}
	/* DWORD Buffer.NextEntryOffset; */
    CloseHandle(hDir);
}