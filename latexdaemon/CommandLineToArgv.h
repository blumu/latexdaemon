PWCHAR* CommandLineToArgvW( PWCHAR CmdLine, int* _argc );
PCHAR* CommandLineToArgvA( PCHAR CmdLine, int* _argc );


#ifdef UNICODE
#define CommandLineToArgv CommandLineToArgvW
#else
#define CommandLineToArgv CommandLineToArgvA
#endif