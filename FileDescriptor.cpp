#include <regex>
#include <fstream>

#include <iostream>

#include "FileDescriptor.h"
#include "ThreadPool.h"
#include "HandlerThread.h"


std::regex FilePattern::Open_Whole("^([0-9]*) *([^<]*) .*openat.* *= *(-*[0-9]*).*");
std::regex FilePattern::Open_Resume("^([0-9]*) *([^<]*) .*openat.*resumed> .*= *(-*[0-9]*)");
std::regex FilePattern::Open_Unfinish("^([0-9]*) *([^<]*) .*openat.* <unfinished ...>(.*)");

std::regex FilePattern::Dump_Whole("^([0-9]*) *([^<]*) .*dup\\(([0-9]*)\\) *= *(-*[0-9]*).*");
std::regex FilePattern::Dump_Resume("^([0-9]*) *([^<]*) .*dup.*resumed> .*= *(-*[0-9]*)");
std::regex FilePattern::Dump_Unfinish("^([0-9]*) *([^<]*) .*dup\\(([0-9]*) <.*\\)*");
std::regex FilePattern::Dump_BadFile("^([0-9]*) *([^<]*) .*dup\\(([0-9]*)\\) *= *(-*[0-9]*) .*Bad file descriptor.*");

std::regex FilePattern::Close_Whole("^([0-9]*) *(.*) *close\\(([0-9]*)\\) .*= (-*[0-9]*).*");
std::regex FilePattern::Close_Resume("^([0-9]*) *([^<]*) .*close resumed>.*= *(-*[0-9]*)");
std::regex FilePattern::Close_Unfinish("^([0-9]*) *(.*) .*close\\(([0-9]*) <.*\\)*");
std::regex FilePattern::Close_BadFile("^([0-9]*) *(.*) .*close\\(([0-9]*)\\) .*= (-*[0-9]*) .*Bad file descriptor.*");

/******************* public function ********************************/
FileDescriptor*
FileDescriptor::getInstance() {
    static FileDescriptor instance;
    return &instance;
}

void
FileDescriptor::initResources(
    pid_t               pid,
    const std::string   file,
    unsigned            threads
) {
    setProcessId(pid);
    setFilePath(file);
    setProcessThread(threads);

    mProcessLine = 0;
    mMaxProcessLine = 0;

    mCloseGraph.clear();
    mOpenGraph.clear();
    mBadFileMap.clear();
    mMapGraph.clear();
    for(fd_t fd = 0; fd < 1024; ++fd) {
        mMapGraph[fd] = Status(-1, "", FDSTATUS::CLOSED);
    }
    DEG_LOG("File Descriptor init ....");
}

void
FileDescriptor::process() {
    mpThreadPool = ThreadPool::getInstance(mThreadCnt);
    mpThreadPool->adjust(mThreadCnt);
    detectEBADF();

    std::cout<<"Detect Bad File Descriptor end xxx"<<std::endl;
    DEG_LOG("Detect Bad File Descriptor End...");

    
    std::ifstream in(mFilePath, std::ios::in);
    if(!in.is_open()) {
        std::cerr<<mFilePath<<" do not exist!"<<std::endl;
        return ;
    } else {
        std::string line;
        while(std::getline(in, line)) {
            if(line.find("openat") != std::string::npos) {
                mpThreadPool->enqueue(processOpen,  line, this);
            } else if(line.find("close") != std::string::npos) {
                mpThreadPool->enqueue(processClose, line, this);
            } else if(line.find("dup") != std::string::npos) {
                mpThreadPool->enqueue(processDump, line, this);
            } else {
                mpThreadPool->enqueue(irrelevantProcess, line, this);
            }
        }
    }
    
}

long 
FileDescriptor::processedLine() {
    if(mMaxProcessLine == 0) {
        return mProcessLine;
    } else {
        return mMaxProcessLine - mProcessLine + mMaxProcessLine;
    }
}

FileDescriptor::ResultData
FileDescriptor::getResult() {
    {
        //wait for threadpool to finish all jobs
        std::unique_lock<std::mutex> lock(mSuccessLock);
        mSuccessCond.wait(lock, [&](){return !mProcessLine;});
    }

    ResultData data;
    for(auto it = mBadFileMap.begin(); it != mBadFileMap.end(); ++it) {
        std::priority_queue<Status, std::vector<Status>, std::greater<Status>> rank;
        while(!it->second.empty()) {
            auto node = it->second.front();
            it->second.pop();
            if(std::get<1>(node.get()) > mFileTimeMap[it->first]) {
                continue;
            }
            if(rank.size() == PRINTLEN) {
                rank.push(node);
                rank.pop();
            } else {
                rank.push(node);
            }
        }

        std::vector<Status> status;
        while(!rank.empty()) {
            auto node = rank.top();
            rank.pop();
            status.push_back(node);
        }
        data.insert({it->first, status});
    }
    return data;
}


/******************* private function ********************************/
FileDescriptor::FileDescriptor(
) : mProcessId(-1)
  , mThreadCnt(1)
  , mpThreadPool(nullptr)
  , mProcessLine(0)
  , mMaxProcessLine(0) {
    mCloseGraph.clear();
    mOpenGraph.clear();
    mBadFileMap.clear();
    for(fd_t fd = 0; fd < 1024; ++fd) {
        mMapGraph[fd] = Status(-1, "", FDSTATUS::CLOSED);
    }
}

void
FileDescriptor::setProcessId(
    pid_t   pid
) {
    mProcessId = pid;
}

void
FileDescriptor::setFilePath(
    const std::string   file
) {
    mFilePath = file;
    DEG_LOG("set process file: %s", mFilePath.c_str());
}

void
FileDescriptor::setProcessThread(
    unsigned int    threads
) {
    mThreadCnt = threads > std::thread::hardware_concurrency() ? std::thread::hardware_concurrency() : threads;
    DEG_LOG("set process thread: %d", mThreadCnt);
}

std::tuple<pid_t, fd_t, std::string, fd_t>
FileDescriptor::regexProcess(
    const std::regex &  pattern,
    const std::string & line
) {

    std::smatch result{};
    if(std::regex_match(line, result, pattern)) {
        pid_t       pid = std::stoi(result[1].str());
        std::string time = result[2].str();
        fd_t        fd = -1;
        fd_t        status = -2;
        if(!result[3].str().empty()) {
            fd = std::stoi(result[3].str());
        }
        if (result.size() == 5) {
            status = std::stoi(result[4].str());
        }

        return std::tuple<pid_t, fd_t, std::string, fd_t>(pid, fd, time, status);
    } else {
        return std::tuple<pid_t, fd_t, std::string, fd_t>(-1, -1, "", -2);
    }
}

void
FileDescriptor::detectEBADF() {

    std::ifstream in(mFilePath, std::ios::in);

    if(!in.is_open()) {
        std::cerr<<mFilePath<<" does not exist!"<<std::endl;
        return ;
    } else {
        std::string line;
        //int num = 0;

        mProcessLine = 0;
        mMaxProcessLine = 0;

        //ThreadPool * pPoolInstance = ThreadPool::getInstance();

        HandlerThread handler;
        

        while(std::getline(in, line)) {
            

            auto res = mpThreadPool->enqueue(doProcess, line, mProcessId);

            handler.enqueue([&,res](){
                auto result = res.get();
                for(auto & element : result) {
                    mBadFileMap.insert({element.first, std::queue<Status>()});
                    mFileTimeMap.insert(element);
                }
                ++mProcessLine;
            });
        }
    }

    mMaxProcessLine = mProcessLine;
    DEG_LOG("max line is %d", mMaxProcessLine);
}

std::vector<std::pair<int,std::string>> 
FileDescriptor::doProcess(
    const std::string  line,
    pid_t               spid
) {
            std::vector<std::pair<int,std::string>> res;
            auto result = regexProcess(FilePattern::Close_BadFile, line);
            if(std::get<3>(result) != -2) {
                pid_t   pid      = std::get<0>(result);
                fd_t    fd       = std::get<1>(result);
                std::string time = std::get<2>(result);
                if((pid == spid)) {
                    res.push_back(std::pair<int,std::string>(fd, time));
                }
            }
            result = regexProcess(FilePattern::Dump_BadFile, line);
            if(std::get<3>(result) != -2) {
                pid_t   pid      = std::get<0>(result);
                fd_t    fd       = std::get<1>(result);
                std::string time = std::get<2>(result);
                if((pid == spid) ) {
                    res.push_back(std::pair<int,std::string>(fd, time));
                }
            }
            return res;
}

void
FileDescriptor::processOpen(
    const std::string & line,
    FileDescriptor*     handle
) {
    //std::cout<<line<<std::endl;
    if(line.find("unfinished") != std::string::npos) {
        openUnfinish(line, handle);
    } else if(line.find("resumed") != std::string::npos) {
        openResume(line, handle);
    } else {
        openWhole(line, handle);
    }
}

void
FileDescriptor::openWhole(
    const std::string   &line,
    FileDescriptor*     handle
) {
    auto res = regexProcess(FilePattern::Open_Whole, line);
    pid_t   pid         = std::get<0>(res);
    fd_t    fd          = std::get<1>(res);
    std::string time    = std::get<2>(res);

    Status status(pid, time, FDSTATUS::OPENING);

    if(handle->mBadFileMap.count(fd) > 0) {
        std::lock_guard<std::mutex> lock(handle->mProcessLock);
        handle->mBadFileMap[fd].push(status);
    }

    /*
    {
        std::lock_guard<std::mutex> lock(handle->mProcessLock);
        if(handle->mBadFileMap.count(fd) > 0) {
            handle->mBadFileMap[fd].push(status);
        }
    }
    */

    {
        std::lock_guard<std::mutex> lock(handle->mSuccessLock);
        --handle->mProcessLine;
        if(handle->mProcessLine == 0) {
            handle->mSuccessCond.notify_all();
        }
    }
}

void
FileDescriptor::openUnfinish(
    const std::string   &line,
    FileDescriptor*     handle
) {
    auto res = regexProcess(FilePattern::Open_Unfinish, line);
    //to do

    {
        std::lock_guard<std::mutex> lock(handle->mSuccessLock);
        --handle->mProcessLine;
        
        if(handle->mProcessLine == 0) {
            handle->mSuccessCond.notify_all();
        }
    }


}

void 
FileDescriptor::openResume(
    const std::string   &line,
    FileDescriptor*     handle
) {
    auto res = regexProcess(FilePattern::Open_Resume, line);
    pid_t   pid         = std::get<0>(res);
    fd_t    fd          = std::get<1>(res);
    std::string time    = std::get<2>(res);


    Status status(pid, time, FDSTATUS::OPENING);

    if(handle->mBadFileMap.count(fd) > 0) {
        std::lock_guard<std::mutex> lock(handle->mProcessLock);
        handle->mBadFileMap[fd].push(status);
    }

    /*
    {
        std::lock_guard<std::mutex> lock(handle->mProcessLock);
        if(handle->mBadFileMap.count(fd) > 0) {
            handle->mBadFileMap[fd].push(status);
        }
    }
    */

    {
        std::lock_guard<std::mutex> lock(handle->mSuccessLock);
        --handle->mProcessLine;
        if(handle->mProcessLine == 0) {
            handle->mSuccessCond.notify_all();
        }
    }

}


void
FileDescriptor::processClose(
    const std::string   &line,
    FileDescriptor*     handle
) {
    if(line.find("unfinished") != std::string::npos) {
        closeUnfinish(line, handle);
    } else if(line.find("resumed") != std::string::npos) {
        closeResume(line, handle);
    } else {
        closeWhole(line, handle);
    }
}

void
FileDescriptor::closeWhole(
    const std::string   &line,
    FileDescriptor*     handle
) {
    auto res = regexProcess(FilePattern::Close_Whole, line);
    pid_t   pid      = std::get<0>(res);
    fd_t    fd       = std::get<1>(res);
    std::string time = std::get<2>(res);
    //int     status   = std::get<3>(res);

    Status status(pid, time, FDSTATUS::CLOSED);

    if(handle->mBadFileMap.count(fd) > 0) {
        std::lock_guard<std::mutex> lock(handle->mProcessLock);
        handle->mBadFileMap[fd].push(status);
    }

    /*
    {
        std::lock_guard<std::mutex> lock(handle->mProcessLock);
        if(handle->mBadFileMap.count(fd) > 0) {
            handle->mBadFileMap[fd].push(status);
        }
    }
    */

    {
        std::lock_guard<std::mutex> lock(handle->mSuccessLock);
        --handle->mProcessLine;
        if(handle->mProcessLine == 0) {
            handle->mSuccessCond.notify_all();
        }
    }

}

void
FileDescriptor::closeUnfinish(
    const std::string   &line,
    FileDescriptor*     handle
) {
    auto res = regexProcess(FilePattern::Close_Unfinish, line);
    pid_t   pid      = std::get<0>(res);
    fd_t    fd       = std::get<1>(res);
    std::string time = std::get<2>(res);
    //int     status   = std::get<3>(res);

    Status status(pid, time, FDSTATUS::CLOSED);

    if(handle->mBadFileMap.count(fd) > 0) {
        std::lock_guard<std::mutex> lock(handle->mProcessLock);
        handle->mBadFileMap[fd].push(status);
    }
    /*
    {
        std::lock_guard<std::mutex> lock(handle->mProcessLock);
        if(handle->mBadFileMap.count(fd) > 0) {
            handle->mBadFileMap[fd].push(status);
        }
    }
    */

    {
        std::lock_guard<std::mutex> lock(handle->mSuccessLock);
        --handle->mProcessLine;
        if(handle->mProcessLine == 0) {
            handle->mSuccessCond.notify_all();
        } 
    }
}

void 
FileDescriptor::closeResume(
    const std::string   &line,
    FileDescriptor*     handle
) {
    auto res = regexProcess(FilePattern::Close_Resume, line);
    //to do
    {
        std::lock_guard<std::mutex> lock(handle->mSuccessLock);
        --handle->mProcessLine;
        if(handle->mProcessLine == 0) {
            handle->mSuccessCond.notify_all();
        }    
    }
}

void
FileDescriptor::processDump(
    const std::string   &line,
    FileDescriptor*     handle
) {
    if(line.find("unfinished") != std::string::npos) {
        dumpUnfinish(line, handle);
    } else if(line.find("resumed") != std::string::npos) {
        dumpResume(line, handle);
    } else {
        dumpWhole(line, handle);
    }
}

void
FileDescriptor::dumpWhole(
    const std::string   &line,
    FileDescriptor*     handle
) {
    auto res = regexProcess(FilePattern::Dump_Whole, line);
    pid_t   pid      = std::get<0>(res);
    fd_t    fd       = std::get<1>(res);
    std::string time = std::get<2>(res);
    fd_t    dumpfd   = std::get<3>(res);


    Status status(pid, time, FDSTATUS::DUMPING);

    if(handle->mBadFileMap.count(fd) > 0) {
        std::lock_guard<std::mutex> lock(handle->mProcessLock);
        handle->mBadFileMap[fd].push(status);
    }

    /*
    {
        std::lock_guard<std::mutex> lock(handle->mProcessLock);
        if(handle->mBadFileMap.count(fd) > 0) {
            handle->mBadFileMap[fd].push(status);
        }
    }
    */

    status = Status(pid, time, FDSTATUS::OPENING);
    if(handle->mBadFileMap.count(dumpfd) > 0) {
        std::lock_guard<std::mutex> lock(handle->mProcessLock);
        handle->mBadFileMap[dumpfd].push(status);
    }

    /*
    {
        std::lock_guard<std::mutex> lock(handle->mProcessLock);
        if(handle->mBadFileMap.count(dumpfd) > 0) {
            handle->mBadFileMap[dumpfd].push(status);
        }
    }
    */

    {
        std::lock_guard<std::mutex> lock(handle->mSuccessLock);
        --handle->mProcessLine;
        if(handle->mProcessLine == 0) {
            handle->mSuccessCond.notify_all();
        }
    }

}

void
FileDescriptor::dumpUnfinish(
    const std::string   &line,
    FileDescriptor*     handle
) {
    auto res = regexProcess(FilePattern::Dump_Unfinish, line);
    pid_t   pid      = std::get<0>(res);
    fd_t    fd       = std::get<1>(res);
    std::string time = std::get<2>(res);

    Status status(pid, time, FDSTATUS::DUMPING);

    if(handle->mBadFileMap.count(fd) > 0) {
        std::lock_guard<std::mutex> lock(handle->mProcessLock);
        handle->mBadFileMap[fd].push(status);
    }

    /*
    {
        std::lock_guard<std::mutex> lock(handle->mProcessLock);
        if(handle->mBadFileMap.count(fd) > 0) {
            handle->mBadFileMap[fd].push(status);
        }
    }
    */

    {
        std::lock_guard<std::mutex> lock(handle->mSuccessLock);
        --handle->mProcessLine;
        if(handle->mProcessLine == 0) {
            handle->mSuccessCond.notify_all();
        }
    }
}

void 
FileDescriptor::dumpResume(
    const std::string   &line,
    FileDescriptor*     handle
) {
    auto res = regexProcess(FilePattern::Dump_Resume, line);
    pid_t   pid      = std::get<0>(res);
    fd_t    fd       = std::get<1>(res);
    std::string time = std::get<2>(res);

    Status status(pid, time, FDSTATUS::OPENING);
    if(handle->mBadFileMap.count(fd) > 0) {
        std::lock_guard<std::mutex> lock(handle->mProcessLock);
        handle->mBadFileMap[fd].push(status);
    }

    /*
    {
        std::lock_guard<std::mutex> lock(handle->mProcessLock);
        if(handle->mBadFileMap.count(fd) > 0) {
            handle->mBadFileMap[fd].push(status);
        }
    }
    */

    {
        std::lock_guard<std::mutex> lock(handle->mSuccessLock);
        --handle->mProcessLine;
        if(handle->mProcessLine == 0) {
            handle->mSuccessCond.notify_all();
        }
    }
}

void    
FileDescriptor::irrelevantProcess(
    const std::string &line, 
    FileDescriptor * handle
) {
    {
        std::lock_guard<std::mutex> lock(handle->mSuccessLock);
        --handle->mProcessLine;
        if(handle->mProcessLine == 0) {
            handle->mSuccessCond.notify_all();
        }
    }
}
