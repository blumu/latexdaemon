/// inspired from the tutorial at http://www.inf.uni-konstanz.de/~kuehl/c++/iostream/ by Dietmar Kuehl 

#include "LatexOutputFilters.h"
#include "Console.h"
#include "global.h"


/// altought the constructor allow to specify a buffer size, the current implementation
/// only works if bsize==0 i.e. in unbuffered mode.
buffLatexFilter::buffLatexFilter(tstreambuf *sb, FILTER filter, int bsize):
      tstreambuf(),
      m_sbuf(sb),
      m_filtermode(filter),
      m_curline(_T("")),
      m_lookforlinenumber(false)
{
    if (bsize) {
        TCHAR *ptr = new TCHAR[bsize];
        setp(ptr, ptr + bsize);
    }
    else
        setp(0, 0); // no put-buffer

    setg(0, 0, 0); // no get-buffer

}

buffLatexFilter::~buffLatexFilter()
{
    sync();
    delete[] pbase();
}

int buffLatexFilter::sync()
{
    return 0;
}


/// send the content of m_curline
void buffLatexFilter::put_buff()
{
    m_sbuf->sputn(m_curline.c_str(), (streamsize)m_curline.size());
    m_curline = _T("");
}

/// send the last line read to the external streambuffer
void buffLatexFilter::put_newline_buff()
{
    if( m_lookforlinenumber ){
        JadedHoboConsole::console.SetColor( _fgLineNumber, JadedHoboConsole::bgMask );
        m_sbuf->sputn(_T("  "),2);
        m_sbuf->sputn(m_curline.c_str(), (streamsize)m_curline.size());
        
        if( _tcsncmp(m_curline.c_str(), _T("l."), 2) == 0  ) 
            m_lookforlinenumber = false;
    }
    else if( _tcsncmp(m_curline.c_str(), _T("Overfull"), 8) == 0 
            || _tcsncmp(m_curline.c_str(), _T("Underfull"),9) == 0 
            || _tcsncmp(m_curline.c_str(), _T("LaTeX Warning"),13) == 0 )
    {
        if( m_filtermode == WarnOnly || m_filtermode == ErrWarnOnly || m_filtermode ==  Highlight ) {
            JadedHoboConsole::console.SetColor( _fgWarning, JadedHoboConsole::bgMask );
            m_sbuf->sputn(_T("  "),2);
            m_sbuf->sputn(m_curline.c_str(), (streamsize)m_curline.size());
        }
    }
    else if ( m_curline.c_str()[0] == '!') {
        if( m_filtermode == ErrOnly || m_filtermode == ErrWarnOnly || m_filtermode ==  Highlight ) {
            JadedHoboConsole::console.SetColor( _fgErr, JadedHoboConsole::bgMask );
            m_sbuf->sputn(_T("  "),2);
            m_sbuf->sputn(m_curline.c_str(), (streamsize)m_curline.size());
            m_lookforlinenumber = true;
        }
    }
    else if( m_filtermode == Highlight ) {
        JadedHoboConsole::console.SetColor( _fgLatex, JadedHoboConsole::bgMask );
        m_sbuf->sputn(_T("  "),2);
        m_sbuf->sputn(m_curline.c_str(), (streamsize)m_curline.size());
    }
    m_curline = _T("");
}



int buffLatexFilter::overflow(int c)
{
  if (c == EOF) {
    put_newline_buff();
    return 0;
  }
  else {
    m_curline += c;
    if (c == '\n')
        put_newline_buff();
    return c;
  }
}


