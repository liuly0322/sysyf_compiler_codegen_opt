#ifndef _SYSYF_ERROR_REPORTER_H_
#define _SYSYF_ERROR_REPORTER_H_

#include "SyntaxTree.h"
#include <deque>
#include <iostream>
#include <vector>

class ErrorReporter {
  public:
    using Position = SyntaxTree::Position;
    explicit ErrorReporter(std::ostream &error_stream);

    void error(Position pos, const std::string &msg);
    void warn(Position pos, const std::string &msg);

  protected:
    virtual void report(Position pos, const std::string &msg,
                        const std::string &prefix);

  private:
    std::ostream &err;
};

#endif // _SYSYF_ERROR_REPORTER_H_
