#include <shellapi.h>

PWCHAR* MyCommandLineToArgvW( PWCHAR CmdLine, int* _argc );
PCHAR* MyCommandLineToArgvA( PCHAR CmdLine, int* _argc );

#ifdef UNICODE
#define CommandLineToArgv CommandLineToArgvW
#else
#define CommandLineToArgv MyCommandLineToArgvA
#endif 
