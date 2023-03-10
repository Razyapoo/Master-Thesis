#ifndef DISPARITY_H
#define DISPARITY_H

#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>
#include <opencv2/calib3d.hpp>
#include <opencv2/ximgproc.hpp>

#include "StereoCamera.h"


class Disparity {
    public:
        Disparity(const StereoCamera& stereoCamera); 
        ~Disparity();
        
        void computeDepth(const std::string& intrinsicFilePath, const std::string& extrinsicFilePath, const std::string& rectificationFilePath);


    private: 
        void loadParameters(const std::string& intrinsicFilePath, const std::string& extrinsicFilePath, const std::string& rectificationFilePath);
        static void onMouse(int event, int x, int y, int flags, void* data);
        // cv::Rect computeROI()

        // Paths to files containing parameters
        // std::string intrinsicFilePath;
        // std::string extrinsicFilePath;
        // std::string rectificationFilePath;

        cv::FileStorage intrinsicFile;
        cv::FileStorage extrinsicFile;
        cv::FileStorage rectificationFile;

        // StereoCamera cameraLeft;
        // StereoCamera cameraRight;
        StereoCamera stereoCamera; 

        cv::Mat imageLeft, imageRight, grayLeft, grayRight, rectifiedLeft, rectifiedRight;
        cv::Mat optimalCameraMatrixLeft, mapLeftX, mapLeftY, mapRightX, mapRightY, disparityMatrix;

        cv::Point3f cameraPoint;
        cv::Point3f worldPoint;

        std::string filter;

        cv::Ptr<cv::StereoSGBM> sgbm;
        cv::Ptr<cv::ximgproc::DisparityWLSFilter> wlsFilter;
        int minDisparity, numDisparities, blockSize, P1, P2, disp12MaxDiff, preFilterCap, uniquenessRatio, speckleWindowSize, speckleRange;
        int mode = cv::StereoSGBM::MODE_SGBM_3WAY;

        double lambda, sigma; //Post-filter parameters
        cv::Mat disparity, depth, filteredDisparity;

        double baseline = 2;
        
        uint8_t key;

};

#endif 