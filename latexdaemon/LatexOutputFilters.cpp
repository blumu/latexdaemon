/// inspired from the tutorial at http://www.inf.uni-konstanz.de/~kuehl/c++/iostream/ by Dietmar Kuehl 

#include "LatexOutputFilters.h"
#include "Console.h"
#include "global.h"


/// altought the constructor allow to specify a buffer size, the current implementation
/// only works if bsize==0 i.e. in unbuffered mode.
buffLatexFilter::buffLatexFilter(streambuf *sb, FILTER filter, int bsize):
      streambuf(),
      m_sbuf(sb),
      m_filtermode(filter),
      m_curline(""),
      m_lookforlinenumber(false)
{
    if (bsize) {
        char *ptr = new char[bsize];
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
    m_curline = "";
}

/// send the last line read to the external streambuffer
void buffLatexFilter::put_newline_buff()
{
    if( m_lookforlinenumber && strncmp(m_curline.c_str(), "l.", 2) == 0  ) {
        JadedHoboConsole::console.SetColor( _fgLineNumber, JadedHoboConsole::bgMask );
        m_sbuf->sputn("  ",2);
        m_sbuf->sputn(m_curline.c_str(), (streamsize)m_curline.size());

        m_lookforlinenumber = false;
    }
    else if( strncmp(m_curline.c_str(), "Overfull", 8) == 0 
            || strncmp(m_curline.c_str(), "Underfull",9) == 0 
            || strncmp(m_curline.c_str(), "LaTeX Warning",13) == 0 )
    {
        if( m_filtermode == WarnOnly || m_filtermode == ErrWarnOnly || m_filtermode ==  Highlight ) {
            JadedHoboConsole::console.SetColor( _fgWarning, JadedHoboConsole::bgMask );
            m_sbuf->sputn("  ",2);
            m_sbuf->sputn(m_curline.c_str(), (streamsize)m_curline.size());
        }
    }
    else if ( m_curline.c_str()[0] == '!') {
        if( m_filtermode == ErrOnly || m_filtermode == ErrWarnOnly || m_filtermode ==  Highlight ) {
            JadedHoboConsole::console.SetColor( _fgErr, JadedHoboConsole::bgMask );
            m_sbuf->sputn("  ",2);
            m_sbuf->sputn(m_curline.c_str(), (streamsize)m_curline.size());
            m_lookforlinenumber = true;
        }
    }
    else if( m_filtermode == Highlight ) {
        JadedHoboConsole::console.SetColor( _fgLatex, JadedHoboConsole::bgMask );
        m_sbuf->sputn("  ",2);
        m_sbuf->sputn(m_curline.c_str(), (streamsize)m_curline.size());
    }
    m_curline = "";
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


