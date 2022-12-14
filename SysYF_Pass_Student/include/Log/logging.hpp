#ifndef LOGGING_HPP
#define LOGGING_HPP

#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <stdarg.h>
#include <stack>

std::string format(const char *fmt, ...);

int get_env_LOG_level();

std::string Sprintf(const char *fmt, ...);

enum LogLevel
{
    DEBUG = 0,
    INFO,
    WARNING,
    ERROR
};

void print_llvm(std::string file, std::string output_IR, LogLevel level=LogLevel::DEBUG);

struct LocationInfo
{
    LocationInfo(std::string file, int line, const char *func) : file_(file), line_(line), func_(func) {}
    ~LocationInfo() = default;

    std::string file_;
    int line_;
    const char *func_;
};
class LogStream;
class LogWriter;


class LogWriter
{
public:
    static std::stack<bool> log_enable_stack;
    static void Push(bool en) {
        log_enable_stack.push(en);
    }
    static void Pop() {
        if (log_enable_stack.empty()) {
            return;
        }
        log_enable_stack.pop();
    }
    LogWriter(LocationInfo location, LogLevel loglevel)
        : location_(location), log_level_(loglevel)
    {
        char *logv = std::getenv("LOGV");
        if (logv)
        {
            std::string string_logv = logv;
            env_log_level = std::stoi(logv);
        }
        else
        {
            env_log_level = 4;
        }
    };

    void operator<(const LogStream &stream);

private:
    void output_log(const std::ostringstream &g);
    LocationInfo location_;
    LogLevel log_level_;
    int env_log_level;
};

class LogStream
{
public:
    LogStream() { sstream_ = new std::stringstream(); }
    ~LogStream() {
        delete sstream_;
    }

    template <typename T>
    LogStream &operator<<(const T &val) noexcept
    {
        (*sstream_) << val;
        return *this;
    }

    friend class LogWriter;

private:
    std::stringstream *sstream_;
};

std::string level2string(LogLevel level);
std::string level2string_with_color(LogLevel level);
std::string get_short_name(const char *file_path);

#define __FILESHORTNAME__ get_short_name(__FILE__)
#define LOG_IF(level) \
    LogWriter(LocationInfo(__FILE__, __LINE__, __FUNCTION__), level) < LogStream()
#define LOG(level) LOG_##level
#define LOG_DEBUG LOG_IF(DEBUG)
#define LOG_INFO LOG_IF(INFO)
#define LOG_WARNING LOG_IF(WARNING)
#define LOG_ERROR LOG_IF(ERROR)
#define LOG_ENABLE LogWriter::Push(true);
#define LOG_DISABLE LogWriter::Push(false);
#define LOG_POP LogWriter::Pop();

#endif
