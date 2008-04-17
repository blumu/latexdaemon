#pragma once

#include <tchar.h>

#ifdef _UNICODE
#define tstring wstring
#define tcerr wcerr
#define tcout wcout
#define tcin wcin
#define tifstream wifstream
#define tostream wostream
#define tstreambuf wstreambuf
#else
#define tstring string
#define tcerr cerr
#define tcout cout
#define tcin cin
#define tifstream ifstream
#define tostream ostream
#define tstreambuf streambuf
#endif
