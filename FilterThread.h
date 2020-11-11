#ifndef FILTERTHREAD_H_
#define FILTERTHREAD_H_

#include "threadlog.h"

#include <QtCore/QThread>
#include <QtCore/QHash>
#include <QtCore/QQueue>
#include <QtCore/QMetaType>
#include <unordered_map>
#include <queue>
//#include <string>
#include "FileDescriptor.h"

#include "QUtil.h"

class QProcessThread: public QThread {
    Q_OBJECT
public:
    explicit QProcessThread(QObject *parent = 0): QThread(parent){}
    explicit QProcessThread(
        FileDescriptor      *pHandler, 
        const QString       file, 
        const unsigned int  threads
    ) : mpFileDescriptor(pHandler)
      , mFilePath(file)
      , mThreadNum(threads){}

    void initResources(
        FileDescriptor      *pHandler, 
        const QString       file, 
        const unsigned int  threads
    ) {
        mpFileDescriptor = pHandler;
        mFilePath = file;
        mThreadNum = threads;
    }

protected:
    void run() {
        DEG_LOG("process begin xxx");
        std::string strFilePath = mFilePath.toStdString();
        DEG_LOG("FileDescriptor(%p) set File enter %s", mpFileDescriptor, strFilePath.c_str());
        mpFileDescriptor->setFilePath(strFilePath);
        mpFileDescriptor->setProcessId(2038);
        mpFileDescriptor->setProcessThread(mThreadNum);
        //mpFileDescriptor->process();
        DEG_LOG("process end xxx");
        QHash<fd_t,QQueue<Status>> rank;
        emit notify(rank);
    }

signals:
    void    notify(QHash<fd_t,QQueue<Status>>);


private:
    FileDescriptor  *mpFileDescriptor = nullptr;
    QString         mFilePath;
    unsigned int    mThreadNum;

};



class QBarThread: public QThread {
    Q_OBJECT
public:
    explicit QBarThread(QObject *parent = 0): QThread(parent){}
    explicit QBarThread(FileDescriptor * pHandler, const long lines, QObject * parent)
        : mpFileDescriptor(pHandler)
          , mFileLines(lines)
          , QThread(parent){}

    void    initResources(FileDescriptor * pHandler, const long lines) {
        mpFileDescriptor = pHandler;
        mFileLines = lines;
    }

protected:
    void run() {
        long lineBefore = 0;
        while(true) {
            long nlines = mpFileDescriptor->processedLine();
            DEG_LOG("PROCESS LINE: %d", nlines);
            if(nlines == lineBefore) {
                continue;
            } else {
                long schedual = 1.0 * nlines / 2 / mFileLines * 100;
                lineBefore = nlines;
                emit notify(schedual);
            }
            if(nlines >= 2 * mFileLines) {
                break;
            }
        }
    }

signals:
    void notify(long);

private:
    FileDescriptor  *mpFileDescriptor = nullptr;
    long            mFileLines = 0;
};


#endif
