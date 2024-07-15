#ifndef CALIBRATION_H
#define CALIBRATION_H

#include <iostream>
#include <stdio.h>
#include <vector>
#include <memory>
#include <filesystem>
#include "Camera.hpp"

class Calibrator
{
public:
    static cv::VideoCapture videoSource;

    // common variables
    static std::string intrinsicFilePath;
    static bool intrinsicParamsSaved;
    static uint8_t chessboardHeight;
    static uint8_t chessboardWidth;
    static uint8_t key;
    static uint16_t imageCounter;
    static float squareSize, alpha;
    static double reprojectionError;
    const static cv::Size chessboardSize, imageSize;
    const static cv::TermCriteria criteriaMono;
    const static int numberOfCells;
    static std::vector<std::vector<cv::Point3f>> objectPoints;
    static std::vector<std::vector<cv::Point2f>> imagePoints;
    static std::vector<cv::Point2f> corners;
    static std::vector<cv::Point3f> objectPoint;
    // Working with frames
    static cv::Mat image, gray;
    static bool found;
    // Intrinsic calibration
    static cv::Mat cameraMatrix, distortionCoeffs;
    static std::vector<cv::Mat> rotationVecs, translationVecs;
    static cv::Mat optimalCameraMatrix;


    // Methods
    static void initCameraCalibration();
    static void detectChessboard();
    static void intrinsicCalibration();
    static double getReprojectionError(const std::vector<std::vector<cv::Point3f>> &objectPoints, const std::vector<std::vector<cv::Point2f>> &imagePoints, const std::vector<cv::Mat> &rotationVecs, const std::vector<cv::Mat> &translationVecs, const cv::Mat &cameraMatrix, const cv::Mat &distortionCoeffs);
};

#endif