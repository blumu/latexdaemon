// LatexDaemon version is defined in version.h2
#include "version.h2"

#define MAKE_VERSION(a,b,c)		#a "." #b "." #c
#define MAKE_VERSION2(a,b,c)	MAKE_VERSION(a,b,c)
#define APP_VERSION				MAKE_VERSION2(MAJOR_VERSION, MINOR_VERSION, BUILD)

#define MAKE_VERSION_T(a,b,c)	_T(#a) _T(".") _T(#b) _T(".") _T(#c)
#define MAKE_VERSION2_T(a,b,c)	MAKE_VERSION_T(a,b,c)
#define APP_VERSION_T			MAKE_VERSION2_T(MAJOR_VERSION, MINOR_VERSION, BUILD)

#define INFO_MAKE_VERSION_T(a,b,c)	_T("Version ") _T(#a) _T(".") _T(#b) _T(" Build ") _T(#c)
#define INFO_MAKE_VERSION2_T(a,b,c)	INFO_MAKE_VERSION_T(a,b,c)
#define INFO_VERSION_T		        INFO_MAKE_VERSION2_T(MAJOR_VERSION, MINOR_VERSION, BUILD)


// version number used in the version section of resource headers(.rc2)
#define VS_INFO_VERSION		MAJOR_VERSION,MINOR_VERSION,BUILD,0

#define APP_TITLE			 "LatexDaemon"
#define BUILD_DATE			__DATE__
#define COMPANY_NAME		"William Blum"
#define COPYRIGHT			"Copyright � William Blum 2006-2021"
