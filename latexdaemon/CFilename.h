#include "path_conv.h"
#include "tstring.h"

#pragma once

BOOL GetDirectory (PCTSTR pszFile, PTSTR pszDir, size_t wMaxSize);
PCTSTR GetFileBaseNamePart( PCTSTR pszPath );
PTSTR GetFileExtPart( PTSTR pszPath, SIZE_T size, PTSTR pszExtFileName );



class CFilename {
public:
    CFilename()
    {
        m_szFullpath[0] = '\0';
        m_pszBasenamePart = m_szFullpath;
    }

    inline void init(LPCTSTR fullpath)
    {
        _tcscpy_s(m_szFullpath, _MAX_PATH, fullpath);
        m_pszBasenamePart = GetFileBaseNamePart(m_szFullpath);
    }

    CFilename(tstring str)
    {
        init(str.c_str());
    }

    CFilename(const CFilename& _assign)
    {
        init(_assign.m_szFullpath);
    }


    operator tstring () {
        return tstring(m_szFullpath);
    }

    operator PCTSTR () {
        return (PCTSTR)m_szFullpath;
    }

    PCTSTR c_str() {
        return (PCTSTR)m_szFullpath;
    }

    tstring Relative(tstring sCurrDir)
    {
        TCHAR temp[_MAX_PATH];
        Abs2Rel(m_szFullpath,temp,_MAX_PATH, sCurrDir.c_str());
        return tstring(temp);
    }

    tstring GetDirectory()
    {
        TCHAR temp[_MAX_PATH];
        ::GetDirectory(m_szFullpath,temp,_MAX_PATH);
        return tstring(temp);
    }

    // create a filename from a relative path
    CFilename(tstring sCurrDir, tstring sRelPath)
    {
        Rel2Abs(sRelPath.c_str(),m_szFullpath, _MAX_PATH, sCurrDir.c_str());
        m_pszBasenamePart = GetFileBaseNamePart(m_szFullpath);
    }

    CFilename& operator= (const CFilename& _assign)
    {
        if (this != &_assign) // check that it is not assigning to itself?
            init(_assign.m_szFullpath);
        return *this;
    }

    friend bool operator== (const CFilename& _Left, const CFilename& _Right);
    friend bool operator!= (const CFilename& _Left, const CFilename& _Right);

private:
    TCHAR   m_szFullpath[_MAX_PATH];
    PCTSTR  m_pszBasenamePart;
};

bool operator== (const CFilename& _Left, const CFilename& _Right) {
    return 0 ==_tcsicmp(_Left.m_szFullpath, _Right.m_szFullpath );
}

bool operator!= (const CFilename& _Left, const CFilename& _Right) {
    return 0 !=_tcsicmp(_Left.m_szFullpath, _Right.m_szFullpath );
}