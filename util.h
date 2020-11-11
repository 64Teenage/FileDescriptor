#ifndef _UTIL_H_
#define _UTIL_H_

#include <string>
#include <tuple>

#include "threadlog.h"

//using pid_t = size_t;

const unsigned int  PRINTLEN = 6;
using fd_t  = int16_t;

enum class FDSTATUS {
    NONE        = 0,
    NORMAL      = -1,
    CLOSING     = -2,
    CLOSED      = -3,
    USELESS     = -4,
    OPENING     = -5,
    DUMPING     = -6
};


class Stream {
public:
    Stream(): pid(-1){}
    Stream(size_t pid, const std::string time): pid(pid), time(time){}

    void set(size_t pid, const std::string time) {
        this->pid = pid;
        this->time = time;
    }

    std::tuple<size_t,std::string> get() {
        return std::tuple<size_t, std::string>{pid, time};
    }

private:
    size_t      pid;
    std::string time;
};

class Status {
public:
    Status() = default;
    explicit Status(size_t pid, const std::string time, FDSTATUS status): pid(pid), time(time), status(status){}

    void set(size_t pid, const std::string time, FDSTATUS status) {
        this->pid = pid;
        this->time = time;
        this->status = status;
    }

    std::tuple<size_t, std::string, FDSTATUS>
    get() {
        return std::tuple<size_t, std::string, FDSTATUS>(pid, time, status);
    }

    friend bool    operator<(const Status &lhs, const Status & rhs)  {
        return lhs.time < rhs.time;
    }

    friend bool    operator>(const Status &lhs, const Status & rhs)  {
        return lhs.time > rhs.time;
    }

private:
    size_t      pid;
    std::string time;
    FDSTATUS    status;
};





#endif
