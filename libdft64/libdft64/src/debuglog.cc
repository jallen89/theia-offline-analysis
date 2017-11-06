#include <map>
#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <string>
#include <iostream>
#include <sstream>
#include <sys/syscall.h>
#include <algorithm>

typedef std::map<std::pair<pid_t, pid_t>, FILE*> fhandle_mapt;
fhandle_mapt fhandles;
static std::string nameprefix;

void loginit(const char* logprefix) {
    nameprefix = logprefix;
    fhandles.clear();
}

FILE* getfhandle(pid_t pid, pid_t tid) {
    if (fhandles.count(std::make_pair(pid, tid)) == 0) {
        std::ostringstream sout;
        sout << nameprefix << pid << "-" << tid << ".log";
        fhandles[std::make_pair(pid, tid)] = fopen(sout.str().c_str(), "w");
    }
    return fhandles[std::make_pair(pid, tid)];
}

int logprintf(const char* format, ...) {
    pid_t pid = getpid();
    pid_t tid = syscall(SYS_gettid);
    FILE* f = getfhandle(pid, tid);
    va_list args;
    va_start(args, format);
    int ret = vfprintf(f, format, args);
    va_end(args);
    fflush(f);
    return ret;
}

void logflush() {
    pid_t pid = getpid();
    pid_t tid = syscall(SYS_gettid);
    fflush(getfhandle(pid,tid));
}

void logfini() {
    for (fhandle_mapt::iterator it = fhandles.begin(); it != fhandles.end(); it++)
        fclose(it->second);
}
