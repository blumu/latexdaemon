/// inspired from the tutorial at http://www.inf.uni-konstanz.de/~kuehl/c++/iostream/

#ifndef _LATEXFILTERS_H_
#define _LATEXFILTERS_H_

#include <iostream>

using namespace std;

// constants corresponding to the different possible output filtering mode
enum FILTER  { Raw = 0, Highlight, ErrOnly, WarnOnly, ErrWarnOnly };


class buffLatexFilter : public streambuf
{
private:
    streambuf       *m_sbuf;    // the actual streambuf used to read and write chars
    bool            m_newline;  // remember whether we are at a new line
    int             m_cache;    // may cache a read character
    string          m_curline;   // line currently read

    FILTER          m_filtermode;
    //bool            skip_prefix();

protected:
    int	overflow(int);
    //int	underflow();
    //int	uflow();
    //int	sync();

public:   
    buffLatexFilter(streambuf *sb, FILTER filtermode = Highlight);
    ~buffLatexFilter();

};


// An output stream class which associates itself to the ostreambuff class
class ostreamLatexFilter: public ostream
{
public:
    ostreamLatexFilter::ostreamLatexFilter(streambuf *sb, FILTER filter = Highlight):
      ostream(new buffLatexFilter(sb, filter))
    {
    }

    ~ostreamLatexFilter()
    {
      delete rdbuf();
    }
};


#endif