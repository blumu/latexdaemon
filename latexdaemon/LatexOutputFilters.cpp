/// inspired from the tutorial at http://www.inf.uni-konstanz.de/~kuehl/c++/iostream/ by Dietmar Kuehl 

#include "LatexOutputFilters.h"
#include "Console.h"
#include "global.h"

buffLatexFilter::buffLatexFilter(streambuf *sb, FILTER filter):
      streambuf(),
      m_sbuf(sb),
      m_filtermode(filter),
      m_newline(true),
      m_curline(""),
      m_cache(EOF)
{
  // All buffering is deferred to the actually used streambuf
  setp(0, 0); // no put buffer
  setg(0, 0, 0); // no get buffer
}

buffLatexFilter::~buffLatexFilter()
{
}
/*
bool buffLatexFilter::skip_prefix()
{
  char buf[4];
  if (i_sbuf->sgetn(buf, 4) != 4)
    return false;
  if (strncmp(buf, "TOTO", 4))
  {
    // an expection could be thrown here...
    return false;
  }
  i_newline = false;
  return true;
}


int buffLatexFilter::underflow()
{
  if (i_cache == EOF)
  {
    if (i_newline)
      if (!skip_prefix())
	return EOF;

    i_cache = i_sbuf->sbumpc();
    if (i_cache == '\n')
      i_newline = true;
    return i_cache;
  }
  else
    return i_cache;
}

int	buffLatexFilter::uflow()
{
  if (i_cache == EOF)
  {
    if (i_newline)
      if (!skip_prefix())
	return EOF;
    
int rc = i_sbuf->sbumpc();
    if (rc == '\n')
      i_newline = true;
    return rc;
  }
  {
    int rc = i_cache;
    i_cache = EOF;
    return rc;
  }
}
*/
int buffLatexFilter::overflow(int c)
{
  if (c != EOF) {
    if (m_newline)
        m_newline = false;

    m_curline += c;
    if (c == '\n') {
        /// print the line just read
        if( strncmp(m_curline.c_str(), "Overfull", 8) == 0 
            || strncmp(m_curline.c_str(), "Underfull",9) == 0 
            || strncmp(m_curline.c_str(), "LaTeX Warning",13) == 0 )
        {
            if( m_filtermode == WarnOnly || m_filtermode == ErrWarnOnly || m_filtermode ==  Highlight ) {
                JadedHoboConsole::console.SetColor( _fgWarning, JadedHoboConsole::bgMask );
                m_sbuf->sputn("  ",2);
                m_sbuf->sputn(m_curline.c_str(), m_curline.size());
            }
        }
        else if ( m_curline.c_str()[0] == '!') {
            if( m_filtermode == ErrOnly || m_filtermode == ErrWarnOnly || m_filtermode ==  Highlight ) {
                JadedHoboConsole::console.SetColor( _fgErr, JadedHoboConsole::bgMask );
                m_sbuf->sputn("  ",2);
                m_sbuf->sputn(m_curline.c_str(), m_curline.size());
            }
        }
        else if( m_filtermode == Highlight ) {
            JadedHoboConsole::console.SetColor( _fgLatex, JadedHoboConsole::bgMask );
            m_sbuf->sputn("  ",2);
            m_sbuf->sputn(m_curline.c_str(), m_curline.size());
        }
        m_newline = true;
        m_curline = "";
    }
    return c;
  }
  return 0;
}
/*
int buffLatexFilter::sync()
{
  i_sbuf->sync();
  return 0;
}
*/