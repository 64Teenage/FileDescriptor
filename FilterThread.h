#ifndef FILTERTHREAD_H_
#define FILTERTHREAD_H_

#include "threadlog.h"

#include <QtCore/QThread>
#include <QtCore/QHash>
#include <QtCore/QQueue>
#include <QtCore/QVector>
#include <QtCore/QMetaType>
#include <unordered_map>
#include <queue>
//#include <string>
#include "FileDescriptor.h"

#include "QUtil.h"

struct ResultData {
    QHash<fd_t,QVector<Status>>  mapGraph;
    operator QHash<fd_t,QVector<Status>> () const {
        return mapGraph;
    }
};

Q_DECLARE_METATYPE(ResultData);

class QProcessThread: public QThread {
    Q_OBJECT
public:
    explicit QProcessThread(QObject *parent = 0): QThread(parent){
        qRegisterMetaType<ResultData>("ResultData");
    }
    explicit QProcessThread(
        FileDescriptor      *pHandler, 
        const QString       file, 
        const unsigned int  threads,
        QObject             *parent = 0
    ) : QThread(parent)
      , mpFileDescriptor(pHandler)
      , mFilePath(file)
      , mThreadNum(threads){
          qRegisterMetaType<ResultData>("ResultData");
      }

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
        mpFileDescriptor->process();
        auto res = mpFileDescriptor->getResult();
        DEG_LOG("process end xxx");
        ResultData data;
        for(auto it = res.begin(); it != res.end(); ++it) {
            QVector<Status> rank = QVector<Status>::fromStdVector(it->second);
            data.mapGraph.insert(it->first, rank);
        }
        emit notify(data);
    }

signals:
    void    notify(ResultData);


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
        : QThread(parent)
        , mpFileDescriptor(pHandler)
        , mFileLines(lines){}

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
            if(nlines >= 2 * mFileLines) {
                long schedual = 1.0 * nlines / 2 / mFileLines * 100;
                emit notify(schedual);
                break;
            }
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
