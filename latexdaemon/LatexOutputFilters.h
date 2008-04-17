/// inspired from the tutorial at http://www.inf.uni-konstanz.de/~kuehl/c++/iostream/

#ifndef _LATEXFILTERS_H_
#define _LATEXFILTERS_H_

#include <iostream>

using namespace std;

#include "tstring.h"

// constants corresponding to the different possible output filtering mode
enum FILTER  { Raw = 0, Highlight, ErrOnly, WarnOnly, ErrWarnOnly };


class buffLatexFilter : public tstreambuf
{
private:
    tstreambuf       *m_sbuf;    // the actual streambuf used to read and write chars
    tstring          m_curline;  // line currently read

    FILTER          m_filtermode;
    bool            m_lookforlinenumber;

protected:
    int overflow(int);
    int sync();

    void put_newline_buff();
    void put_buff();

public:
    buffLatexFilter(tstreambuf *sb, FILTER filtermode = Highlight, int bsize = 0);
    ~buffLatexFilter();

};


// An output stream class which associates itself to the ostreambuff class
class tostreamLatexFilter: public tostream
{
public:
    tostreamLatexFilter::tostreamLatexFilter(tstreambuf *sb, FILTER filter = Highlight, int bsize = 0):
      tostream(new buffLatexFilter(sb, filter, bsize))
    {
    }

    ~tostreamLatexFilter()
    {
      delete rdbuf();
    }
};


#endif