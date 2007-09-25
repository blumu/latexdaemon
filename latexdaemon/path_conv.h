//*********************************************************************
//** Module       : path_conv.cpp
//** Directory    : ---
//** Class        : ---
//** Author/Date  : Boby Thomas/RBIN / 04/2006
//** Description  : Defines functions for converting relative path to
//**				absolute and vice versa.
//*********************************************************************


#include <iostream>
#include <string>
#include <stack>
#include <queue>
using namespace std;

#if defined(_MSC_VER)
	#include<windows.h>
#elif defined(__GNUC__)
   #include <dlfcn.h>
   #include <dirent.h>
#define MAX_PATH 500
#else
	#error define your compiler
#endif



#if defined(_MSC_VER)
	#define path_separator "\\/"
	#define path_sep_char  '\\'
#elif defined(__GNUC__)
	#define path_separator "/"
	#define path_sep_char  '/'
#else
	#error define your compiler
#endif



typedef queue<char*> QueuePtrChar;
typedef stack<char*> StackPtrChar;


void GetCurrentDir(char* pcTmp);

char * Rel2Abs(const char *pcRelPath, char *pcAbsPath, const char* pcCurrDir);

char * Abs2Rel(const char *pcAbsPath, char *pcRelPath, const char* pcCurrDir);

