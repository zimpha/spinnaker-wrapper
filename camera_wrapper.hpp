#include "Spinnaker.h"
#include "SpinGenApi/SpinnakerGenApi.h"
#include <string>
#include <opencv2/opencv.hpp>
#include <exception>
#include <iostream>

using namespace Spinnaker;
using namespace Spinnaker::GenApi;
using namespace Spinnaker::GenICam;

class incomplete_image_exception: public std::exception {
    virtual const char* what() const throw() {
        return "Image incomplete";
    }
} myex;

class camera_manager {
private:
    SystemPtr system;
    CameraList camList;
    camera_manager() {
        system = System::GetInstance();
        camList = system->GetCameras();
    }
public:
    ~camera_manager() {
        camList.Clear();
        system->ReleaseInstance();
    }
    static camera_manager& the_manager() {
        static camera_manager m;
        return m;
    }
    CameraPtr get_camera(const std::string& serial_number) const {
        std::cout << camList.GetSize() << std::endl;
        return camList.GetBySerial(serial_number);
    }
};

class camera_wrapper {
public:
    camera_wrapper(std::string serialNumber) {
        cam_ptr = camera_manager::the_manager().get_camera(serialNumber);
        cam_ptr->Init();
        node_map = &cam_ptr->GetNodeMap();
    }
    ~camera_wrapper() {
        cam_ptr->DeInit();
    }
    void start() {
        cam_ptr->BeginAcquisition();
    }
    void end() {
        cam_ptr->EndAcquisition();
    }
    void set_gain(double gain) {
    }
    double get_gain() const {
    }
    void set_exposure_time(double exposure_time) {
        cam_ptr->ExposureAuto.SetValue(ExposureAutoEnums::ExposureAuto_Off);
        cam_ptr->ExposureMode.SetValue(ExposureModeEnums::ExposureMode_Timed);
        cam_ptr->ExposureTime.SetValue(exposure_time);
    }
    double get_exposure_time() const {
        return cam_ptr->ExposureTime.GetValue();
    }
    double get_frame_rate() const {
        return cam_ptr->AcquisitionFrameRate.GetValue();
    }
    void set_frame_rate(double frame_rate) {
        CBooleanPtr AcquisitionFrameRateEnable = node_map->GetNode("AcquisitionFrameRateEnable");
        if (!IsAvailable(AcquisitionFrameRateEnable) || !IsReadable(AcquisitionFrameRateEnable)) {
            std::cout << "Unable to retrieve frame rate enable." << std::endl;
            return;
        }
        AcquisitionFrameRateEnable->SetValue(1);
        cam_ptr->AcquisitionFrameRate.SetValue(frame_rate);
    }
    cv::Mat grab_next_image(const std::string& format = "bgr") {
        ImagePtr image_ptr= cam_ptr->GetNextImage();
    //    if (image_ptr->IsIncomplete()) {
    //        image_ptr->Release();
    //        throw myex;
    //    }
        int width = image_ptr->GetWidth();
        int height = image_ptr->GetHeight();
        ImagePtr converted_image_ptr;
        cv::Mat result;
        if (format == "bgr") {
            converted_image_ptr = image_ptr->Convert(PixelFormat_BGR8);
            cv::Mat temp_img(height, width, CV_8UC3, converted_image_ptr->GetData());
            result = temp_img.clone();
        } else if (format == "rgb") {
            converted_image_ptr = image_ptr->Convert(PixelFormat_RGB8);
            cv::Mat temp_img(height, width, CV_8UC3, converted_image_ptr->GetData());
            result = temp_img.clone();
        } else if (format == "gray") {
            converted_image_ptr = image_ptr->Convert(PixelFormat_Mono8);
            cv::Mat temp_img(height, width, CV_8UC1, converted_image_ptr->GetData());
            result = temp_img.clone();
        } else {
            throw std::invalid_argument("Invalid argument: format = " + format + ". Expected bgr, rgr, or gray.");
        }
        image_ptr->Release();
        return result;
    }
private:
    CameraPtr cam_ptr;
    INodeMap* node_map;
};
