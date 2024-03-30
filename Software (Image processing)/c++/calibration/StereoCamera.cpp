#include "StereoCamera.hpp"

const std::string StereoCamera::options = "rtsp_transport;tcp";
const std::string StereoCamera::envvar = "OPENCV_FFMPEG_CAPTURE_OPTIONS";

int StereoCamera::leftCameraIndex, StereoCamera::rightCameraIndex;
std::string StereoCamera::leftCameraURL, StereoCamera::rightCameraURL;
cv::VideoCapture StereoCamera::leftCamera, StereoCamera::rightCamera;

void StereoCamera::initSingleCamera(const int &leftCameraIndex)
{
    StereoCamera::leftCameraIndex = leftCameraIndex;

    leftCamera = cv::VideoCapture(leftCameraIndex);

    if (!leftCamera.isOpened())
    {
        throw std::runtime_error("Failed to open camera " + std::to_string(leftCameraIndex));
    }
}

void StereoCamera::initStereoCamera(const int &leftCameraIndex, const int &rightCameraIndex)
{
    StereoCamera::leftCameraIndex = leftCameraIndex;
    StereoCamera::rightCameraIndex = rightCameraIndex;
    leftCamera = cv::VideoCapture(leftCameraIndex);
    rightCamera = cv::VideoCapture(rightCameraIndex);

    if (!leftCamera.isOpened())
    {
        throw std::runtime_error("Failed to open camera " + std::to_string(leftCameraIndex));
    }

    if (!rightCamera.isOpened())
    {
        throw std::runtime_error("Failed to open camera " + std::to_string(rightCameraIndex));
    }
}

void StereoCamera::initStereoCamera(const std::string &leftCameraURL, const std::string &rightCameraURL)
{
    StereoCamera::leftCameraURL = leftCameraURL;
    StereoCamera::rightCameraURL = rightCameraURL;

    setenv(envvar.c_str(), options.c_str(), 1);

    leftCamera = cv::VideoCapture(leftCameraURL, cv::CAP_FFMPEG);
    rightCamera = cv::VideoCapture(rightCameraURL, cv::CAP_FFMPEG);

    if (!leftCamera.isOpened())
    {
        throw std::runtime_error("Failed to open camera " + leftCameraURL);
    }

    if (!rightCamera.isOpened())
    {
        throw std::runtime_error("Failed to open camera " + rightCameraURL);
    }
}

void StereoCamera::release()
{
    leftCamera.release();
    rightCamera.release();
    cv::destroyAllWindows();
}

cv::Mat StereoCamera::getLeftFrame()
{
    cv::Mat readLeftFrame;
    if (!leftCamera.read(readLeftFrame))
    {
        if (leftCameraURL != "")
            throw std::runtime_error("Failed to read the frame from the camera " + StereoCamera::leftCameraURL);
        else
            throw std::runtime_error("Failed to read the frame from the camera " + std::to_string(StereoCamera::leftCameraIndex));
    }

    if (leftCameraURL != "")
        cv::resize(readLeftFrame, readLeftFrame, cv::Size(640, 360));

    // cv::resize(readLeftFrame, readLeftFrame, cv::Size(416, 416));
    return readLeftFrame;
}

cv::Mat StereoCamera::getRightFrame()
{
    cv::Mat readRightFrame;

    if (!rightCamera.read(readRightFrame))
    {
        if (rightCameraURL != "")
            throw std::runtime_error("Failed to read the frame from the camera " + StereoCamera::rightCameraURL);
        else
            throw std::runtime_error("Failed to read the frame from the camera " + std::to_string(StereoCamera::rightCameraIndex));
    }

    if (rightCameraURL != "")
        cv::resize(readRightFrame, readRightFrame, cv::Size(640, 360));

    // cv::resize(readRightFrame, readRightFrame, cv::Size(416, 416));
    return readRightFrame;
}