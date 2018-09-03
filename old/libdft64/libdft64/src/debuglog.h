#pragma once
#include <unistd.h>

void loginit(const char* logprefix);

FILE* getfhandle(pid_t pid);

int logprintf(const char* format, ...);

void logflush();

void logfini();
