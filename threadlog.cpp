#include <cstdio>
#include <ctime>
#include <cstdarg>
#include <cstring>

#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>

#include <sys/time.h>

#ifndef __linux__
#include <windows.h>

#else
#include <sys/syscall.h>
#endif

#include "threadlog.h"
#include "HandlerThread.h"

static bool isFirst=true;
pthread_mutex_t	logMutex;

#ifndef __linux__
const static char * pLogPath = "D:/";
#else
struct passwd * pw = getpwuid(getuid());
const static char * pLogPath = pw->pw_dir;
#endif

char strFilePath[MAXLOGLEN] = {0};

const char *	get_filename(const char * pFilePath)
{
    const char * pFileName = strrchr(pFilePath, static_cast<int>('/'));
    if (NULL != pFileName)
    {
        pFileName += 1;
    }
    else
    {
        pFileName = pFilePath;
    }
    return pFileName;
}

void    log_dbg_print(const char * pFuncName, const char * pFormat, ...)
{
    char str_buffer[MAXLOGLEN];

    std::va_list args;
    va_start(args, pFormat);
    vsnprintf(str_buffer, sizeof(str_buffer), pFormat, args);
    va_end(args);

    struct timeval tmv = {0};
    time_t now = time(0);
    gettimeofday(&tmv, NULL);
    struct tm * pNow = localtime(&now);

    pid_t pid = getpid();

#ifndef __linux__
    pid_t tid = GetCurrentThreadId();
#else
    pid_t tid = syscall(SYS_gettid);
#endif


    char strtime[MAXLOGLEN];
    snprintf(strtime, sizeof(strtime), "%4d-%02d-%2d %02d:%02d:%02d.%06ld", (1900 + pNow->tm_year), (1 + pNow->tm_mon), pNow->tm_mday, pNow->tm_hour, pNow->tm_min, pNow->tm_sec, tmv.tv_usec);

    char strLine[MAXLOGLEN];
    snprintf(strLine, sizeof(strLine), "%s %d %d %s: %s\n", strtime, pid, tid, pFuncName, str_buffer);

    pthread_mutex_lock(&logMutex);
    FILE * fp = NULL;
    if (isFirst)
    {
        strcpy(strFilePath, pLogPath);
        strcat(strFilePath, "/tombstone");
        fp = fopen(strFilePath, "w");
        isFirst = false;
    }
    else
    {
        fp = fopen(strFilePath, "a");
    }
    if (fp == NULL)
    {
        pthread_mutex_unlock(&logMutex);
        return ;
    }
    fputs(strLine, fp);
    fclose(fp);
    fp = NULL;
    pthread_mutex_unlock(&logMutex);
}
