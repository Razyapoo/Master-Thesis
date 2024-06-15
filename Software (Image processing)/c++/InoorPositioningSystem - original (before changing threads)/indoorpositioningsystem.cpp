#include "indoorpositioningsystem.h"
#include "./ui_indoorpositioningsystem.h"

IndoorPositioningSystem::IndoorPositioningSystem(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::IndoorPositioningSystem)
    , frameQueue(10)

{
    ui->setupUi(this);

    // dataProcessor = new DataProcessor(frameQueue);
    dataProcessor = std::make_unique<DataProcessor>(frameQueue);
    dataProcessor->moveToThread(&uwbDataThread);

    // videoProcessor = new VideoProcessor(frameQueue, dataProcessor);
    videoProcessor = std::make_unique<VideoProcessor>(frameQueue, dataProcessor.get());
    videoProcessor->moveToThread(&videoThread);

    frameTimer = new QTimer(this);
    // frameTimer = std::make_unique<QTimer>(this);
    frameTimer->setInterval(58); // 3500 for human detector

    // checkForDisplayThread = std::thread(&IndoorPositioningSystem::checkForDisplay, this);
    // connect(&videoThread, &QThread::started, this, &IndoorPositioningSystem::startProcessVideo);
    connect(this, &IndoorPositioningSystem::requestProcessVideo, videoProcessor.get(), &VideoProcessor::processVideo);
    connect(this, &IndoorPositioningSystem::requestLoadData, dataProcessor.get(), &DataProcessor::loadData, Qt::DirectConnection);
    // connect(videoProcessor, &VideoProcessor::latestFrame, this, &IndoorPositioningSystem::updateDataDisplay);

    connect(frameTimer, &QTimer::timeout, this, &IndoorPositioningSystem::checkForDisplay);
    connect(this, &IndoorPositioningSystem::frameIsReady, this, &IndoorPositioningSystem::updateDataDisplay);
    connect(videoProcessor.get(), &VideoProcessor::requestFindUWBMeasurementAndEnqueue, dataProcessor.get(), &DataProcessor::onFindUWBMeasurementAndEnqueue, Qt::DirectConnection);

    connect(this, &IndoorPositioningSystem::requestStopProcessing, videoProcessor.get(),  &VideoProcessor::stopProcessing, Qt::DirectConnection);
    // connect(this, &IndoorPositioningSystem::requestContinueProcessing, videoProcessor,  &VideoProcessor::continueProcessing, Qt::BlockingQueuedConnection);
    connect(this, &IndoorPositioningSystem::requestSeekToFrame, videoProcessor.get(),  &VideoProcessor::seekToFrame, Qt::DirectConnection);
    connect(videoProcessor.get(), &VideoProcessor::processingIsStopped, this, &IndoorPositioningSystem::seekToFrame);
    connect(videoProcessor.get(), &VideoProcessor::seekingDone, this, &IndoorPositioningSystem::afterSeeking, Qt::DirectConnection);

    connect(this, &IndoorPositioningSystem::requestDataExport, videoProcessor.get(), &VideoProcessor::onDataExport, Qt::DirectConnection);
    connect(videoProcessor.get(), &VideoProcessor::requestFindUWBMeasurementAndExport, dataProcessor.get(), &DataProcessor::onFindUWBMeasurementAndExport, Qt::DirectConnection);
    connect(videoProcessor.get(), &VideoProcessor::exportFinished, this, &IndoorPositioningSystem::onExportFinish, Qt::DirectConnection);




    // connect(this, &IndoorPositioningSystem::requestAnalyseData, dataProcessor.get(), &DataProcessor::dataAnalysisInit, Qt::DirectConnection);

    connect(this, &IndoorPositioningSystem::finishedVideoProcessing, &videoThread, &QThread::quit);
    connect(this, &IndoorPositioningSystem::finishedVideoProcessing, videoProcessor.get(), &QObject::deleteLater);

    // connect(this, &QMainWindow::destroyed, &videoThread, &QThread::quit);
    // connect(this, &QMainWindow::destroyed, videoProcessor.get(), &QObject::deleteLater);
    // connect(this, &QMainWindow::destroyed, &uwbDataThread, &QThread::quit);
    // connect(this, &QMainWindow::destroyed, dataProcessor.get(), &QObject::deleteLater);

    // connect(dataAnalysisWindow.get(), &QDialog::finished, &dataAnalysisWindowThread, &QThread::quit);
    // connect(uwbLocalizationWindow.get(), &QDialog::finished, uwbLocalizationWindow.get(), &QDialog::deleteLater);

    // connect(videoProcessor, &VideoProcessor::finished, this, &IndoorPositioningSystem::stopCheckingForDisplayThread);
    // connect(&videoThread, &QThread::finished, &videoThread, &QThread::deleteLater);

    ui->pushButton_Play_Pause->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    // ui->timeEdit_Data_Analysis_Start->setDisplayFormat("HH:mm:ss");
    // ui->timeEdit_Data_Analysis_End->setDisplayFormat("HH:mm:ss");

    // // ClickEventFilter *filter = new ClickEventFilter(this);
    // ui->timeEdit_Data_Analysis_Start->installEventFilter(this);
}

IndoorPositioningSystem::~IndoorPositioningSystem()
{
    frameQueue.notify_all();
    frameQueue.clear();

    videoThread.terminate();
    videoThread.wait();
    uwbDataThread.terminate();
    uwbDataThread.wait();
    // dataAnalysisWindowThread.terminate();
    // dataAnalysisWindowThread.wait();
    delete ui;
}


void IndoorPositioningSystem::updateDataDisplay(const UWBVideoData& data) {

    if (data.videoData.qImage.isNull() && data.videoData.id == -1) {
        frameTimer->stop();
        // stopCheckingForDisplayThread();
        std::cout << "No frames left" << std::endl;
        emit finishedVideoProcessing();
    } else {
        qPixmap = QPixmap::fromImage(data.videoData.qImage);
        qPixmap = qPixmap.scaled(ui->label_Video->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);

        ui->label_Video->setPixmap(qPixmap);

        ui->label_Video_Frame_ID_value->setNum(data.videoData.id);
        QString videoTimestampText = QString::number(data.videoData.timestamp);
        ui->label_Video_Frame_Timestamp_value->setText(videoTimestampText);

        // QLayout* uwbDataContainerLayout = ui->uwbDataContainer->layout();

        // if (uwbDataContainerLayout != NULL) {
        //     QLayoutItem* item;
        //     item = uwbDataContainerLayout->takeAt(0);
        //     while (item != NULL) {
        //         if (QWidget* widget = item->widget()) {
        //             widget->hide();
        //             delete widget;
        //         }
        //         delete item;
        //         item = uwbDataContainerLayout->takeAt(0);
        //     }
        // }


        // // for (const auto& tagData : data.uwbData) {
        // QLabel* tagLabel = new QLabel(this);
        // tagLabel->setText(QString("ID: %1  Timestamp: %2").arg(data.uwbData.tagID).arg(data.uwbData.timestamp));
        // // QLabel* tagLabel = new QLabel(tr("ID: %1  Timestamp: %2").arg(data.uwbData.tagID).arg(data.uwbData.timestamp), this);
        // uwbDataContainerLayout->addWidget(tagLabel);

        // for (const auto& anchor : data.uwbData.anchorList) {
        //     QLabel* anchorLabel = new QLabel(this);
        //     anchorLabel->setText(QString("Anchor %1: %2").arg(anchor.anchorID).arg(anchor.distance));
        //     uwbDataContainerLayout->addWidget(anchorLabel);
        // }
        // // }


        // ui->uwbDataContainer->setLayout(uwbDataContainerLayout);

        for (const UWBData& tag: data.uwbData)
        {
            QString tagTimestampText = QString::number(tag.timestamp);
            QString anchor101DistanceText = QString::number(tag.anchorList[0].distance, 'f', 6);
            QString anchor102DistanceText = QString::number(tag.anchorList[1].distance, 'f', 6);

            switch (tag.tagID)
            {
            case 1:
                ui->label_Tag_ID_value_1->setNum(tag.tagID);
                ui->label_Tag_timestamp_value_1->setText(tagTimestampText);
                ui->label_Anchor101_value_1->setText(anchor101DistanceText);
                ui->label_Anchor102_value_1->setText(anchor102DistanceText);
                break;
            case 2:
                ui->label_Tag_ID_value_2->setNum(tag.tagID);
                ui->label_Tag_timestamp_value_2->setText(tagTimestampText);
                ui->label_Anchor101_value_2->setText(anchor101DistanceText);
                ui->label_Anchor102_value_2->setText(anchor102DistanceText);
                break;
            case 3:
                ui->label_Tag_ID_value_3->setNum(tag.tagID);
                ui->label_Tag_timestamp_value_3->setText(tagTimestampText);
                ui->label_Anchor101_value_3->setText(anchor101DistanceText);
                ui->label_Anchor102_value_3->setText(anchor102DistanceText);
                break;
            }

            if ((uwbLocalizationWindow != nullptr) && (!uwbLocalizationWindow->isHidden()) ) {
                emit tagPositionUpdated(tag.coordinates, tag.tagID);
            }

        }


        if (!ui->horizontalSlider_Duration->isSliderDown()){
            double currentTimeInSeconds = data.videoData.id / fps;
            QTime currentTime = QTime(0, 0).addSecs(static_cast<int>(currentTimeInSeconds));
            ui->horizontalSlider_Duration->setValue(data.videoData.id);
            ui->label_Current_Time->setText(currentTime.toString("HH:mm:ss"));
        }
    }

}

void IndoorPositioningSystem::on_pushButton_Play_Pause_clicked()
{
    if (frameTimer->isActive()) {
        isPlayPauseSetToPlay = false;
        frameTimer->stop();
        ui->pushButton_Play_Pause->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    } else {
        isPlayPauseSetToPlay = true;
        frameTimer->start();
        ui->pushButton_Play_Pause->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
    }
}

void IndoorPositioningSystem::checkForDisplay() {
    UWBVideoData data;

    // qDebug() << "Checking for frame to be displayed...";

    if (videoProcessor->isRunning()) {
        if (frameQueue.dequeue(data)) {
            emit frameIsReady(data);
        }
    }
}

void IndoorPositioningSystem::on_actionOpen_Video_triggered()
{
    // QString filename = QFileDialog::getOpenFileName(this, tr("Open Video"), "", tr("Video Files (*.avi)"));
    QString videoFileName, videotTimestampsFileName, UWBDataFileName;

    QString directory = QFileDialog::getExistingDirectory(this, tr("Open Directory"), "", (QFileDialog::ShowDirsOnly, QFileDialog::DontResolveSymlinks));

    if (!directory.isEmpty()) {
        QDir qDirecroty(directory);

        videoFileName = qDirecroty.filePath("video.avi");
        UWBDataFileName = qDirecroty.filePath("UWB_timestamps.txt");/*qDirecroty.entryList(QStringList() << "UWB_*.txt", QDir::Files);*/
        videotTimestampsFileName = qDirecroty.filePath("video_timestamps.txt");/*qDirecroty.entryList(QStringList() << "video_*.txt", QDir::Files);*/
    } /*else {
        throw QException('Directory is empty!');
    }*/

    videoProcessor->init(videoFileName);
    emit requestLoadData(UWBDataFileName, videotTimestampsFileName);

    videoDuration = videoProcessor->getVideoDuration();
    fps = videoProcessor->getFPS();
    totalFrames = videoProcessor->getTotalFrames();
    // if (totalFrames == -1) throw std::runtime_error("Error opening video file");

    isPlayPauseSetToPlay = true;
    isExportState = false;
    // toShowUWBLocalization = false;
    frameTimer->start();
    videoThread.start();

    emit requestProcessVideo();

    ui->horizontalSlider_Duration->setRange(1, totalFrames);
    QTime totalTime = QTime(0, 0).addSecs(static_cast<int>(videoDuration));
    ui->label_Total_Time->setText(totalTime.toString("HH:mm:ss"));
    ui->pushButton_Play_Pause->setIcon(style()->standardIcon(QStyle::SP_MediaPause));

}

void IndoorPositioningSystem::on_horizontalSlider_Duration_valueChanged(int position) {
    if (ui->horizontalSlider_Duration->isSliderDown()) {
        if (frameTimer->isActive()) {
            frameTimer->stop();
        }
        // std::cout << "Position: " << position << std::endl;
        double setTimeInSeconds = position / fps;
        QTime setTime = QTime(0, 0).addSecs(static_cast<int>(setTimeInSeconds));
        ui->label_Current_Time->setText(setTime.toString("HH:mm:ss"));
        seekPosition = position;
    }
}

void IndoorPositioningSystem::seekToFrame() {

    // frameQueue.notify_all();
    emit requestSeekToFrame(seekPosition);
}

void IndoorPositioningSystem::afterSeeking() {


    if (isExportState) {
        emit requestDataExport(endDataExportPosition);
    } else if (isPlayPauseSetToPlay) {
        frameTimer->start();
    }
    // frameQueue.notify_all();
    emit requestProcessVideo();
}



void IndoorPositioningSystem::on_horizontalSlider_Duration_sliderReleased()
{
    emit requestStopProcessing();
    frameQueue.clear();
    frameQueue.notify_all();
}


void IndoorPositioningSystem::on_pushButton_UWB_Data_Analysis_clicked()
{
    dataAnalysisWindow = std::make_unique<DataAnalysisWindow>(this, dataProcessor.get(), fps);
    // connect(dataAnalysisWindow.get(), &QDialog::finished, dataAnalysisWindow.get(), &DataAnalysisWindow::onDialogClosed);
    // dataAnalysisWindow->moveToThread(&(dataAnalysisWindow->dataAnalysisWindowThread));
    // dataAnalysisWindow->dataAnalysisWindowThread.start();

    dataAnalysisWindow->show();
    // dataAnalysisWindow->moveToThread(&dataAnalysisWindowThread);
    // dataAnalysisWindowThread.start();

    // QTime startTime = ui->timeEdit_Data_Analysis_Start->time();
    // QTime endTime = ui->timeEdit_Data_Analysis_End->time();
    // long long startFrameIndex = (startTime.hour()*3600 + startTime.minute()*60 + startTime.second()) * fps;
    // long long endFrameIndex = (endTime.hour()*3600 + endTime.minute()*60 + endTime.second()) * fps;
    // startFrameIndex = ((startFrameIndex - 1) < 0) ? 0 : startFrameIndex - 1;
    // endFrameIndex = endFrameIndex - 1;
    // emit requestAnalyseData(startFrameIndex, endFrameIndex);
}




void IndoorPositioningSystem::on_pushButton_UWB_Localization_clicked()
{
    std::vector<QPointF> anchorPositions = {QPointF(1, 0), QPointF(3.5, 0)};

    uwbLocalizationWindow = std::make_unique<UWBLocalizationWindow>(this, anchorPositions);
    uwbLocalizationWindow->show();

    connect(this, &IndoorPositioningSystem::tagPositionUpdated, uwbLocalizationWindow.get(), &UWBLocalizationWindow::updateTagPosition);

}

void IndoorPositioningSystem::onUWBLocalizationWindowClosed() {
    uwbLocalizationWindow = nullptr;

    disconnect(this, &IndoorPositioningSystem::tagPositionUpdated, uwbLocalizationWindow.get(), &UWBLocalizationWindow::updateTagPosition);
}


void IndoorPositioningSystem::on_pushButton_Export_Data_clicked()
{
    exportTimeRangeSetter = std::make_unique<ExportTimeRangeSetter>(this);

    connect(exportTimeRangeSetter.get(), &QDialog::accepted, this, &IndoorPositioningSystem::onAcceptDataExport);
    exportTimeRangeSetter->show();

    // connect(exportTimeRangeSetter.get(), &QDialog::accepted, this, [this, exportTimeRangeSetter, ui]() {
    //     // frameTimer->stop();

    // });
}

void IndoorPositioningSystem::onAcceptDataExport() {
    ui->horizontalSlider_Duration->setEnabled(false);
    ui->pushButton_Export_Data->setEnabled(false);
    ui->pushButton_UWB_Data_Analysis->setEnabled(false);
    ui->pushButton_UWB_Localization->setEnabled(false);
    ui->pushButton_Play_Pause->setEnabled(false);
    if (frameTimer->isActive()) {
        frameTimer->stop();
    }
    lastPosition = ui->horizontalSlider_Duration->pos().x();

    isExportState = true;
    QTime startTime = exportTimeRangeSetter->startTimeEdit->time();
    QTime endTime = exportTimeRangeSetter->endTimeEdit->time();
    seekPosition = (startTime.hour() * 3600 + startTime.minute() * 60 + startTime.second()) * fps;
    endDataExportPosition = (endTime.hour() * 3600 + endTime.minute() * 60 + endTime.second()) * fps;
    seekPosition = ((seekPosition - 1) < 0) ? 0 : seekPosition - 1;
    endDataExportPosition = ((endDataExportPosition - 1) < 0) ? 0 : endDataExportPosition - 1;

    emit requestStopProcessing();
}

void IndoorPositioningSystem::onExportFinish() {
    ui->horizontalSlider_Duration->setEnabled(true);
    ui->pushButton_Export_Data->setEnabled(true);
    ui->pushButton_UWB_Data_Analysis->setEnabled(true);
    ui->pushButton_UWB_Localization->setEnabled(true);
    ui->pushButton_Play_Pause->setEnabled(true);
    isExportState = false;

    emit requestSeekToFrame(lastPosition);
}
