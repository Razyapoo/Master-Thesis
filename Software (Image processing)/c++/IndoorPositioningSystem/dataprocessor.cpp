#include "dataprocessor.h"

DataProcessor::DataProcessor(ThreadSafeQueue& frameQueue): frameQueue(frameQueue) {

    dataProcessorThread.reset(new QThread);
    moveToThread(dataProcessorThread.get());

    dataProcessorThread->start();

    segmentFrameIDs = {};
    segmentRepresentatives = {};

}

DataProcessor::~DataProcessor() {
    // cleanup();
    QMetaObject::invokeMethod(this, "cleanup");
    dataProcessorThread->wait();
}

void DataProcessor::cleanup() {
    if (uwbDataFile.is_open()){
        uwbDataFile.close();
    }

    if (videoDataFile.is_open()) {
        videoDataFile.close();
    }
    XGBoosterFree(booster);

    dataProcessorThread->quit();
}

void DataProcessor::loadData(const std::string& folderName, const std::string& UWBDataFilename, const std::string& videoDataFilename) {

    projectFolderName = folderName;
    videoDataFile = std::ifstream(videoDataFilename);
    videoTimestampsVector.clear();

    int id;
    long long timestamp;
    while (videoDataFile >> id >> timestamp)
    {
        // assuming data are recorded sequentially and no intermidiate data is missing
        videoTimestampsVector.push_back(timestamp);
    }

    uwbDataFile = std::ifstream(UWBDataFilename);
    UWBData record;
    std::string line;
    Anchor anchor;
    uwbDataVector.clear();
    uwbDataPerTag.clear();
    coordinateHistory.clear();
    while (std::getline(uwbDataFile, line, '\n'))
    {
        std::istringstream ss(line);

        ss >> record.id >> record.timestamp >> record.tagID; // guaranteed to be present


        int i = 0;
        record.anchorList.clear();
        for (int i = 0; i < 2; i++) // guaranteed to be 2. It is possible to use while when more anchors found. This exact number is need for time measurement variable
        {
            ss >> anchor.anchorID >> anchor.distance;
            record.anchorList.push_back(anchor);
        }

        double temp;
        if (ss.rdbuf()->in_avail()) {
            ss >> temp;
            record.measurementTime = temp;
        } else {
            record.measurementTime = std::nullopt;
        }

        uwbDataVector.push_back(record);

    }

    for (UWBData& data: uwbDataVector) {
        uwbDataPerTag[data.tagID].push_back(&data); //Add Pointer to uwbDataVector
    }

    // qDebug() << "Data are loaded";

    fileIncrementer = 1;
    videoDataFile.close();
    uwbDataFile.close();
}

long long DataProcessor::getVideoTimestampById(int id) {
    return videoTimestampsVector[id];
}

int DataProcessor::getTotalFrames() {
    return videoTimestampsVector.size();
}

void DataProcessor::onFindUWBMeasurementAndEnqueue(int frameIndex, QImage qImage, DetectionData detectionData) {

    long long frameTimestamp = videoTimestampsVector[frameIndex - 1];

    // qDebug() << "Searching for UWB data...";

    VideoData videoData(frameIndex, std::move(qImage), frameTimestamp);
    // qDebug() << "Frame id: " << frameIndex;

    std::vector<UWBData> closestForEachTag;
    for (auto& data: uwbDataPerTag) {
        UWBData closestUWB = binarySearchUWB(frameTimestamp, data.second);
        // Calculating coordinates here and not when data are read, because data can be updated in the future
        calculateUWBCoordinates(closestUWB);
        closestForEachTag.push_back(closestUWB);
    }

    QPointF pixelToRealCoordinates(0.0, 0.0);
    QPointF opticalCoordinates(0.0, 0.0);
    std::vector<QPointF> pixelToRealCoordinatesVector;
    std::vector<QPointF> opticalCoordinatesVector;


    if (detectionData.detectionResults.size()) {
        for (const DetectionResult& detection: detectionData.detectionResults){
            if (isPredictionByPixelToRealRequested)
            {
                pixelToRealCoordinates = predictWorldCoordinatesPixelToReal(detection);
                pixelToRealCoordinatesVector.push_back(pixelToRealCoordinates);
            }

            if (isPredictionByOpticalRequested) {
                opticalCoordinates = predictWorldCoordinatesOptical(detection, detectionData.cameraFrameSize, detectionData.detectionFrameSize);
                opticalCoordinatesVector.push_back(opticalCoordinates);
            }
        }
    }


    // it is better to make a copy of UWB Data and then move it to the queue rather than push a pointer to existing array, just in case UWBData array will be deleted.
    UWBVideoData uwbVideoData(std::move(videoData), std::move(closestForEachTag), std::move(pixelToRealCoordinatesVector), std::move(opticalCoordinatesVector));

    frameQueue.enqueue(std::move(uwbVideoData));

}

void DataProcessor::onFindUWBMeasurementAndExport(int frameIndex, int rangeIndex, ExportType exportType, const DetectionData& detectionData, bool lastRecord) {

    long long frameTimestamp = videoTimestampsVector[frameIndex - 1];
    std::string exportDir = projectFolderName + "/export/";
    std::string outputFilePathUWB = exportDir + "uwb_to_bb_mapping_" + std::to_string(fileIncrementer) + ".txt";
    std::string outputFilePathPixelToReal = exportDir + "pixel_to_real_to_bb_mapping_" + std::to_string(fileIncrementer) + ".txt";
    std::string outputFilePathOptical = exportDir + "optical_to_bb_mapping_" + std::to_string(fileIncrementer) + ".txt";

    // Check if the directory exists, and create it if it doesn't
    if (!std::filesystem::exists(exportDir)) {
        std::filesystem::create_directories(exportDir);
    }

    if (!outputFileUWB.is_open()) {
        outputFileUWB = std::ofstream(outputFilePathUWB, std::ios::out);
    }

    if (!outputFilePixelToReal.is_open()) {
        outputFilePixelToReal = std::ofstream(outputFilePathPixelToReal, std::ios::out);
    }

    if (!outputFileOptical.is_open()) {
        outputFileOptical = std::ofstream(outputFilePathOptical, std::ios::out);
    }

    if (exportType == ExportType::FrameByFrameExport) {
        auto data = uwbDataPerTag.begin();
        // Vector of bottom edge centers is used for the future improvement of the code. When more people are used for model calibration
        // As for now only one person in the scene assumed
        for (int i = 0; i < detectionData.detectionResults.size(); ++i) {
            if (data != uwbDataPerTag.end()) {
                UWBData closestUWB = binarySearchUWB(frameTimestamp, data->second);
                calculateUWBCoordinates(closestUWB);
                outputFileUWB << frameIndex << " " << closestUWB.coordinates.x() << " " << closestUWB.coordinates.y() << " " << detectionData.detectionResults[i].bottomEdgeCenter.x() << " " << detectionData.detectionResults[i].bottomEdgeCenter.y() << std::endl;
                ++data;
            } else {
                // qDebug() << "Intruder is found!! There is no tag to match with people";
            }
        }
    // Always only for one tag for which data analysis is performed. Frame timestamp and UWBData are already known and synchornized. Synzhronization was done during split of dataset into segments
    // Assuming only one person in the scene. Otherwise need to solve synchronization between indexes of detected people and indexes of the tags they are wearing
    } else if (exportType == ExportType::SegmentFramesExport) {
        double segmentDistanceSum = 0.0;
        UWBData* data;
        for (Anchor& segmentRepresentativeAnchor: segmentRepresentatives[rangeIndex].anchorList) {
            for (int i = 0; i < segmentSizes[rangeIndex]; ++i) {
                int middleIdx = segmentSizes[rangeIndex] / 2;
                data = &uwbDataVector[segmentRepresentatives[rangeIndex].id - middleIdx + i]; // assuming id of record corresponds to index of vector -1 position. Seeking the first record of range
                std::vector<Anchor>::iterator anchor = std::find_if(data->anchorList.begin(), data->anchorList.end(), [segmentRepresentativeAnchor](const Anchor& anchor2) {
                    return anchor2.anchorID == segmentRepresentativeAnchor.anchorID;
                });
                if (anchor != data->anchorList.end()) {
                    segmentDistanceSum += anchor->distance;
                } else {
                    int j = 0;
                    j++;
                }
                // ++data;
            }
            double distanceMean = segmentDistanceSum / segmentSizes[rangeIndex];
            segmentRepresentativeAnchor.distance = distanceMean;
            segmentDistanceSum = 0.0;
        }

        calculateUWBCoordinates(segmentRepresentatives[rangeIndex]);

        // As for now without check whether everything needed for pixel-to-real and optical methods is loaded. Only hard export. Export only for analysis purpose. Otherwise should be commented.
        QPointF pixelToRealCoordinates(0.0, 0.0);
        QPointF opticalCoordinates(0.0, 0.0);

        outputFileUWB << frameIndex << " ";
        outputFilePixelToReal << frameIndex << " ";
        outputFileOptical << frameIndex << " ";

        for (int i = 0; i < detectionData.detectionResults.size(); ++i) {
            outputFileUWB << detectionData.detectionResults[i].bottomEdgeCenter.x() << " " << detectionData.detectionResults[i].bottomEdgeCenter.y() << " ";

            pixelToRealCoordinates = predictWorldCoordinatesPixelToReal(detectionData.detectionResults[i]);
            opticalCoordinates = predictWorldCoordinatesOptical(detectionData.detectionResults[i], detectionData.cameraFrameSize, detectionData.detectionFrameSize);

            outputFilePixelToReal << detectionData.detectionResults[i].bottomEdgeCenter.x() << " " << detectionData.detectionResults[i].bottomEdgeCenter.y() << " ";
            outputFileOptical << detectionData.detectionResults[i].bottomEdgeCenter.x() << " " << detectionData.detectionResults[i].bottomEdgeCenter.y() << " ";

            outputFilePixelToReal << pixelToRealCoordinates.x() << " " << pixelToRealCoordinates.y() << " ";
            outputFileOptical << opticalCoordinates.x() << " " << opticalCoordinates.y() << " ";
        }

        outputFileUWB << segmentRepresentatives[rangeIndex].coordinates.x() << " " << segmentRepresentatives[rangeIndex].coordinates.y() << std::endl;
        outputFilePixelToReal << std::endl;
        outputFileOptical << std::endl;


    }

    if (lastRecord && (outputFileUWB.is_open() || outputFilePixelToReal.is_open() || outputFileOptical.is_open())) {
        fileIncrementer++;
        segmentFrameIDs.clear();
        segmentSizes.clear();
        segmentRepresentatives.clear();
        if (outputFileUWB.is_open()) outputFileUWB.close();
        if (outputFilePixelToReal.is_open()) outputFilePixelToReal.close();
        if (outputFileOptical.is_open()) outputFileOptical.close();
    }

    emit exportProgressUpdated(rangeIndex);
}

void DataProcessor::setAnchorPositions(std::vector<AnchorPosition> positions) {
    anchorPositions = positions;
}

void DataProcessor::calculateUWBCoordinates(UWBData& tag) {

    // As for now, assuming only two anchors 101 and 102. Anchor 102 has coordinates (0, 0) in UWB coordinate system, Anchor 101 has coordinates (2.5, 0) in UWB corrdinate system
    // QPointF anchor101Coordinates(0, 0);
    // QPointF anchor102Coordinates(2.5, 0);
    // QPointF anchor103Coordinates(2.5, 15);
    // QPointF anchor104Coordinates(0, 15);

    // QPointF anchor101Coordinates(2.5, 0);
    // QPointF anchor102Coordinates(0, 0);
    // QPointF anchor103Coordinates(0, 15);
    // QPointF anchor104Coordinates(2.5, 15);

    // QMap<int, QPointF> anchorCoordinates;
    // // anchorCoordinates[101] = anchor101Coordinates;
    // // anchorCoordinates[102] = anchor102Coordinates;
    // // anchorCoordinates[103] = anchor103Coordinates;
    // // anchorCoordinates[104] = anchor104Coordinates;
    // for (const AnchorPosition& anchor: anchorPositions) {
    //     anchorCoordinates[anchor.anchorID] =
    // }

    double distanceAnchor1 = 0;
    double distanceAnchor2 = 0;
    int anchor1ID = 0, anchor2ID = 0;
    AnchorPosition anchor1Position, anchor2Position;

    auto found = std::find_if(anchorPositions.begin(), anchorPositions.end(), [](const AnchorPosition& pos) {
        return pos.isOrigin;
    });

    AnchorPosition origin = *found;


    // Extract distances from the anchor list and find the pair of anchors
    for (const Anchor& anchor: tag.anchorList) {
        // qDebug() << "AnchorID: " << anchor.anchorID;
        auto position = std::find_if(anchorPositions.begin(), anchorPositions.end(), [&anchor](const AnchorPosition& pos) {
            return pos.anchorID == anchor.anchorID;
        });

        if (position != anchorPositions.end()) {
            if (distanceAnchor1 == 0) {
                distanceAnchor1 = anchor.distance;
                anchor1Position = *position;
                // anchor1Position.x = anchor1Position.x - origin.x;
                // anchor1Position.y = anchor1Position.y - origin.y;
                anchor1ID = anchor.anchorID;
            } else if (distanceAnchor2 == 0) {
                distanceAnchor2 = anchor.distance;
                anchor2Position = *position;
                // anchor2Position.x = anchor2Position.x - origin.x;
                // anchor2Position.y = anchor2Position.y - origin.y;
                anchor2ID = anchor.anchorID;
                break; // We have found two distances, we can break out of the loop
            }
        }
    }

    double anchorBaseline = std::sqrt(std::pow(anchor1Position.x - anchor2Position.x, 2) +
                                      std::pow(anchor1Position.y - anchor2Position.y, 2));

    double x = 0, y = 0;

    // if (origin.anchorID == 101) { // S8
    //     if (anchor1ID == 101 || anchor1ID == 102) {
    //         x = (std::pow(distanceAnchor1, 2) - std::pow(distanceAnchor2, 2) + std::pow(anchorBaseline, 2)) / (2 * anchorBaseline);
    //         y = std::sqrt(std::pow(distanceAnchor1, 2) - std::pow(x, 2));
    //         //tag.coordinates.setY(std::abs(y + 2.08 - std::max(anchor1Position.y, anchor2Position.y)));
    //         tag.coordinates.setY(std::abs(y + 4.16 - std::max(anchor1Position.y, anchor2Position.y)));
    //     } else if (anchor1ID == 103 || anchor1ID == 104) {
    //         x = (std::pow(distanceAnchor2, 2) - std::pow(distanceAnchor1, 2) + std::pow(anchorBaseline, 2)) / (2 * anchorBaseline);
    //         y = std::sqrt(std::pow(distanceAnchor2, 2) - std::pow(x, 2));
    //         tag.coordinates.setY(std::abs(y - std::max(anchor1Position.y, anchor2Position.y)));
    //     }

    // } else if (origin.anchorID == 102) { // S306
    //     if (anchor1ID == 101 || anchor1ID == 102) {
    //         x = (std::pow(distanceAnchor2, 2) - std::pow(distanceAnchor1, 2) + std::pow(anchorBaseline, 2)) / (2 * anchorBaseline);
    //         y = std::sqrt(std::pow(distanceAnchor2, 2) - std::pow(x, 2));
    //         tag.coordinates.setY(std::abs(y + 4.16 - std::max(anchor1Position.y, anchor2Position.y)));
    //     } else if (anchor1ID == 103 || anchor1ID == 104) {
    //         x = (std::pow(distanceAnchor1, 2) - std::pow(distanceAnchor2, 2) + std::pow(anchorBaseline, 2)) / (2 * anchorBaseline);
    //         y = std::sqrt(std::pow(distanceAnchor1, 2) - std::pow(x, 2));
    //         tag.coordinates.setY(std::abs(y - std::max(anchor1Position.y, anchor2Position.y)));
    //     }
    // }

    double a = (std::pow(distanceAnchor1, 2) - std::pow(distanceAnchor2, 2) + std::pow(anchorBaseline, 2)) / (2 * anchorBaseline);
    double h = std::sqrt(std::pow(distanceAnchor1, 2) - std::pow(a, 2)); // could be negative due to descrepancies in measurements

    QPointF Ptemp, P1, P2, P;
    Ptemp.setX(anchor1Position.x + a * (anchor2Position.x - anchor1Position.x) / anchorBaseline);
    Ptemp.setY(anchor1Position.y + a * (anchor2Position.y - anchor1Position.y) / anchorBaseline);

    P1.setX(Ptemp.x() - h * (anchor2Position.y - anchor1Position.y) / anchorBaseline);
    P1.setY(Ptemp.y() + h * (anchor2Position.x - anchor1Position.x) / anchorBaseline);
    P2.setX(Ptemp.x() + h * (anchor2Position.y - anchor1Position.y) / anchorBaseline);
    P2.setY(Ptemp.y() - h * (anchor2Position.x - anchor1Position.x) / anchorBaseline);

    if ((anchor1Position.anchorID == 101 && anchor2Position.anchorID == 102) || (anchor1Position.anchorID == 102 && anchor2Position.anchorID == 101)) {
        // P1.setX(P.x() - h * (anchor2Position.y - anchor1Position.y) / anchorBaseline);
        // P1.setY(P.y() + h * (anchor2Position.x - anchor1Position.x) / anchorBaseline);
        if (P1.y() > P2.y()) {
            // tag.coordinates = QPointF(P1.x() + origin.x, P1.y() + origin.y);
            tag.coordinates = P1;
        } else {
            // tag.coordinates = QPointF(P2.x() + origin.x, P2.y() + origin.y);
            tag.coordinates = P2;
        }
        coordinateHistory[tag.tagID] = tag.coordinates;
    } else if ((anchor1Position.anchorID == 103 && anchor2Position.anchorID == 104) || (anchor1Position.anchorID == 104 && anchor2Position.anchorID == 103)) {
        // P2.setX(P.x() + h * (anchor2Position.y - anchor1Position.y) / anchorBaseline);
        // P2.setY(P.y() - h * (anchor2Position.x - anchor1Position.x) / anchorBaseline);
        if (P1.y() < P2.y()) {
            // tag.coordinates = QPointF(P1.x() + origin.x, P1.y() + origin.y);
            tag.coordinates = P1;
        } else {
            // tag.coordinates = QPointF(P2.x() + origin.x, P2.y() + origin.y);
            tag.coordinates = P2;
        }
        coordinateHistory[tag.tagID] = tag.coordinates;
    } else {
        if (coordinateHistory.contains(tag.tagID)) {
            double delta1 = std::abs(coordinateHistory[tag.tagID].x() - P1.x());
            double delta2 = std::abs(coordinateHistory[tag.tagID].x() - P2.x());
            if (h > 0 && (delta1 < 0.1 || delta2 < 0.1)){
                if (delta1 < delta2) {
                    tag.coordinates = P1;
                } else {
                    tag.coordinates = P2;
                }
                coordinateHistory[tag.tagID] = tag.coordinates;
            } else {
                tag.coordinates = coordinateHistory[tag.tagID];
            }
        } else {
            tag.coordinates = P2;
        }
    }

    // tag.coordinates.setX(x + origin.x);

    // qDebug() << "Intersection Point 1: (" << P1.x() << ", " << P1.y() << ")\n";
    // qDebug() << "Intersection Point 2: (" << P2.x() << ", " << P2.y() << ")\n";
}


UWBData DataProcessor::linearSearchUWB(const long long &frameTimestamp) {
    UWBData* closestUWB = &uwbDataVector[0];
    long long minDif = std::abs(frameTimestamp - closestUWB->timestamp);

    for (auto &data : uwbDataVector)
    {
        long long dif = std::abs(frameTimestamp - data.timestamp);
        if (dif < minDif)
        {
            closestUWB = &data;
            minDif = dif;
        }
    }

    return *closestUWB;
}


UWBData DataProcessor::binarySearchUWB(const long long &frameTimestamp, const std::vector<UWBData*>& uwbDataVectorPtr) {
    int left = 0;
    int right = uwbDataVectorPtr.size() - 1;

    UWBData* closestUWB = uwbDataVectorPtr[0];
    long long minDif = std::abs(frameTimestamp - closestUWB->timestamp);
    int mid;
    long long dif;

    while (left <= right) {
        mid = left + (right - left) / 2;
        dif = std::abs(frameTimestamp - uwbDataVectorPtr[mid]->timestamp);

        if (dif < minDif) {
            minDif = dif;
            closestUWB = uwbDataVectorPtr[mid];
        }

        if (frameTimestamp > uwbDataVectorPtr[mid]->timestamp) {
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }

    return *closestUWB;
}

UWBData DataProcessor::binarySearchUWB(const long long &frameTimestamp) {
    int left = 0;
    int right = uwbDataVector.size() - 1;

    UWBData* closestUWB = &uwbDataVector[0];
    long long minDif = std::abs(frameTimestamp - closestUWB->timestamp);
    int mid;
    long long dif;

    while (left <= right) {
        mid = left + (right - left) / 2;
        dif = std::abs(frameTimestamp - uwbDataVector[mid].timestamp);

        if (dif < minDif) {
            minDif = dif;
            closestUWB = &uwbDataVector[mid];
        }

        if (frameTimestamp > uwbDataVector[mid].timestamp) {
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }

    return *closestUWB;
}

int DataProcessor::binarySearchVideoFrameID(const long long &uwbTimestamp) {
    int left = 0;
    int right = videoTimestampsVector.size() - 1;
    int closestID = -1;

    long long minDif = std::abs(uwbTimestamp - videoTimestampsVector[0]);
    int mid;
    long long dif;

    while (left <= right) {
        mid = left + (right - left) / 2;
        dif = std::abs(uwbTimestamp - videoTimestampsVector[mid]);

        if (dif < minDif) {
            minDif = dif;
            closestID = mid;
        }

        if (uwbTimestamp > videoTimestampsVector[mid]) {
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }

    return closestID;
}

void DataProcessor::setRangeForDataAnalysis(const long long startTimeSec, const long long endTimeSec) {

    long long startTimestampMS = startTimeSec * 1000 + videoTimestampsVector[0];
    long long endTimestampMS = endTimeSec * 1000 + videoTimestampsVector[0];

    int startFrameIndex = binarySearchVideoFrameID(startTimestampMS);
    int endFrameIndex = binarySearchVideoFrameID(endTimestampMS);

    long long startFrameTimestamp = videoTimestampsVector[startFrameIndex];
    long long endFrameTimestamp = videoTimestampsVector[endFrameIndex];

    // To not copy subarray of elements, it is possible to use span
    UWBData startUWB = binarySearchUWB(startFrameTimestamp);
    UWBData endUWB = binarySearchUWB(endFrameTimestamp);

    uwbDataRangeToAnalyze = std::span<UWBData>(uwbDataVector.begin() + startUWB.id - 1, uwbDataVector.begin() + endUWB.id - 1);

    // Extract unique Tag IDs
    uniqueTagIDs.reserve(uwbDataRangeToAnalyze.size());

    std::transform(uwbDataRangeToAnalyze.begin(), uwbDataRangeToAnalyze.end(), std::back_inserter(uniqueTagIDs), [](const UWBData& obj) { return obj.tagID; });
    std::sort(uniqueTagIDs.begin(), uniqueTagIDs.end());
    std::vector<int>::iterator last = std::unique(uniqueTagIDs.begin(), uniqueTagIDs.end());

    uniqueTagIDs.erase(last, uniqueTagIDs.end());

    emit requestShowAvailableTags(uniqueTagIDs);
}

// Selecting Tag data and then choosing necassery Anchor ID when plotting is faster, than everytime creating new array of pair Tag - Anchor
// Tag is selected every time combobox is changed
void DataProcessor::collectDataForTag(const QString &tagIDText) {
    bool converted;
    int tagID = tagIDText.toInt(&converted);

    // tagDataToAnalyze.erase(tagDataToAnalyze.begin(), tagDataToAnalyze.end());
    tagDataToAnalyze.clear();

    std::vector<int> availableAnchorsForTag;
    for (UWBData& data: uwbDataRangeToAnalyze) {
        if (data.tagID == tagID) {
            tagDataToAnalyze.push_back(&data);
            for (const Anchor& anchor : data.anchorList) {
                if (std::find(availableAnchorsForTag.begin(), availableAnchorsForTag.end(), anchor.anchorID) == availableAnchorsForTag.end()) {
                    availableAnchorsForTag.push_back(anchor.anchorID);
                }
            }
        }
    }
    std::sort(availableAnchorsForTag.begin(), availableAnchorsForTag.end());
    emit requestShowAvailableAnchors(availableAnchorsForTag);
}

// /* Data for anchor is collected only when Analyze button is pressed.
// *  The reason it is not like with tag, because whenever anchor is selected from combobox there is still possibility to choose a tag.
// *  So, when a new tag is chosen, tagDataToAnalyze is changed, but anchorDataToAnalyze stays old, because combobox for anchor was not changed (and corresponding signal was not triggered).
// *  Therefore it is better to call signal to collect anchor data when Analyze button is pressed.
// */
// void DataProcessor::collectDataForAnchor(const int anchorID) {

//     anchorDataToAnalyze.erase(anchorDataToAnalyze.begin(), anchorDataToAnalyze.end());

//     for (const UWBData& data: tagDataToAnalyze) {
//         for (const Anchor& anchor: data.anchorList) {
//             if (anchor.anchorID == anchorID) {
//                 anchorDataToAnalyze.push_back(anchor);
//             }
//         }
//     }
// }

void DataProcessor::collectDataForPlotDistancesVsTimestamps(const int anchorID) {
    // timestampsToAnalyze.erase(timestampsToAnalyze.begin(), timestampsToAnalyze.end());
    // distancesToAnalyzeOriginal.erase(distancesToAnalyzeOriginal.begin(), distancesToAnalyzeOriginal.end());

    timestampsToAnalyze.clear();
    distancesToAnalyzeOriginal.clear();
    for (UWBData* data: tagDataToAnalyze) {
        for (Anchor& anchor: data->anchorList) {
            if (anchor.anchorID == anchorID) {
                timestampsToAnalyze.push_back(data->timestamp - videoTimestampsVector[0]);
                distancesToAnalyzeOriginal.push_back(&(anchor.distance));
            }
        }
    }

    emit requestShowPlotDistancesVsTimestamps(timestampsToAnalyze, distancesToAnalyzeOriginal);

}

void DataProcessor::calculateRollingDeviation(const int windowSize) {
    // rollingDeviations.erase(rollingDeviations.begin(), rollingDeviations.end());
    rollingDeviations.clear();

    // Ensure to handle cases where there are not enough elements to form a full window at the end
    int n = distancesToAnalyzeOriginal.size();
    for (int i = 0; i < n; ++i) {
        // Calculate mean for the window considering available elements
        double sum = 0.0;
        int count = 0; // Count actual elements in the window
        for (int j = i; j < i + windowSize && j < n; ++j, ++count) {
            sum += *distancesToAnalyzeOriginal[j];
        }
        double mean = sum / count;

        // Calculate sum of squares of differences from the mean for the actual window
        double sumSquares = 0.0;
        for (int j = i; j < i + windowSize && j < n; ++j) {
            sumSquares += std::pow(*distancesToAnalyzeOriginal[j] - mean, 2);
        }

        // Calculate standard deviation for the window
        double deviation = std::sqrt(sumSquares / count);
        rollingDeviations.push_back(deviation);
    }

    emit requestShowPlotRollingDeviations(timestampsToAnalyze, rollingDeviations);
}

void DataProcessor::splitDataset(const double threshold) {
    // segmentMeans.erase(segmentMeans.begin(), segmentMeans.end());
    segmentMeans.clear();
    segmentFrameIDs.clear();
    segmentSizes.clear();
    segmentRepresentatives.clear();

    // Detect segments where tag was standing and calculate mean for each segment
    // Assuming segments are continuous
    double segmentDistanceSum = 0.0;
    int segmentSize = 0;
    int i;
    for (i = 0; i < rollingDeviations.size(); ++i) {
        if (rollingDeviations[i] <= threshold) {
            segmentDistanceSum += *distancesToAnalyzeOriginal[i];
            segmentSize += 1;
        } else if (segmentDistanceSum > 0 && segmentSize > 0) {
            double segmentMean = segmentDistanceSum / segmentSize;
            segmentMeans.push_back(segmentMean);
            int middleIdx = segmentSize / 2;
            UWBData segmentRepresentative = *tagDataToAnalyze[i - middleIdx];
            int segmentVideoFrameID = binarySearchVideoFrameID(segmentRepresentative.timestamp);
            segmentFrameIDs.push_back(segmentVideoFrameID); // remember timestamp of the segment's middle record for model training, first frame of the segment is not always good choice
            segmentSizes.push_back(segmentSize);
            segmentRepresentatives.push_back(std::move(segmentRepresentative));
            segmentDistanceSum = 0;
            segmentSize = 0;
        }
    }

    if (segmentDistanceSum > 0 && segmentSize > 0) {
        double segmentMean = segmentDistanceSum / segmentSize;
        segmentMeans.push_back(segmentMean);
        int middleIdx = segmentSize / 2;
        UWBData segmentRepresentative = *tagDataToAnalyze[i - middleIdx];
        int segmentVideoFrameID = binarySearchVideoFrameID(segmentRepresentative.timestamp);
        segmentFrameIDs.push_back(segmentVideoFrameID); // remember timestamp of the segment's middle record for model training, first frame of the segment is not always good choice
        segmentSizes.push_back(segmentSize);
        segmentRepresentatives.push_back(std::move(segmentRepresentative));
    }

    emit requestShowDatasetSegments(segmentMeans);
}

std::vector<int> DataProcessor::getSegmentFrameIDs() {
    return segmentFrameIDs;
}

void DataProcessor::calculatePolynomialRegression(const std::vector<double>& referenceValues) {
    distancesToAnalyzeAdjusted.clear();

    int n = referenceValues.size();

    Eigen::MatrixXd A(n, 3);
    Eigen::VectorXd y(n);

    for (int i = 0; i < n; ++i) {
        A(i, 0) = 1;
        A(i, 1) = segmentMeans[i];
        A(i, 2) = std::pow(segmentMeans[i], 2);
        y(i) = referenceValues[i];
    }

    Eigen::VectorXd betaCoeff = (A.transpose() * A).ldlt().solve(A.transpose() * y);

    for (size_t i = 0; i < distancesToAnalyzeOriginal.size(); ++i) {
        double distance = *(distancesToAnalyzeOriginal[i]);
        distancesToAnalyzeAdjusted.push_back(betaCoeff(0) + betaCoeff(1) * distance + betaCoeff(2) * distance * distance);
    }



    emit requestShowOriginalVsAdjustedDistances(timestampsToAnalyze, distancesToAnalyzeOriginal, distancesToAnalyzeAdjusted);
}

void DataProcessor::updateOriginalWithAdjustedValues() {
    for (int i = 0; i < distancesToAnalyzeAdjusted.size(); ++i) {
        *(distancesToAnalyzeOriginal[i]) = distancesToAnalyzeAdjusted[i];
    }
}

QPointF DataProcessor::predictWorldCoordinatesPixelToReal(const DetectionResult& detection) {

    QPointF coordinates;

    // XGBooster Regressor
    DMatrixHandle dmatrix;
    const float pixelCoordinates[] = {static_cast<float>(detection.bottomEdgeCenter.x()), static_cast<float>(detection.bottomEdgeCenter.y())};
    XGDMatrixCreateFromMat(pixelCoordinates, 1, 2, NAN, &dmatrix);

    // Make prediction
    bst_ulong outLen;
    const float* outResult;
    XGBoosterPredict(booster, dmatrix, 0, 0, 0, &outLen, &outResult);

    auto found = std::find_if(anchorPositions.begin(), anchorPositions.end(), [](const AnchorPosition& pos) {
        return pos.isOrigin;
    });

    AnchorPosition origin = *found;

    coordinates = QPointF(static_cast<double>(outResult[0]) + origin.x - 0.627, static_cast<double>(outResult[1])); // 0.627 is the offset from the left wall in video on which model is trained
    // qDebug() << "Real-World coordinates predicted by model: (" << outResult[0] << ", " << outResult[1] << ")";

    // Cleanup
    XGDMatrixFree(dmatrix);

    return coordinates;

}

QPointF DataProcessor::predictWorldCoordinatesOptical(const DetectionResult& detection, const cv::Size& cameraFrameSize, const cv::Size& detectionFrameSize) {

    QPointF coordinates;
    int bboxHeight = detection.bbox.height;

    int imageX = detection.bottomEdgeCenter.x();
    int imageY = detection.bottomEdgeCenter.y();

    // Camera parameters are computed for 640x360 size of an image. Need a scale factor for 640x640.
    double scaleX = detectionFrameSize.width / cameraFrameSize.width;
    double scaleY = (double)detectionFrameSize.height / (double)cameraFrameSize.height;


    double fxAdjusted = cameraMatrix.at<double>(0, 0) * scaleX;
    double fyAdjusted = cameraMatrix.at<double>(1, 1) * scaleY;
    double cxAdjusted = cameraMatrix.at<double>(0, 2) * scaleX;
    double cyAdjusted = cameraMatrix.at<double>(1, 2) * scaleY;

    double personHeight = 1.78;

    if (imageX < 280) {
        personHeight = 1.78;
    } else if (280 <= imageX && imageX <= 315) {
        personHeight = 1.86;
    } else {
        personHeight = 1.78;
    }

    double distance = (personHeight * fyAdjusted) / bboxHeight;




    // cx and fx from intrinsic calibration
    double worldX = ((imageX - cxAdjusted) * (distance / fxAdjusted));
    // cy and fy from intrinsic calibration
    double worldY = (imageY - cyAdjusted) * (distance / fyAdjusted);

    // qDebug() << "Pixel Coordinates: (" << imageX << ", "<< imageY << ")";
    // qDebug() << "   Real-World coordinates calculated by height: (" << worldX << ", " << distance << ")" << " Z: " << worldY;
    auto found = std::find_if(anchorPositions.begin(), anchorPositions.end(), [](const AnchorPosition& pos) {
        return pos.isOrigin;
    });

    AnchorPosition origin = *found;

    coordinates = QPointF(worldX + origin.x + 1.25, distance - 0.15); // + origin.x - distance between the left wall to the left anchor, + 1.25 - distance between the camera and the left anchor

    return coordinates;
}

// Both predictions at the same time are possilbe. Optimized to detect people only once.
int DataProcessor::setPredict(bool toPredict, PredictionType type) {

    predictionType = type;
    if (type == PredictionType::PredictionByPixelToReal) {

        if (!booster) {
            return -1;
        }

        isPredictionByPixelToRealRequested = toPredict;
        emit requestChangePredictionButtonName(type, isPredictionByPixelToRealRequested);

    } else if (type == PredictionType::PredictionByOptical) {
        // if (!cameraMatrix.size()) {
        if (cameraMatrix.empty()) {
            return -1;
        }

        isPredictionByOpticalRequested = toPredict;
        emit requestChangePredictionButtonName(type, isPredictionByOpticalRequested);
    }

    return 0;
}

int DataProcessor::loadPixelToRealModelParams(const QString& filename) {
    int result = XGBoosterCreate(NULL, 0, &booster);
    if (result == 0) {
        result = XGBoosterLoadModel(booster, filename.toStdString().c_str());
        if (result == -1) {
            booster = nullptr;
        }
    }

    return result;
}

void DataProcessor::setCameraMatrix(const cv::Mat& matrix) {
    cameraMatrix = matrix;
}

// void DataProcessor::setOptimalCameraMatrix(std::vector<double> &&matrix) {
//     optimalCameraMatrix = std::move(matrix);
// }

// void DataProcessor::setDistCoeffs(std::vector<double> &&matrix) {
//     distCoeffs = std::move(matrix);
// }