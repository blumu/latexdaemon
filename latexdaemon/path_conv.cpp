//*********************************************************************
//** Module       : path_conv.cpp
//** Directory    : ---
//** Class        : ---
//** Author/Date  : Boby Thomas/RBIN / 04/2006
//** Description  : Defines functions for converting relative path to
//**				absolute and vice versa.
//*********************************************************************


#include <shlwapi.h>
#include <tchar.h>
#include "path_conv.h"

//------------------------------------------------------------------------------
// Method		: Abs2Rel
// Description	: Convert absolute path to relative path.
// Parameter	: pcAbsPath - Input - Absolute path
// Parameter	: pcRelPath - Output - Relative path
// Parameter	: sizeInBytes - Input - size of the pcRelPath buffer in bytes
// Parameter	: pcCurrDir - Input - Current Dir/Reference dir path
// Return		: Relative path
// Author		: Boby Thomas Pazheparampil April 2006
//------------------------------------------------------------------------------
PTSTR Abs2Rel(PCTSTR pcAbsPath, PTSTR pcRelPath, size_t sizeInBytes, PCTSTR pcCurrDir)
{

	TCHAR acTmpCurrDir[MAX_PATH];
	TCHAR acTmpAbsPath[MAX_PATH];
	_tcscpy_s(acTmpCurrDir,pcCurrDir);
	_tcscpy_s(acTmpAbsPath,pcAbsPath);
	
	StackPtrChar tmpStackAbsPath;
	StackPtrChar tmpStackCurrPath;
	StackPtrChar tmpStackOutput;
	QueuePtrChar tmpMatchQueue;
	
    PTSTR nextToken;
	PTSTR sTmp = _tcstok_s(acTmpAbsPath,path_separator, &nextToken);
	while(sTmp)
	{
		tmpStackAbsPath.push(sTmp);
		sTmp = _tcstok_s(NULL, path_separator, &nextToken);
	}

	sTmp = _tcstok_s(acTmpCurrDir,path_separator, &nextToken);
	while(sTmp)
	{
		tmpStackCurrPath.push(sTmp);
		sTmp = _tcstok_s(NULL, path_separator, &nextToken);
	}

	sTmp = pcRelPath;
	while(tmpStackCurrPath.size() > tmpStackAbsPath.size() )
	{
		*sTmp++ = '.';
		*sTmp++ = '.';
		*sTmp++ = path_sep_char;
		tmpStackCurrPath.pop();
	}

	while(tmpStackAbsPath.size() > tmpStackCurrPath.size() )
	{
		PTSTR pcTmp = tmpStackAbsPath.top();
		tmpStackOutput.push(pcTmp);
		tmpStackAbsPath.pop();
	}

	while(tmpStackAbsPath.size() > 0)
	{
		if(_tcsicmp(tmpStackAbsPath.top(),tmpStackCurrPath.top())== 0  )
			tmpMatchQueue.push(tmpStackAbsPath.top());
		else
		{
			while(tmpMatchQueue.size() > 0)
				tmpStackOutput.push(tmpMatchQueue.front());
			tmpStackOutput.push(tmpStackAbsPath.top());
			*sTmp++ = '.';
			*sTmp++ = '.';
			*sTmp++ = path_sep_char;
		}
		tmpStackAbsPath.pop();
		tmpStackCurrPath.pop();	
	}
	while(tmpStackOutput.size() > 0)
	{
		PTSTR pcTmp= tmpStackOutput.top();
		while(*pcTmp != '\0')	
			*sTmp++ = *pcTmp++;
		tmpStackOutput.pop();
		*sTmp++ = path_sep_char;
	}
	*(--sTmp) = '\0';

	return pcRelPath;
}

//------------------------------------------------------------------------------
// Method		: Rel2Abs
// Description	: Convert absolute path to relative path.
// Parameter	: pcRelPath - Input - Relative path
// Parameter	: pcAbsPath - Output - Absolute path
// Parameter	: sizeInBytes - Input - size of the pcAbsPath buffer in bytes
// Parameter	: pcCurrDir - Input - Current Dir/Reference dir path
// Return		: Absolute path
// Author		: Boby Thomas Pazheparampil April 2006
//------------------------------------------------------------------------------

PTSTR Rel2Abs(PCTSTR pcRelPath, PTSTR pcAbsPath, size_t sizeInBytes, PCTSTR pcCurrDir)
{
    ////////////////
    // Modification by William Blum
    if(!PathIsRelative(pcRelPath)) {
        _tcscpy_s(pcAbsPath, sizeInBytes, pcRelPath);
        return pcAbsPath;
    }
    //////////////////

	TCHAR acTmpCurrDir[MAX_PATH];
	TCHAR acTmpRelPath[MAX_PATH];
	_tcscpy_s(acTmpCurrDir,pcCurrDir);
	_tcscpy_s(acTmpRelPath,pcRelPath);
	
	QueuePtrChar tmpQueueRelPath;
	StackPtrChar tmpStackCurrPath;
	StackPtrChar tmpStackOutPath;
	
    PTSTR nextToken;
	PTSTR sTmp = _tcstok_s(acTmpRelPath,path_separator, &nextToken);
	while(sTmp)
	{
		tmpQueueRelPath.push(sTmp);
		sTmp = _tcstok_s(NULL, path_separator, &nextToken);
	}

	sTmp = _tcstok_s(acTmpCurrDir,path_separator, &nextToken);
	while(sTmp)
	{
		tmpStackCurrPath.push(sTmp);
		sTmp = _tcstok_s(NULL, path_separator, &nextToken);
	}


	while(tmpQueueRelPath.size() > 0)
	{
		PTSTR pcTmp= tmpQueueRelPath.front();
		if(_tcscmp(pcTmp, _T("..")) == 0)
			tmpStackCurrPath.pop();
		else
			tmpStackCurrPath.push(pcTmp);
		tmpQueueRelPath.pop();
	}
	while(tmpStackCurrPath.size() > 0)
	{
		tmpStackOutPath.push(tmpStackCurrPath.top());
		tmpStackCurrPath.pop();
	}


	sTmp = pcAbsPath;
#if defined(__GNUC__)
	*sTmp++ = path_sep_char;
#endif
	while(tmpStackOutPath.size() > 0)
	{
		PTSTR pcTmp= tmpStackOutPath.top();
		while(*pcTmp != '\0')	
			*sTmp++ = *pcTmp++;
		tmpStackOutPath.pop();
		*sTmp++ = path_sep_char;
	}
	*(--sTmp) = '\0';

	return pcAbsPath; 	
}


//------------------------------------------------------------------------------
// Method		: GetCurrentDir
// Description	: Get Current Directory.
// Parameter	: pcTmp - Output - Current directory
// Return		: ---
// Author		: Boby Thomas Pazheparampil April 2006
//------------------------------------------------------------------------------
void GetCurrentDir(PTSTR pcTmp)
{

#if defined(_MSC_VER)
   GetCurrentDirectory(MAX_PATH,pcTmp);
#elif defined(__GNUC__)
   getcwd(pcTmp,MAX_PATH);
#else
	#error define your compiler
#endif

   return;
}

//
////------------------------------------------------------------------------------
//// Method		: main
//// Description	: Application to test the functions.
//// Parameter	: ---
//// Return		: ---
//// Author		: Boby Thomas Pazheparampil April 2006
////------------------------------------------------------------------------------
//
//int main(int argc, char* argv[])
//{
//	char sCurrDir[MAX_PATH];
//	char sRelPath[MAX_PATH];
//	char sAbsPath[MAX_PATH];
//	
//	//´Get current directory and keep as reference directory
//	GetCurrentDir(sCurrDir);
//	
//	//Convert all test path to relative and nack to absolute
//	for(int iTmp=0;iTmp<5;iTmp++)
//	{
//		Abs2Rel(sTestAbsPaths[iTmp],sRelPath,sCurrDir);
//		Rel2Abs(sRelPath,sAbsPath,sCurrDir);
//		cout<<"\n\n\n\n"<<sTestAbsPaths[iTmp]<<"\n"<<sRelPath<<"\n"<<sAbsPath;
//	}
//
//	// Test a file
//	FILE *fp = fopen(sTestAbsPaths[0],"r");
//	if(fp != NULL)
//	{
//		cout<<"\nFile exist";
//		fclose(fp);
//		fp=0;
//	}
//
//
//	//Test absolute path.
//	Abs2Rel(sTestAbsPaths[0],sRelPath,sCurrDir);
//	fp = fopen(sRelPath,"r");
//	if(fp != NULL)
//	{
//		cout<<"\nRelative path File exist";
//		fclose(fp);
//	}
//
//	//test relative path
//	Rel2Abs(sRelPath,sAbsPath,sCurrDir);
//
//	fp = fopen(sAbsPath,"r");
//	if(fp != NULL)
//	{
//		cout<<"\nAbsolute path File exist";
//		fclose(fp);
//	}
//	return 0;
//}
//
