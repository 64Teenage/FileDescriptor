#include "filterwidget.h"
#include "FilterThread.h"

#include <fstream>

#include <QtCharts/QChartView>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QFileDialog>

#include "HandlerThread.h"

FilterWidget::FilterWidget(QWidget *parent)
: QWidget(parent)
, mProcessComboBox(createProcessBox())
, mThreadComboBox(createThreadBox())
, mFilePathButton(new QPushButton())
, mProcessButton(createProcessButton())
, mpProcessHandler(new HandlerThread())
, mProcessBar(createProcessBar())
, mpBarThread(new QBarThread())
, mpProcessThread(new QProcessThread())
{
    connectSignal();

    QGridLayout *pBaseLayout = new QGridLayout();
    QHBoxLayout *pSettingLayout = new QHBoxLayout();
    pSettingLayout->addWidget(new QLabel("线程"));
    pSettingLayout->addWidget(mThreadComboBox);
    pSettingLayout->addWidget(new QLabel("功能"));
    pSettingLayout->addWidget(mProcessComboBox);
    pSettingLayout->addWidget(new QLabel("log"));
    pSettingLayout->addWidget(mFilePathButton);
    pSettingLayout->addWidget(mProcessButton);
    //pSettingLayout->addStretch();

    pBaseLayout->addLayout(pSettingLayout, 0, 0, 1, 3);
    pBaseLayout->addWidget(mProcessBar, 1, 0);
    setLayout(pBaseLayout);

    initUIResources();

}

FilterWidget::~FilterWidget(){
    if(mProcessComboBox) {
        delete mProcessComboBox;
        mProcessComboBox = nullptr;
    }

    if(mThreadComboBox) {
        delete mThreadComboBox;
        mThreadComboBox = nullptr;
    }

    if(mFilePathButton) {
        delete mFilePathButton;
        mFilePathButton = nullptr;
    }

    if(mProcessButton) {
        delete mProcessButton;
        mProcessButton = nullptr;
    }

    if(mpProcessHandler) {
        delete mpProcessHandler;
        mpProcessHandler = nullptr;
    }

    if(mpBarThread) {
        delete mpBarThread;
        mpBarThread = nullptr;
    }

    if(mpProcessThread) {
        delete mpProcessThread;
        mpProcessThread = nullptr;
    }

}

void
FilterWidget::initUIResources() {
    processBoxChanged();
    threadBoxChanged();
}

void
FilterWidget::connectSignal(){
    connect(mProcessComboBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &FilterWidget::processBoxChanged);
    connect(mThreadComboBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &FilterWidget::threadBoxChanged);
    connect(mFilePathButton, static_cast<void (QPushButton::*)(bool)>(&QPushButton::clicked),
            this, &FilterWidget::logFilePathChanged);
    connect(mProcessButton, static_cast<void (QPushButton::*)(bool)>(&QPushButton::clicked),
            this, &FilterWidget::processButtonClicked);
    connect(mpBarThread, static_cast<void (QBarThread::*)(long)>(&QBarThread::notify),
            this, &FilterWidget::processBarChanged);
    connect(mpProcessThread, static_cast<void (QProcessThread::*)(ResultData)>(&QProcessThread::notify),
            this, &FilterWidget::processDescriptorChanged);
    DEG_LOG("Connect Signal Success");
}

QComboBox *
FilterWidget::createProcessBox() const {
    // settings layout
    QComboBox *processComboBox = new QComboBox();
    processComboBox->addItem("DescriptorMatch", QVariant::fromValue(PROCESSMODE::FILEDESCRIPTORMATCH));
    processComboBox->addItem("BadDescriptor", QVariant::fromValue(PROCESSMODE::BADFILEDESCRIPTOR));
    DEG_LOG("Create ProcessBox: %p, Success", processComboBox);
    processComboBox->setCurrentIndex(0);
    return processComboBox;
}

QComboBox*
FilterWidget::createThreadBox() const {
    QComboBox *threadComboBox = new QComboBox();
    auto nThreads = std::thread::hardware_concurrency();
    for(unsigned int iThreads = 1;  iThreads <= nThreads; ++iThreads) {
        QString item = QString::number(iThreads).append("/").append(QString::number(nThreads));
        threadComboBox->addItem(item, QVariant::fromValue(iThreads));
    }
    threadComboBox->setCurrentIndex(0);
    DEG_LOG("Create ThreadBox: %p, Success.", threadComboBox);
    return threadComboBox;
}
QPushButton*    
FilterWidget::createProcessButton() const {
    QPushButton *processButton = new QPushButton();
    processButton->setText("process");
    DEG_LOG("Create ProcessButton: %p, Success", processButton);
    return processButton;
}

QProgressBar*
FilterWidget::createProcessBar() const {
    QProgressBar *processBar = new QProgressBar();
    processBar->setMinimum(0);
    processBar->setMaximum(100);
    processBar->setValue(0);
    return processBar;
}

void
FilterWidget::processBoxChanged(){
    PROCESSMODE processMode = static_cast<PROCESSMODE>(mProcessComboBox->itemData(mProcessComboBox->currentIndex()).toInt());
    switch(processMode) {
    case PROCESSMODE::BADFILEDESCRIPTOR:
        mFileDescriptor = FileDescriptor::getInstance();
        break;
    case PROCESSMODE::FILEDESCRIPTORMATCH:
        //to do
        mFileDescriptor = FileDescriptor::getInstance();
        break;
    default:
        //to do
        mFileDescriptor = FileDescriptor::getInstance();
        break;
    }
    DEG_LOG("Create Process Instance: %p", mFileDescriptor);
}

void
FilterWidget::threadBoxChanged() {
        mThreadNum = static_cast<unsigned int>(mThreadComboBox->itemData(mThreadComboBox->currentIndex()).toInt());
        DEG_LOG("threadBox set Threads %d", mThreadNum);
}

void
FilterWidget::logFilePathChanged() {
        mFilePath = QFileDialog::getOpenFileName(this, "选择文件", "C:");
        mFilePathButton->setText(mFilePath);

        auto res = mpProcessHandler->enqueue([&](){
            mProcessLine = 0;
            std::ifstream in(mFilePath.toStdString());
            if(in.is_open()) {
                std::string line;
                while(getline(in, line)) {
                    ++mProcessLine;
                }
            }
        });

        DEG_LOG("log file changed %s", mFilePath.toStdString().c_str());
}

void
FilterWidget::processBarChanged(long val) {
    mProcessBar->setValue( val);
}

void
FilterWidget::processDescriptorChanged(ResultData data) {
    DEG_LOG("receive process end signal");
    mProcessComboBox->setDisabled(false);
    mThreadComboBox->setDisabled(false);
    mFilePathButton->setDisabled(false);
}

void
FilterWidget::processButtonClicked() {
    DEG_LOG("set ui disable begin xxx");
    mProcessComboBox->setDisabled(true);
    mThreadComboBox->setDisabled(true);
    mFilePathButton->setDisabled(true);
    DEG_LOG("set ui disable end xxx");
    

    // process log
    // 耗时任务放到子线程，避免UI线程卡死
    mpProcessThread->initResources(mFileDescriptor, mFilePath, mThreadNum);
    mpProcessThread->start();

    mpBarThread->initResources(mFileDescriptor, mProcessLine);
    mpBarThread->start();

}
