#include "path_conv.h"

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

    CFilename(string str)
    {
        _tcscpy_s(m_szFullpath, _MAX_PATH, str.c_str());
        m_pszBasenamePart = GetFileBaseNamePart(m_szFullpath);
    }

    CFilename(const CFilename& _assign)
    {
        _tcscpy_s(m_szFullpath, _MAX_PATH, _assign.m_szFullpath);
        m_pszBasenamePart = GetFileBaseNamePart(m_szFullpath);
    }


    operator string () {
        return string(m_szFullpath);
    }

    operator PCTSTR () {
        return (PCTSTR)m_szFullpath;
    }

    PCTSTR c_str() {
        return (PCTSTR)m_szFullpath;
    }

    string Relative(string sCurrDir)
    {
        char temp[_MAX_PATH];
        Abs2Rel(m_szFullpath,temp,_MAX_PATH, sCurrDir.c_str());
        return string(temp);
    }

    string GetDirectory()
    {
        char temp[_MAX_PATH];
        ::GetDirectory(m_szFullpath,temp,_MAX_PATH);
        return string(temp);
    }

    // create a filename from a relative path
    CFilename(string sCurrDir, string sRelPath)
    {
        Rel2Abs(sRelPath.c_str(),m_szFullpath, _MAX_PATH, sCurrDir.c_str());
        m_pszBasenamePart = GetFileBaseNamePart(m_szFullpath);
    }

    CFilename& operator= (const CFilename& _assign)
    {
        CFilename::CFilename(_assign);
        return *this;
    }

    friend bool operator== (const CFilename& _Left, const CFilename& _Right);

private:
    TCHAR   m_szFullpath[_MAX_PATH];
    PCTSTR  m_pszBasenamePart;
};

bool operator== (const CFilename& _Left, const CFilename& _Right) {
    return 0 ==_tcsicmp(_Left.m_szFullpath, _Right.m_szFullpath );
}