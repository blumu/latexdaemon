#include <windows.h>
#include <string>
#include <errno.h>
#include <tchar.h>

// extract the extension part from a filepath
// the result is copied to pszExtFileName if pszExtFileName!=NULL
// the function returns a pointer to the extension part in pszPath
PTSTR GetFileExtPart( PTSTR pszPath, SIZE_T size, PTSTR pszExtFileName )
{
	for (size_t cbPath = _tcslen(pszPath); cbPath>0; cbPath--) {
		if( pszPath[cbPath-1] == _T('.') ) {
			if( pszExtFileName != NULL )
				_tcscpy_s(pszExtFileName, size, &pszPath[cbPath]);
			return &pszPath[cbPath];
		}
	}

	return NULL;
}

// Extrait le nom du fichier a partir d'un chemin complet.
//
// Retourne un pointeur sur le nom du fichier dans pszPath.
PCTSTR GetFileBaseNamePart( PCTSTR pszPath )
{
    size_t cbPath;

    for (cbPath = _tcslen(pszPath); cbPath>0; cbPath--)
    {
        if( (pszPath[cbPath-1]=='\\') ||
            (pszPath[cbPath-1]==':')  ||
            (pszPath[cbPath-1]=='/')  )
            break;
    }
    return &pszPath[cbPath];
}

// Obtient le nom du repertoire (sans le nom du fichier) et retourne le dans pszDir
BOOL GetDirectory (PCTSTR pszFile, PTSTR pszDir, size_t wMaxSize)
{
    PCTSTR pszBaseName = GetFileBaseNamePart(pszFile);

    if( EINVAL == _tcsncpy_s(pszDir, wMaxSize, pszFile, pszBaseName-pszFile) )
        return FALSE;

    // Cas particulier ci le fichier se trouve dans la racine
    if( pszDir[pszBaseName-pszFile-2] == ':' )
    {
        // rajoute l'anti-slash à la fin
        pszDir[pszBaseName-pszFile-1] = '\\';
        pszDir[pszBaseName-pszFile] = '\0';
    }

    return TRUE;
}