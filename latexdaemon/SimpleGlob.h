// File:    SimpleGlob.h
// Library: SimpleOpt
// Author:  Brodie Thiesfield <code@jellycan.com>
// Source:  http://code.jellycan.com/simpleopt/
// Version: 1.4
//
// MIT LICENCE
// ===========
// The licence text below is the boilerplate "MIT Licence" used from:
// http://www.opensource.org/licenses/mit-license.php
//
// Copyright (c) 2006, Brodie Thiesfield
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is furnished
// to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
// IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#ifndef INCLUDED_SimpleGlob
#define INCLUDED_SimpleGlob

// on Windows we want to use MBCS aware string functions and mimic the
// Unix glob functionality. On Unix we just use glob.
#ifdef _WIN32
# include <mbstring.h>
# define sg_strchr      ::_mbschr
# define sg_strlen      ::_mbslen
# define sg_strcpy      ::_mbscpy
# define sg_strcmp      ::_mbscmp
# define sg_stricmp     ::_mbsicmp
# define SOCHAR_T       unsigned char
#else
# define __USE_GNU
# include <glob.h>
# define sg_strchr      ::strchr
# define sg_strlen      ::strlen
# define sg_strcpy      ::strcpy
# define sg_strcmp      ::strcmp
# define sg_stricmp     ::strcasecmp
# define SOCHAR_T       char
#endif

#include <string.h>
#include <wchar.h>

// flags
enum {
    SG_GLOB_ERR         = 1 << 0,   // return upon read error (e.g. directory does not have read permission)
    SG_GLOB_MARK        = 1 << 1,   // append a slash (backslash in Windows) to each path which corresponds to a directory
    SG_GLOB_NOSORT      = 1 << 2,   // don't sort the returned pathnames (they are by default)
    SG_GLOB_NOCHECK     = 1 << 3,   // if no pattern matches, return the original pattern
    SG_GLOB_TILDE       = 1 << 4,   // tilde expansion is carried out (on Unix platforms)
    SG_GLOB_ONLYDIR     = 1 << 5,   // return only directories which match (not compatible with SG_GLOB_ONLYFILE)
    SG_GLOB_ONLYFILE    = 1 << 6,   // return only files which match (not compatible with SG_GLOB_ONLYDIR, Windows only)
    SG_GLOB_NODOT       = 1 << 7    // do not return the "." or ".." special files
};

// errors
enum {
    SG_SUCCESS          = 0,        // success
    SG_ERR_MEMORY       = -1,       // out of memory
    SG_ERR_FAILURE      = -2        // general failure
};

// ----------------------------------------------------------------------------
// Util functions
class CSimpleGlobUtil
{
public:
    static const char * strchr(const char *s, char c)           { return (char *) sg_strchr((const SOCHAR_T *)s, c); }
    static const wchar_t * strchr(const wchar_t *s, wchar_t c)  { return ::wcschr(s, c); }

    // Note: char strlen returns number of bytes, not characters
    static size_t strlen(const char *s)                         { return ::strlen(s); }
    static size_t strlen(const wchar_t *s)                      { return ::wcslen(s); }

    static char *strcpy(char *dst, const char *src)             { return (char *) sg_strcpy((SOCHAR_T *)dst, (const SOCHAR_T *)src); }
    static wchar_t *strcpy(wchar_t *dst, const wchar_t *src)    { return ::wcscpy(dst, src); }

    static int strcmp(const char *s1, const char *s2)           { return sg_strcmp((const SOCHAR_T *)s1, (const SOCHAR_T *)s2); }
    static int strcmp(const wchar_t *s1, const wchar_t *s2)     { return ::wcscmp(s1, s2); }

    static int stricmp(const char *s1, const char *s2)          { return sg_stricmp((const SOCHAR_T *)s1, (const SOCHAR_T *)s2); }
#if 1
    static int stricmp(const wchar_t *s1, const wchar_t *s2)    { return ::_wcsicmp(s1, s2); }
#else
    static int stricmp(const wchar_t *s1, const wchar_t *s2)    { return ::wcscasecmp(s1, s2); }
#endif // _WIN32
};

// ----------------------------------------------------------------------------
// Windows implementation
#ifdef _WIN32

#define SG_ARGV_INITIAL_SIZE       32
#define SG_BUFFER_INITIAL_SIZE     1024

#ifndef INVALID_FILE_ATTRIBUTES
# define INVALID_FILE_ATTRIBUTES    ((DWORD)-1)
#endif

template<class SOCHAR>
class CSimpleGlobImplWin
{
public:
    CSimpleGlobImplWin() {
        m_uiFlags           = 0;
        m_nReservedSlots    = 0;
        m_bArgsIsIndex      = true;
        m_nArgsLen          = 0;
        m_nArgsSize         = 0;
        m_rgpArgs           = 0;
        m_uiBufferLen       = 0;
        m_uiBufferSize      = 0;
        m_pBuffer           = 0;
    }

    ~CSimpleGlobImplWin() {
        if (m_rgpArgs) free(m_rgpArgs);
        if (m_pBuffer) free(m_pBuffer);
    }

    void Init(unsigned int a_uiFlags, int a_nReservedSlots) {
        m_uiFlags           = a_uiFlags;
        m_nReservedSlots    = a_nReservedSlots;
        m_nArgsLen          = a_nReservedSlots;
        m_uiBufferLen       = 0;
    }

    int Add(const SOCHAR *a_pszFileSpec) {
        if (!m_bArgsIsIndex)
            SwapArgsType();

        // if this doesn't contain wildcards then we can just add it directly
        if (!CSimpleGlobUtil::strchr(a_pszFileSpec, '*') &&
            !CSimpleGlobUtil::strchr(a_pszFileSpec, '?')) {
            DWORD dwAttrib = GetFileAttributesS(a_pszFileSpec);
            if (dwAttrib == INVALID_FILE_ATTRIBUTES) {
                if (m_uiFlags & SG_GLOB_NOCHECK)
                    return AppendName(a_pszFileSpec, false);
                return SG_ERR_FAILURE;
            }
            bool bIsDir = (dwAttrib & FILE_ATTRIBUTE_DIRECTORY) != 0;
            return AppendName(a_pszFileSpec, bIsDir);
        }

        // search for the first match on the file
        HANDLE hFind = FindFirstFileS(a_pszFileSpec);
        if (hFind == INVALID_HANDLE_VALUE) {
            if (m_uiFlags & SG_GLOB_NOCHECK)
                return AppendName(a_pszFileSpec, false);
            return SG_ERR_FAILURE;
        }

        // add it and find all subsequent matches
        int nError, nStartLen = m_nArgsLen;
        BOOL bSuccess;
        do
        {
            nError = AppendName(GetFileNameS((SOCHAR)0), IsDirS((SOCHAR)0));
            bSuccess = FindNextFileS(hFind, (SOCHAR)0);
        }
        while (nError == SG_SUCCESS && bSuccess);
        FindClose(hFind);

        // sort if not disabled
        if (m_nArgsLen > (nStartLen + 1) && !(m_uiFlags & SG_GLOB_NOSORT)) {
            SwapArgsType();
            qsort(
                m_rgpArgs + nStartLen,
                m_nArgsLen - nStartLen,
                sizeof(m_rgpArgs[0]),
                compareFile);
        }

        return nError;
    }

    int FileCount() const {
        return m_nArgsLen;
    }

    SOCHAR ** Files() {
        if (m_bArgsIsIndex)
            SwapArgsType();
        return m_rgpArgs;
    }

    static int compareFile(const void *a1, const void *a2) {
        const SOCHAR *s1 = *(const SOCHAR **)a1;
        const SOCHAR *s2 = *(const SOCHAR **)a2;
        return CSimpleGlobUtil::stricmp(s1, s2);
    }

private:
    int AppendName(const SOCHAR *a_pszFileName, bool a_bIsDir) {
        // check for special cases which cause us to ignore this entry
        if ((m_uiFlags & SG_GLOB_ONLYDIR) && !a_bIsDir)
            return SG_SUCCESS;
        if ((m_uiFlags & SG_GLOB_ONLYFILE) && a_bIsDir)
            return SG_SUCCESS;
        if ((m_uiFlags & SG_GLOB_NODOT) && a_bIsDir) {
            if (a_pszFileName[0] == '.') {
                if (a_pszFileName[1] == '\0')
                    return SG_SUCCESS;
                if (a_pszFileName[1] == '.' && a_pszFileName[2] == '\0')
                    return SG_SUCCESS;
            }
        }

        // ensure that we have enough room in the argv array
        if (m_nArgsLen + 1 >= m_nArgsSize) {
            int nNewSize = (m_nArgsSize > 0) ? m_nArgsSize * 2 : SG_ARGV_INITIAL_SIZE;
            while (m_nArgsLen + 1 >= nNewSize)
                nNewSize *= 2;
            void * pNewBuffer = realloc(m_rgpArgs, nNewSize * sizeof(SOCHAR*));
            if (!pNewBuffer)
                return SG_ERR_MEMORY;
            m_nArgsSize = nNewSize;
            m_rgpArgs = (SOCHAR**) pNewBuffer;
        }

        // ensure that we have enough room in the string buffer
        size_t uiLen = CSimpleGlobUtil::strlen(a_pszFileName) + 1; // len in characters + null
        if (a_bIsDir && (m_uiFlags & SG_GLOB_MARK) == SG_GLOB_MARK)
            ++uiLen;    // need space for the backslash
        if (m_uiBufferLen + uiLen >= m_uiBufferSize) {
            size_t uiNewSize = (m_uiBufferSize > 0) ? m_uiBufferSize * 2 : SG_BUFFER_INITIAL_SIZE;
            while (m_uiBufferLen + uiLen >= uiNewSize)
                uiNewSize *= 2;
            void * pNewBuffer = realloc(m_pBuffer, uiNewSize * sizeof(SOCHAR));
            if (!pNewBuffer)
                return SG_ERR_MEMORY;
            m_uiBufferSize = uiNewSize;
            m_pBuffer = (SOCHAR*) pNewBuffer;
        }

        // add this entry
        m_rgpArgs[m_nArgsLen++] = (SOCHAR*)m_uiBufferLen;    // offset from beginning of buffer
        CSimpleGlobUtil::strcpy(m_pBuffer + m_uiBufferLen, a_pszFileName);
        m_uiBufferLen += uiLen;

        // add the backslash if desired
        if (a_bIsDir && (m_uiFlags & SG_GLOB_MARK) == SG_GLOB_MARK) {
            SOCHAR szBackslash[] = { '\\', '\0' };
            CSimpleGlobUtil::strcpy(m_pBuffer + m_uiBufferLen - 2, szBackslash);
        }

        return SG_SUCCESS;
    }

    void SwapArgsType() {
        if (m_bArgsIsIndex)
            for (int n = 0; n < m_nArgsLen; ++n)
                m_rgpArgs[n] = (SOCHAR*) (m_pBuffer + (size_t) m_rgpArgs[n]);
        else
            for (int n = 0; n < m_nArgsLen; ++n)
                m_rgpArgs[n] = (SOCHAR*) (m_rgpArgs[n] - m_pBuffer);
        m_bArgsIsIndex = !m_bArgsIsIndex;
    }

    HANDLE FindFirstFileS(const char *a_pszFileSpec)    { return FindFirstFileA(a_pszFileSpec, &m_oFindDataA); }
    BOOL FindNextFileS(HANDLE a_hFile, char)            { return FindNextFileA(a_hFile, &m_oFindDataA); }
    const char * GetFileNameS(char) const               { return m_oFindDataA.cFileName; }
    bool IsDirS(char) const                             { return (m_oFindDataA.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY; }
    DWORD GetFileAttributesS(const char *a_pszPath)     { return GetFileAttributesA(a_pszPath); }

    HANDLE FindFirstFileS(const wchar_t *a_pszFileSpec) { return FindFirstFileW(a_pszFileSpec, &m_oFindDataW); }
    BOOL FindNextFileS(HANDLE a_hFile, wchar_t)         { return FindNextFileW(a_hFile, &m_oFindDataW); }
    const wchar_t * GetFileNameS(wchar_t) const         { return m_oFindDataW.cFileName; }
    bool IsDirS(wchar_t) const                          { return (m_oFindDataW.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY; }
    DWORD GetFileAttributesS(const wchar_t *a_pszPath)  { return GetFileAttributesW(a_pszPath); }

private:
    unsigned int        m_uiFlags;
    int                 m_nReservedSlots;   // number of argv slots reserved for the client
    bool                m_bArgsIsIndex;     // is the argv array storing indexes or pointers
    SOCHAR **           m_rgpArgs;          // argv array
    int                 m_nArgsSize;        // allocated size of array
    int                 m_nArgsLen;         // used length
    SOCHAR *            m_pBuffer;          // argv string buffer
    size_t              m_uiBufferSize;     // allocated size of buffer
    size_t              m_uiBufferLen;      // used length of buffer
    WIN32_FIND_DATAA    m_oFindDataA;
    WIN32_FIND_DATAW    m_oFindDataW;
};
#endif // _WIN32

// ----------------------------------------------------------------------------
// Unix implementation
#ifndef _WIN32
template<class SOCHAR>
class CSimpleGlobImplUnix
{
public:
    CSimpleGlobImplUnix() {
        m_uiFlags           = 0;
        m_nReservedSlots    = 0;
        memset(&m_oGlobData, 0, sizeof(m_oGlobData));
    }

    ~CSimpleGlobImplUnix() {
        globfree(&m_oGlobData);
    }

    void Init(unsigned int a_uiFlags, int a_nReservedSlots) {
        m_uiFlags           = a_uiFlags;
        m_nReservedSlots    = a_nReservedSlots;
        globfree(&m_oGlobData);
        memset(&m_oGlobData, 0, sizeof(m_oGlobData));
    }

    int Add(const SOCHAR *a_pszFileSpec) {
        // set all of the flags
        int nFlags = 0;
        if (m_nReservedSlots > 0) {
            m_oGlobData.gl_offs = (size_t) m_nReservedSlots;
            nFlags |= GLOB_DOOFFS;
            m_nReservedSlots = 0; // only ever do this once
        }
        if (m_uiFlags & SG_GLOB_ERR)
            nFlags |= GLOB_ERR;
        if (m_uiFlags & SG_GLOB_MARK)
            nFlags |= GLOB_MARK;
        if (m_uiFlags & SG_GLOB_NOSORT)
            nFlags |= GLOB_NOSORT;
        if (m_uiFlags & SG_GLOB_NOCHECK)
            nFlags |= GLOB_NOCHECK;
        if (m_uiFlags & SG_GLOB_TILDE)
            nFlags |= GLOB_TILDE;
        if (m_uiFlags & SG_GLOB_ONLYDIR)
            nFlags |= GLOB_ONLYDIR;
        //if (m_uiFlags & SG_GLOB_ONLYFILE) // not supported by glob
        //    nFlags |= GLOB_ONLYFILE;
        //if (m_uiFlags & SG_GLOB_NODOT)    // not supported by glob
        //    nFlags |= GLOB_NODOT;
        if (m_oGlobData.gl_pathv)
            nFlags |= GLOB_APPEND;

        // call glob
        int nError = glob(a_pszFileSpec, nFlags, 0, &m_oGlobData);

        // interpret the result
        if (nError == 0)
            return SG_SUCCESS;
        if (nError == GLOB_NOSPACE)
            return SG_ERR_MEMORY;
        return SG_ERR_FAILURE;
    }

    int FileCount() const {
        return m_oGlobData.gl_pathc;
    }

    SOCHAR ** Files() const {
        return m_oGlobData.gl_pathv;
    }

private:
    int     m_uiFlags;
    int     m_nReservedSlots;   // number of argv slots reserved for the client
    glob_t  m_oGlobData;
};
#endif // UNIX

// ----------------------------------------------------------------------------
// Wrapper class
template<class SOCHAR>
class CSimpleGlobTempl
{
public:
    // see Init() for details
    CSimpleGlobTempl(unsigned int a_uiFlags = 0, int a_nReservedSlots = 0) {
        Init(a_uiFlags, a_nReservedSlots);
    }

    // a_uiFlags            combination of the SG_GLOB flags above
    // a_nReservedSlots     number of empty slots that should be reserved in the returned argv array
    inline void Init(unsigned int a_uiFlags = 0, int a_nReservedSlots = 0) {
        m_oImpl.Init(a_uiFlags, a_nReservedSlots);
    }

    // a_pszFileSpec        filespec to parse and add files for
    inline int Add(const SOCHAR *a_pszFileSpec) {
        return m_oImpl.Add(a_pszFileSpec);
    }

    // add an array of filespecs
    int Add(int a_nCount, const SOCHAR * const * a_rgpszFileSpec) {
        int nResult;
        for (int n = 0; n < a_nCount; ++n) {
            nResult = Add(a_rgpszFileSpec[n]);
            if (nResult != SG_SUCCESS)
                return nResult;
        }
        return SG_SUCCESS;
    }

    inline int FileCount() const    { return m_oImpl.FileCount(); }
    inline SOCHAR ** Files()        { return m_oImpl.Files(); }
    inline SOCHAR * File(int n)     { return m_oImpl.Files()[n]; }

private:
#ifdef _WIN32
    CSimpleGlobImplWin<SOCHAR>  m_oImpl;
#else
    CSimpleGlobImplUnix<SOCHAR> m_oImpl;
#endif
};

// we supply both ASCII and WIDE char versions, plus a
// SOCHAR style that changes depending on the build setting
typedef CSimpleGlobTempl<char>    CSimpleGlobA;
typedef CSimpleGlobTempl<wchar_t> CSimpleGlobW;
#if defined(_UNICODE)
# define CSimpleGlob CSimpleGlobW
#else
# define CSimpleGlob CSimpleGlobA
#endif

#endif // INCLUDED_SimpleGlob
