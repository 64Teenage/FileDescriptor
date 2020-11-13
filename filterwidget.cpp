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
, mFilePathButton(createLogButton())
, mProcessButton(createProcessButton())
, mProcessBar(createProcessBar())
, mpProcessHandler(new HandlerThread())
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
    connect(mpBarThread, static_cast<void (QBarThread::*)(double)>(&QBarThread::notify),
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
    threadComboBox->setFixedSize(50, 23);
    return threadComboBox;
}
QPushButton*    
FilterWidget::createProcessButton() const {
    QPushButton *processButton = new QPushButton();
    processButton->setText("process");
    DEG_LOG("Create ProcessButton: %p, Success", processButton);
    processButton->setFixedSize(80,23);
    return processButton;
}

QProgressBar*
FilterWidget::createProcessBar() const {
    QProgressBar *processBar = new QProgressBar();
    processBar->setMinimum(0);
    processBar->setMaximum(1000);
    processBar->setValue(0);
    processBar->setFormat(tr("Current progress : %1%").arg(QString::number(0, 'f', 1)));  
    processBar->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    
    processBar->setFixedWidth(490);
    return processBar;
}

QPushButton*
FilterWidget::createLogButton() const {
    QPushButton *logButton = new QPushButton();
    logButton->setFixedSize(140, 23);

    return logButton;
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
FilterWidget::processBarChanged(double val) {
    mProcessBar->setValue( val * 10);
    mProcessBar->setFormat(tr("Current progress : %1%").arg(QString::number(val, 'f', 1)));  
    mProcessBar->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
}

void
FilterWidget::processDescriptorChanged(ResultData data) {
    DEG_LOG("receive process end signal");

    mProcessComboBox->setDisabled(false);
    mThreadComboBox->setDisabled(false);
    mFilePathButton->setDisabled(false);

    for(auto it = data.begin(); it != data.end(); ++it) {
        std::cout<<"Bad File Descriptor: "<< it.key()<<std::endl;

        for( auto & element : it.value()) {
            auto node = element.get();
            FDSTATUS status = std::get<2>(node);
            std::cout<<"\t"<<std::get<0>(node)<<"\t";
            if(status == FDSTATUS::OPENING) {
                std::cout<<"Opening\t";
            } else if(status == FDSTATUS::DUMPING) {
                std::cout<<"Dumping\t";
            } else if(status == FDSTATUS::CLOSED) {
                std::cout<<"CLOSED\t";
            }
            std::cout<<std::get<1>(node)<<std::endl;
        }
    }

}

void
FilterWidget::processButtonClicked() {
    DEG_LOG("set ui disable begin xxx");
    mProcessComboBox->setDisabled(true);
    mThreadComboBox->setDisabled(true);
    mFilePathButton->setDisabled(true);
    DEG_LOG("set ui disable end xxx");

    DEG_LOG("Button width: %d, height: %d", mProcessButton->width(), mProcessButton->height());
    
    // 数据处理（耗时任务）放到子线程，避免UI线程卡死
    mFileDescriptor->initResources(2038, mFilePath.toStdString(), mThreadNum);
    mpProcessThread->initResources(mFileDescriptor);
    mpProcessThread->start();

    //获取数据处理进度，子线程
    mpBarThread->initResources(mFileDescriptor, mProcessLine);
    mpBarThread->start();

}
