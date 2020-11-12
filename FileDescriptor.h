#ifndef _FILEDESCRIPTOR_H_
#define _FILEDESCRIPTOR_H_

#include <regex>
#include <string>
#include <unordered_map>
#include <queue>
#include <memory>
#include <atomic>

#include <set>

#include "ThreadPool.h"
#include "util.h"


#include <mutex>
#include <condition_variable>

class FilePattern {
public:
    static std::regex Open_Whole;
    static std::regex Open_Resume;
    static std::regex Open_Unfinish;

    static std::regex Dump_Whole;
    static std::regex Dump_Resume;
    static std::regex Dump_Unfinish;
    static std::regex Dump_BadFile;

    static std::regex Close_Whole;
    static std::regex Close_Resume;
    static std::regex Close_Unfinish;
    static std::regex Close_BadFile;

private:
    FilePattern() = delete;
    FilePattern(const FilePattern &) = delete;
    FilePattern& operator=(const FilePattern &) = delete;
};

class FileDescriptor {
private:
    using ResultData = std::unordered_map<fd_t, std::vector<Status>>;

public:
    FileDescriptor(const FileDescriptor &) = delete;
    FileDescriptor& operator=(const FileDescriptor &) = delete;

public:
    static FileDescriptor*  getInstance();  
    void    process();  
    void    dump();
    void    setProcessId(pid_t pid);
    void    setFilePath(const std::string file);
    void    setProcessThread(unsigned int threads);
    long    processedLine();
    ResultData  getResult();

private:
    static std::vector<std::pair<int,std::string>>    
        doProcess(const std::string, pid_t pid);
    static std::tuple<pid_t, fd_t, std::string, fd_t> 
        regexProcess(const std::regex & pattern, const std::string & line);

private:
    FileDescriptor();


    void    detectEBADF();

    static void    processOpen(const std::string & line, FileDescriptor * instance);
    static void    openWhole(const std::string & line, FileDescriptor * instance);
    static void    openUnfinish(const std::string & line, FileDescriptor * instance);
    static void    openResume(const std::string & line, FileDescriptor * instance);

    static void    processClose(const std::string & line, FileDescriptor * instance);
    static void    closeWhole(const std::string & line, FileDescriptor * instance);
    static void    closeUnfinish(const std::string & line, FileDescriptor * instance);
    static void    closeResume(const std::string & line, FileDescriptor * instance);

    static void    processDump(const std::string & line, FileDescriptor * instance);
    static void    dumpWhole(const std::string & line, FileDescriptor * instance);
    static void    dumpUnfinish(const std::string & line, FileDescriptor * instance);
    static void    dumpResume(const std::string & line, FileDescriptor * instance);

    static void    irrelevantProcess(const std::string &line, FileDescriptor * instance);

private:
    pid_t           mProcessId;
    std::string     mFilePath;
    std::unordered_map<pid_t,std::queue<fd_t>>  mCloseGraph;
    std::unordered_map<pid_t,std::queue<fd_t>>  mOpenGraph;
    std::unordered_map<fd_t, Status>            mMapGraph;
    std::unordered_map<fd_t, std::queue<Status>>mBadFileMap;
    std::unordered_map<fd_t, std::string>       mFileTimeMap;

    //used when multi-thread
    std::mutex              mProcessLock;
    std::mutex              mSuccessLock;
    long                    mProcessLine;
    long                    mMaxProcessLine;
    std::condition_variable mSuccessCond;

    unsigned int            mThreadCnt;
    ThreadPool              *mpThreadPool;

};


#endif
