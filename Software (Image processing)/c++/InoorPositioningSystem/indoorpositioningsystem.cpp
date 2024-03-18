#include "indoorpositioningsystem.h"
#include "./ui_indoorpositioningsystem.h"

IndoorPositioningSystem::IndoorPositioningSystem(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::IndoorPositioningSystem)
    , frameQueue(100)

{
    ui->setupUi(this);

    // dataProcessor = new DataProcessor(frameQueue);
    // dataProcessor = std::make_unique<DataProcessor>(frameQueue);


    // videoProcessor = new VideoProcessor(frameQueue, dataProcessor);



    frameTimer = new QTimer(this);
    // frameTimer = std::make_unique<QTimer>(this);
    frameTimer->setInterval(58); // 3500 for human detector

    dataProcessor = std::make_unique<DataProcessor>(frameQueue);
    videoProcessor = std::make_unique<VideoProcessor>(frameQueue, dataProcessor.get());


    connect(frameTimer, &QTimer::timeout, this, &IndoorPositioningSystem::checkForDisplay);
    connect(this, &IndoorPositioningSystem::frameIsReady, this, &IndoorPositioningSystem::updateDataDisplay);
    connect(this, &IndoorPositioningSystem::requestProcessVideo, videoProcessor.get(), &VideoProcessor::processVideo);

    // connect(this, &IndoorPositioningSystem::requestStopProcessing, videoProcessor.get(),  &VideoProcessor::stopProcessing, Qt::DirectConnection);
    // connect(this, &IndoorPositioningSystem::requestContinueProcessing, videoProcessor,  &VideoProcessor::continueProcessing, Qt::BlockingQueuedConnection);
    // connect(this, &IndoorPositioningSystem::requestSeekToFrame, videoProcessor.get(),  &VideoProcessor::seekToFrame, Qt::DirectConnection);
    // connect(videoProcessor.get(), &VideoProcessor::processingIsStopped, this, &IndoorPositioningSystem::seekToFrame);
    // connect(this, &IndoorPositioningSystem::requestVideoInit, videoProcessor.get(), &VideoProcessor::init, Qt::BlockingQueuedConnection);
    // connect(this, &IndoorPositioningSystem::requestLoadData, dataProcessor.get(), &DataProcessor::loadData, Qt::BlockingQueuedConnection);

    connect(videoProcessor.get(), &VideoProcessor::seekingDone, this, &IndoorPositioningSystem::afterSeeking);

    // connect(this, &IndoorPositioningSystem::requestDataExport, videoProcessor.get(), &VideoProcessor::dataExport, Qt::BlockingQueuedConnection);//, Qt::DirectConnection);

    // connect(this, &IndoorPositioningSystem::requestAnalyseData, dataProcessor.get(), &DataProcessor::dataAnalysisInit, Qt::DirectConnection);

    // connect(this, &IndoorPositioningSystem::finishedVideoProcessing, &videoThread, &QThread::quit);
    // connect(this, &IndoorPositioningSystem::finishedVideoProcessing, videoProcessor.get(), &QObject::deleteLater);

    // connect(this, &QMainWindow::destroyed, &videoThread, &QThread::quit);
    // connect(this, &QMainWindow::destroyed, videoProcessor.get(), &QObject::deleteLater);
    // connect(this, &QMainWindow::destroyed, &uwbDataThread, &QThread::quit);
    // connect(this, &QMainWindow::destroyed, dataProcessor.get(), &QObject::deleteLater);

    // connect(dataAnalysisWindow.get(), &QDialog::finished, &dataAnalysisWindowThread, &QThread::quit);
    // connect(uwbLocalizationWindow.get(), &QDialog::finished, uwbLocalizationWindow.get(), &QDialog::deleteLater);

    // connect(videoProcessor, &VideoProcessor::finished, this, &IndoorPositioningSystem::stopCheckingForDisplayThread);
    // connect(&videoThread, &QThread::finished, &videoThread, &QThread::deleteLater);

    ui->pushButton_Play_Pause->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    // dataProcessor->moveToThread(&uwbDataThread);
    // videoProcessor->moveToThread(&videoThread);
    // ui->timeEdit_Data_Analysis_Start->setDisplayFormat("HH:mm:ss");
    // ui->timeEdit_Data_Analysis_End->setDisplayFormat("HH:mm:ss");

    // // ClickEventFilter *filter = new ClickEventFilter(this);
    // ui->timeEdit_Data_Analysis_Start->installEventFilter(this);
}

IndoorPositioningSystem::~IndoorPositioningSystem()
{
    videoProcessor->stopProcessing();

    // videoThread.terminate();
    // videoThread.wait();
    // uwbDataThread.terminate();
    // uwbDataThread.wait();
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
            lastPosition = data.videoData.id;
        }
    }

}

void IndoorPositioningSystem::on_pushButton_Play_Pause_clicked()
{
    if (frameTimer->isActive()) {
        isPause = true;
        frameTimer->stop();
        ui->pushButton_Play_Pause->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    } else {
        isPause = false;
        frameTimer->start();
        ui->pushButton_Play_Pause->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
    }
}

void IndoorPositioningSystem::checkForDisplay() {
    UWBVideoData data;

    // qDebug() << "Checking for frame to be displayed...";

    if (!isPause && !isExportState) {
        if (frameQueue.dequeue(data)) {
            emit frameIsReady(data);
        }
    }
}

void IndoorPositioningSystem::on_actionOpen_Video_triggered()
{
    // QString filename = QFileDialog::getOpenFileName(this, tr("Open Video"), "", tr("Video Files (*.avi)"));
    std::string videoFileName, videotTimestampsFileName, UWBDataFileName;

    QString directory = QFileDialog::getExistingDirectory(this, tr("Open Directory"), "", (QFileDialog::ShowDirsOnly, QFileDialog::DontResolveSymlinks));

    if (!directory.isEmpty()) {
        QDir qDirecroty(directory);

        videoFileName = qDirecroty.filePath("video.avi").toStdString();
        UWBDataFileName = qDirecroty.filePath("UWB_timestamps.txt").toStdString();/*qDirecroty.entryList(QStringList() << "UWB_*.txt", QDir::Files);*/
        videotTimestampsFileName = qDirecroty.filePath("video_timestamps.txt").toStdString();/*qDirecroty.entryList(QStringList() << "video_*.txt", QDir::Files);*/
    } /*else {
        throw QException('Directory is empty!');
    }*/

    dataProcessor->loadData(UWBDataFileName, videotTimestampsFileName);
    videoProcessor->init(videoFileName);

    // emit requestVideoInit(videoFileName);

    // emit requestLoadData(UWBDataFileName, videotTimestampsFileName);

    videoDuration = videoProcessor->getVideoDuration();
    fps = videoProcessor->getFPS();
    totalFrames = videoProcessor->getTotalFrames();
    // if (totalFrames == -1) throw std::runtime_error("Error opening video file");

    isPause = false;
    isExportState = false;
    // toShowUWBLocalization = false;
    emit requestProcessVideo();
    frameTimer->start();
    // videoThread.start();

    // emit requestProcessVideo();

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

// void IndoorPositioningSystem::seekToFrame() {

//     // frameQueue.notify_all();
//     // emit requestSeekToFrame(seekPosition);
//     videoProcessor->seekToFrame(seekPosition);

// }

void IndoorPositioningSystem::afterSeeking() {

    if (!isPause) {
        frameTimer->start();
    }
    // frameQueue.notify_all();
    // emit requestProcessVideo();
}



void IndoorPositioningSystem::on_horizontalSlider_Duration_sliderReleased()
{
    // emit requestStopProcessing();
    videoProcessor->pauseProcessing();
    frameQueue.clear();
    videoProcessor->seekToFrame(seekPosition);
    videoProcessor->resumeProcessing();
}


void IndoorPositioningSystem::on_pushButton_UWB_Data_Analysis_clicked()
{
    if (!dataAnalysisWindow) {
        dataAnalysisWindow = std::make_unique<DataAnalysisWindow>(this, dataProcessor.get(), fps);
    }

    dataAnalysisWindow->show();
}




void IndoorPositioningSystem::on_pushButton_UWB_Localization_clicked()
{
    if (!uwbLocalizationWindow) {
        std::vector<QPointF> anchorPositions = {QPointF(1, 0), QPointF(3.5, 0)};

        uwbLocalizationWindow = std::make_unique<UWBLocalizationWindow>(this, anchorPositions);
        connect(this, &IndoorPositioningSystem::tagPositionUpdated, uwbLocalizationWindow.get(), &UWBLocalizationWindow::updateTagPosition);
    }

    uwbLocalizationWindow->show();
}

// void IndoorPositioningSystem::onUWBLocalizationWindowClosed() {
//     uwbLocalizationWindow = nullptr;

//     disconnect(this, &IndoorPositioningSystem::tagPositionUpdated, uwbLocalizationWindow.get(), &UWBLocalizationWindow::updateTagPosition);
// }


void IndoorPositioningSystem::on_pushButton_Export_Data_clicked()
{

    if (!exportTimeRangeSetter) {
        exportTimeRangeSetter = std::make_unique<ExportTimeRangeSetter>(this);

        connect(exportTimeRangeSetter.get(), &QDialog::accepted, this, &IndoorPositioningSystem::onAcceptDataExport);
    }

    exportTimeRangeSetter->startTimeEdit->setTime(QTime(0, 0));
    exportTimeRangeSetter->endTimeEdit->setTime(QTime(0, 0));
    exportTimeRangeSetter->show();

    // connect(exportTimeRangeSetter.get(), &QDialog::accepted, this, [this, exportTimeRangeSetter, ui]() {
    //     // frameTimer->stop();

    // });
}

void IndoorPositioningSystem::onAcceptDataExport() {
    QTime startTime = exportTimeRangeSetter->startTimeEdit->time();
    QTime endTime = exportTimeRangeSetter->endTimeEdit->time();
    seekPosition = (startTime.hour() * 3600 + startTime.minute() * 60 + startTime.second()) * fps;
    endDataExportPosition = (endTime.hour() * 3600 + endTime.minute() * 60 + endTime.second()) * fps;
    seekPosition = ((seekPosition - 1) < 0) ? 0 : seekPosition - 1;
    endDataExportPosition = ((endDataExportPosition - 1) < 0) ? 0 : endDataExportPosition - 1;
    int totalExportDuration = endDataExportPosition - seekPosition;

    if (totalExportDuration > 0) {
        ui->horizontalSlider_Duration->setEnabled(false);
        ui->pushButton_Export_Data->setEnabled(false);
        ui->pushButton_UWB_Data_Analysis->setEnabled(false);
        ui->pushButton_UWB_Localization->setEnabled(false);
        ui->pushButton_Play_Pause->setEnabled(false);
        isPause = true;
        if (frameTimer->isActive()) {
            frameTimer->stop();
        }
        isExportState = true;


        if (!exportProgressDialog) {
            exportProgressDialog = new QProgressDialog("Exporting data...", "Cancel", 0, 100, this); // 0..100%
            exportProgressDialog->setWindowModality(Qt::WindowModal);

            connect(videoProcessor.get(), &VideoProcessor::exportFinished, this, &IndoorPositioningSystem::onExportFinish, Qt::BlockingQueuedConnection);
            connect(dataProcessor.get(), &DataProcessor::exportProgressUpdated, this, [this, totalExportDuration](int position) {
                    position = ((position - seekPosition) * 100) / totalExportDuration;
                    exportProgressDialog->setValue(position);
                }, Qt::QueuedConnection);
            connect(exportProgressDialog, &QProgressDialog::canceled, this, [this]() {
                // exportProgressDialog->setValue(0);
                // exportProgressDialog->hide();
                videoProcessor->stopExport();

            });
        }


        videoProcessor->pauseProcessing();
        frameQueue.interruptionRequest();
        videoProcessor->dataExport(endDataExportPosition);
        videoProcessor->seekToFrame(seekPosition);
        videoProcessor->resumeProcessing();
        exportProgressDialog->show();
    } else {
        QMessageBox::warning(this, "Warning! Wrong time range.", "Time range is set wrong. End time should be larger than start time.");
        exportTimeRangeSetter->startTimeEdit->setTime(QTime(0, 0));
        exportTimeRangeSetter->endTimeEdit->setTime(QTime(0, 0));
        exportTimeRangeSetter->show();
    }
}

void IndoorPositioningSystem::onExportFinish(bool success) {
    if (success) {
        exportProgressDialog->setValue(exportProgressDialog->maximum());
        QMessageBox::information(this, "Export Complete", "The data export operation has completed successfully.");
    } else {
        // exportProgressDialog->setValue(0);
        exportProgressDialog->reset();
        // exportProgressDialog->close();
    }

    ui->horizontalSlider_Duration->setEnabled(true);
    ui->pushButton_Export_Data->setEnabled(true);
    ui->pushButton_UWB_Data_Analysis->setEnabled(true);
    ui->pushButton_UWB_Localization->setEnabled(true);
    ui->pushButton_Play_Pause->setEnabled(true);
    isPause = false;
    isExportState = false;

    videoProcessor->pauseProcessing();
    frameQueue.clear();
    videoProcessor->seekToFrame(lastPosition);
    videoProcessor->resumeProcessing();
}

