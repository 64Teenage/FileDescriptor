#ifndef FILTERWIDGET_H
#define FILTERWIDGET_H

#include <QtWidgets/QWidget>
#include <QtCharts/QChartGlobal>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QProgressBar>
#include <QtCore/QThread>

#include "FileDescriptor.h"
#include "HandlerThread.h"
#include "FilterThread.h"

QT_BEGIN_NAMESPACE
class QComboBox;
class QCheckBox;
QT_END_NAMESPACE

QT_CHARTS_BEGIN_NAMESPACE
class QChartView;
class QChart;
QT_CHARTS_END_NAMESPACE

QT_CHARTS_USE_NAMESPACE


enum class PROCESSMODE {
    FILEDESCRIPTORMATCH,
    BADFILEDESCRIPTOR
};
Q_DECLARE_METATYPE(PROCESSMODE);



class FilterWidget : public QWidget
{
    Q_OBJECT
private:

public:
    FilterWidget(QWidget *parent = 0);
    ~FilterWidget();

private Q_SLOTS:
    //void        updateUI();

    void        processBoxChanged();
    void        threadBoxChanged();
    void        logFilePathChanged();
    void        processButtonClicked();
    void        processBarChanged(long val);
    void        processDescriptorChanged(ResultData);

private:
    void            connectSignal();
    QComboBox*      createProcessBox() const;
    QComboBox*      createThreadBox() const;
    QPushButton*    createProcessButton() const;
    QProgressBar*   createProcessBar() const;

    void            initUIResources();

private:
    void            getProcessLine(const QString strFilePath);

private:
    QComboBox       *mProcessComboBox   = nullptr;
    QComboBox       *mThreadComboBox    = nullptr;
    QPushButton     *mFilePathButton    = nullptr;
    QPushButton     *mProcessButton     = nullptr;
    QProgressBar    *mProcessBar        = nullptr;

private:
    FileDescriptor  *mFileDescriptor = nullptr;
    HandlerThread   *mpProcessHandler = nullptr;
    QBarThread      *mpBarThread = nullptr;
    QProcessThread  *mpProcessThread = nullptr;
    unsigned int    mThreadNum;
    unsigned int    mProcessLine;
    QString         mFilePath;

};



#endif // FILTERWIDGET_H
