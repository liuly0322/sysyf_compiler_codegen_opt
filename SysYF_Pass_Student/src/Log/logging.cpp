#include "logging.hpp"

std::stack<bool> LogWriter::log_enable_stack;

int get_env_LOG_level() {
    char *logv = std::getenv("LOGV");
    int env_log_level;
    if (logv)
    {
        std::string string_logv = logv;
        env_log_level = std::stoi(logv);
    }
    else
    {
        env_log_level = 4;
    }
    return env_log_level;
}

std::string Sprintf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    const auto len = vsnprintf(nullptr, 0, fmt, args);
    va_end(args);
    std::string r;
    r.resize(static_cast<size_t>(len) + 1);
    va_start(args, fmt);
    vsnprintf(&r.front(), len + 1, fmt, args);
    va_end(args);
    r.resize(static_cast<size_t>(len));
 
    return r;
}


void print_llvm(std::string filename, std::string llvm_ir, LogLevel level) {
    if (level < get_env_LOG_level()) return;
    std::ofstream output_stream;
    output_stream.open(filename, std::ios::out);
    output_stream << llvm_ir;
    output_stream.close();
}

void LogWriter::operator<(const LogStream &stream) {
    std::ostringstream msg;
    msg << stream.sstream_->rdbuf();
    output_log(msg);
}

void LogWriter::output_log(const std::ostringstream &msg) {
    if (LogWriter::log_enable_stack.empty()) return ;
    if (!LogWriter::log_enable_stack.top()) return ;
    if (log_level_ >= env_log_level) {
        std::cerr << "[" << level2string(log_level_) << "] " 
                    << "(" <<  location_.file_ 
                    << ":" << location_.line_ 
                    << "  "<< location_.func_<<")"
                    << msg.str() << std::endl;
    }
    fflush(stdout);
}
std::string level2string_with_color(LogLevel level) {
    switch (level)
    {
        case DEBUG:
            return "\033[34mDEBUG\033[0m";
            
        case INFO:
            return "\033[32mINFO\033[0m";

        case WARNING:
            return "\033[33mWARNING\033[0m";

        case ERROR:
            return "\033[31mERROR\033[0m";

        default:
            return "";
    }
}
std::string level2string(LogLevel level) {
    switch (level)
    {
        case DEBUG:
            return "DEBUG";
            
        case INFO:
            return "INFO";

        case WARNING:
            return "WARNING";

        case ERROR:
            return "ERROR";

        default:
            return "";
    }
}
std::string get_short_name(const char * file_path) {
    std::string short_file_path = file_path;
    int index = short_file_path.find_last_of('/');

    return short_file_path.substr(index+1);
}


std::string format(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    const auto len = vsnprintf(nullptr, 0, fmt, args);
    va_end(args);
    std::string r;
    r.resize(static_cast<size_t>(len) + 1);
    va_start(args, fmt);
    vsnprintf(&r.front(), len + 1, fmt, args);
    va_end(args);
    r.resize(static_cast<size_t>(len));
 
    return r;
}