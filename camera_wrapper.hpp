#include "Spinnaker.h"
#include "SpinGenApi/SpinnakerGenApi.h"
#include <string>
#include <opencv2/opencv.hpp>
#include <exception>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <chrono>

using namespace Spinnaker;
using namespace Spinnaker::GenApi;
using namespace Spinnaker::GenICam;

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

  // Print Device Info
  void print_device_info() {
    INodeMap &device_node_map = cam_ptr->GetTLDeviceNodeMap();
    FeatureList_t features;
    CCategoryPtr category = device_node_map.GetNode("DeviceInformation");
    if (IsAvailable(category) && IsReadable(category)) {
      category->GetFeatures(features);
      for (auto it = features.begin(); it != features.end(); ++it) {
        CNodePtr feature_node = *it;
        std::cout << feature_node->GetName() << " : ";
        CValuePtr value_ptr = (CValuePtr)feature_node;
        std::cout << (IsReadable(value_ptr) ? value_ptr->ToString() : "Node not readable");
        std::cout << std::endl;
      }
    } else {
      std::cout << "Device control information not available." << std::endl;
    }
  }

  // Start/End camera
  void start() {
    cam_ptr->AcquisitionMode.SetValue(AcquisitionModeEnums::AcquisitionMode_Continuous);
    cam_ptr->BeginAcquisition();
  }
  void end() {
    cam_ptr->EndAcquisition();
  }

  // Setting Hardware/Software Trigger
  void enable_hardware_trigger() {
    this->disable_trigger();
    cam_ptr->TriggerSource.SetValue(TriggerSourceEnums::TriggerSource_Line0);
    cam_ptr->TriggerMode.SetValue(TriggerModeEnums::TriggerMode_On);
    cam_ptr->TriggerSelector.SetValue(TriggerSelectorEnums::TriggerSelector_FrameStart);
    cam_ptr->TriggerActivation.SetValue(TriggerActivationEnums::TriggerActivation_RisingEdge);
  }
  void enable_software_trigger() {
    this->disable_trigger();
    cam_ptr->TriggerSource.SetValue(TriggerSourceEnums::TriggerSource_Software);
    cam_ptr->TriggerMode.SetValue(TriggerModeEnums::TriggerMode_On);
  }
  void disable_trigger() {
    if (cam_ptr->TriggerMode == NULL || cam_ptr->TriggerMode.GetAccessMode() != RW) {
      std::cout << "Unable to disable trigger mode. Aborting..." << std::endl;
      exit(-1);
    }
    cam_ptr->TriggerMode.SetValue(TriggerModeEnums::TriggerMode_Off);
  }

  // Setting Black Level
  void enable_black_level_auto() {
    cam_ptr->BlackLevelAuto.SetValue(BlackLevelAutoEnums::BlackLevelAuto_Continuous);
  }
  void disable_black_level_auto() {
    cam_ptr->BlackLevelAuto.SetValue(BlackLevelAutoEnums::BlackLevelAuto_Off);
  }
  void set_black_level(double black_level) {
    cam_ptr->BlackLevel.SetValue(black_level);
  }
  double get_black_level() const {
    return cam_ptr->BlackLevel.GetValue();
  }

  // Setting Frame Rate
  void enable_frame_rate_auto() {
    CBooleanPtr AcquisitionFrameRateEnable = node_map->GetNode("AcquisitionFrameRateEnable");
    if (!IsAvailable(AcquisitionFrameRateEnable) || !IsReadable(AcquisitionFrameRateEnable)) {
      std::cout << "Unable to enable frame rate." << std::endl;
      return;
    }
    AcquisitionFrameRateEnable->SetValue(0);
  }
  void disable_frame_rate_auto() {
    CBooleanPtr AcquisitionFrameRateEnable = node_map->GetNode("AcquisitionFrameRateEnable");
    if (!IsAvailable(AcquisitionFrameRateEnable) || !IsReadable(AcquisitionFrameRateEnable)) {
      std::cout << "Unable to enable frame rate." << std::endl;
      return;
    }
    AcquisitionFrameRateEnable->SetValue(1);
  }
  void set_frame_rate(double frame_rate) {
    cam_ptr->AcquisitionFrameRate.SetValue(frame_rate);
  }
  double get_frame_rate() const {
    return cam_ptr->AcquisitionFrameRate.GetValue();
  }

  // Setting Exposure Time, us
  void enable_exposure_auto() {
    cam_ptr->ExposureAuto.SetValue(ExposureAutoEnums::ExposureAuto_Continuous);
  }
  void disable_exposure_auto() {
    cam_ptr->ExposureAuto.SetValue(ExposureAutoEnums::ExposureAuto_Off);
    cam_ptr->ExposureMode.SetValue(ExposureModeEnums::ExposureMode_Timed);
  }
  void set_exposure_time(double exposure_time) {
    cam_ptr->ExposureTime.SetValue(exposure_time);
  }
  double get_exposure_time() const {
    return cam_ptr->ExposureTime.GetValue();
  }

  // Setting Gain
  void enable_gain_auto() {
    cam_ptr->GainAuto.SetValue(GainAutoEnums::GainAuto_Continuous);
  }
  void disable_gain_auto() {
    cam_ptr->GainAuto.SetValue(GainAutoEnums::GainAuto_Off);
  }
  void set_gain(double gain) {
    cam_ptr->Gain.SetValue(gain);
  }
  double get_gain() const {
    return cam_ptr->Gain.GetValue();
  }

  // Setting Gamma
  void set_gamma(double gamma) {
    cam_ptr->Gamma.SetValue(gamma);
  }
  double get_gamme() const {
    return cam_ptr->Gamma.GetValue();
  }

  // Setting White Balance
  void enable_white_balance_auto() {
    cam_ptr->BalanceWhiteAuto.SetValue(BalanceWhiteAutoEnums::BalanceWhiteAuto_Continuous);
  }
  void disable_white_balance_auto() {
    cam_ptr->BalanceWhiteAuto.SetValue(BalanceWhiteAutoEnums::BalanceWhiteAuto_Off);
  }
  void set_white_balance_blue(double value) {
    cam_ptr->BalanceRatioSelector.SetValue(BalanceRatioSelectorEnums::BalanceRatioSelector_Blue);
    CFloatPtr BalanceRatio = node_map->GetNode("BalanceRatio");
    BalanceRatio->SetValue(value);
  }
  void set_white_balance_red(double value) {
    cam_ptr->BalanceRatioSelector.SetValue(BalanceRatioSelectorEnums::BalanceRatioSelector_Red);
    CFloatPtr BalanceRatio = node_map->GetNode("BalanceRatio");
    BalanceRatio->SetValue(value);
  }
  double get_white_balance_blue() const {
    cam_ptr->BalanceRatioSelector.SetValue(BalanceRatioSelectorEnums::BalanceRatioSelector_Blue);
    CFloatPtr BalanceRatio = node_map->GetNode("BalanceRatio");
    return BalanceRatio->GetValue();
  }
  double get_white_balance_red() const {
    cam_ptr->BalanceRatioSelector.SetValue(BalanceRatioSelectorEnums::BalanceRatioSelector_Red);
    CFloatPtr BalanceRatio = node_map->GetNode("BalanceRatio");
    return BalanceRatio->GetValue();
  }

  // Get Image Timestamp
  uint64_t get_system_timestamp() const {
    return system_timestamp;
  }
  uint64_t get_image_timestamp() const {
    return image_timestamp;
  }

  // Grab Next Image
  cv::Mat grab_next_image(const std::string& format = "bgr") {
    ImagePtr image_ptr= cam_ptr->GetNextImage();
    system_timestamp = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    image_timestamp = image_ptr->GetTimeStamp();
    int width = image_ptr->GetWidth();
    int height = image_ptr->GetHeight();
    cv::Mat result;
    if (format == "bgr") {
      ImagePtr converted_image_ptr = image_ptr->Convert(PixelFormat_BGR8);
      cv::Mat temp_img(height, width, CV_8UC3, converted_image_ptr->GetData());
      result = temp_img.clone();
    } else if (format == "rgb") {
      ImagePtr converted_image_ptr = image_ptr->Convert(PixelFormat_RGB8);
      cv::Mat temp_img(height, width, CV_8UC3, converted_image_ptr->GetData());
      result = temp_img.clone();
    } else if (format == "gray") {
      ImagePtr converted_image_ptr = image_ptr->Convert(PixelFormat_Mono8);
      cv::Mat temp_img(height, width, CV_8UC1, converted_image_ptr->GetData());
      result = temp_img.clone();
    } else {
      throw std::invalid_argument("Invalid argument: format = " + format + ". Expected bgr, rgr, or gray.");
    }
    image_ptr->Release();
    return result;
  }
  void trigger_software_execute() {
    cam_ptr->TriggerSoftware.Execute();
  }

private:
  CameraPtr cam_ptr;
  INodeMap* node_map;
  uint64_t system_timestamp;
  uint64_t image_timestamp;
};

struct image_t {
  cv::Mat img;
  int idx;
  uint64_t sys_ts, img_ts;
  std::string prefix;
  image_t() noexcept {}
  image_t(const cv::Mat& _img, int _idx, uint64_t _sys_ts, uint64_t _img_ts, const std::string& _prefix) noexcept:
    img(_img), idx(_idx), sys_ts(_sys_ts), img_ts(_img_ts), prefix(_prefix) {}
  image_t(const image_t& rhs) noexcept:
    img(rhs.img), idx(rhs.idx), sys_ts(rhs.sys_ts), img_ts(rhs.img_ts), prefix(rhs.prefix) {}
  image_t(image_t&& rhs) noexcept:
    img(std::move(rhs.img)), idx(std::move(idx)), sys_ts(std::move(rhs.sys_ts)), img_ts(std::move(rhs.img_ts)), prefix(std::move(rhs.prefix)) {}
  image_t& operator=(image_t&& rhs) noexcept {
    img = std::move(rhs.img);
    idx = std::move(rhs.idx);
    sys_ts = std::move(rhs.sys_ts);
    img_ts = std::move(rhs.img_ts);
    prefix = std::move(rhs.prefix);
    return *this;
  }
  void save() const noexcept {
    std::ostringstream filename;
    filename << prefix << '-' << std::setfill('0') << std::setw(8) << idx << '-';
    filename << sys_ts << '-' << img_ts << ".jpg";
    cv::imwrite(filename.str().c_str(), img);
  }
};
