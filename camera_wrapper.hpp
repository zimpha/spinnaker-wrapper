#include "Spinnaker.h"
#include "SpinGenApi/SpinnakerGenApi.h"
#include <string>
#include <opencv2/opencv.hpp>
#include <exception>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <sys/stat.h>

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
    std::cout << camList.GetSize()<< std::endl;
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

  /************************** Acquisition Control *****************************/

  /**
   * Sets the acquisition mode of the device. It defines mainly the number of
   * frames to capture during an acquisition and the way the acquisition stops.
   * There are three acquisition modes:
   *   + Continuous - acquires images continuously. This is the default mode.
   *   + SingleFrame - acquires 1 image before stopping acquisition.
   *   + MultiFrame - acquires a specified number of images before stopping
   *                  acquisition. The number of frames is specified by
   *                  AcquisitionFrameCount.
   */
  void set_acquisition_mode(const std::string &mode = "Continuous") {
    auto mode = AcquisitionModeEnums::AcquisitionMode_Continuous;
    if (mode == "Continuous") {
      mode_value = AcquisitionModeEnums::AcquisitionMode_Continuous;
    } else if (mode == "SingleFrame") {
      mode_value = AcquisitionModeEnums::AcquisitionMode_SingleFrame;
    } else if (mode == "MultiFrame") {
      mode_value = AcquisitionModeEnums::AcquisitionMode_MultiFrame;
    } else {
      throw std::invalid_argument("Invalid argument: mode = " + mode + ".");
    }
    cam_ptr->AcquisitionMode.SetValue(mode);
  }
  std::string get_acquisition_mode() const {
    auto mode = cam_ptr->AcquisitionMode.GetValue();
    if (mode == AcquisitionModeEnums::AcquisitionMode_Continuous) {
      return "Continuous";
    } else if (mode == AcquisitionModeEnums::AcquisitionMode_SingleFrame) {
      return "SingleFrame";
    } else if (mode == AcquisitionModeEnums::AcquisitionMode_MultiFrame) {
      return "MultiFrame";
    } else {
      return "Unknown";
    }
  }

  /**
   * start the acquisition of images.
   */
  void start_acquisition() {
    cam_ptr->BeginAcquisition();
  }

  /**
   * stop the acquisition of images.
   */
  void stop_acquisition() {
    cam_ptr->EndAcquisition();
  }

  /**
   * set acquisition frame count.
   */
  void set_acquisition_frame_count(int64_t count) {
    cam_ptr->AcquisitionFrameCount.SetValue(count);
  }
  int64_t get_acquisition_frame_count() const {
    return cam_ptr->AcquisitionFrameCount.GetValue();
  }

  /**
   * This feature is used only if the FrameBurstStart trigger is enabled and the
   * FrameBurstEnd trigger is disabled. Note that the total number of frames
   * captured is also conditioned by AcquisitionFrameCount if AcquisitionMode
   * is MultiFrame and ignored if AcquisitionMode is Single.
   */
  void set_acquisition_burst_frame_count(int64_t count) {
    cam_ptr->AcquisitionBurstFrameCount.SetValue(count);
  }
  int64_t get_acquisition_burst_frame_count() const {
    return cam_ptr->AcquisitionBurstFrameCount.GetValue();
  }

  /**
   * Set the operation mode of the Exposure. There are four exposure time modes:
   *   + Off - Disables the Exposure and let the shutter open.
   *   + Timed - The exposure duration time is set using the ExposureTime or
   *             ExposureAuto features and the exposure starts with the FrameStart
   *             or LineStart. This is the default mode.
   *   + TriggerWidth - Uses the width of the current Frame or Line trigger
   *                    signal(s) pulse to control the exposure duration. Note
   *                    that if the Frame or Line TriggerActivation is RisingEdge
   *                    or LevelHigh, the exposure duration will be the time the
   *                    trigger stays High. If TriggerActivation is FallingEdge
   *                    or LevelLow, the exposure time will last as long as the
   *                    trigger stays Low.
   *   + TriggerControlled - Uses one or more trigger signal(s) to control the
   *                         exposure duration independently from the current Frame
   *                         or Line triggers. See ExposureStart, ExposureEnd and
   *                         ExposureActive of the TriggerSelector feature.
   */
  void set_exposure_mode(const std::string &mode) {
    auto mode_value = ExposureModeEnums::ExposureMode_Timed;
    if (mode == "Off") {
      mode_value = ExposureModeEnums::ExposureMode_Off;
    } else if (mode == "Timed") {
      mode_value = ExposureModeEnums::ExposureMode_Timed;
    } else if (mode == "TriggerWidth") {
      mode_value = ExposureModeEnums::ExposureMode_TriggerWidth;
    } else if (mode == "TriggerControlled") {
      mode_value = ExposureModeEnums::ExposureMode_TriggerControlled;
    } else {
      throw std::invalid_argument("Invalid argument: mode = " + mode + ".");
    }
    cam_ptr->ExposureMode.SetValue(mode_value);
  }
  std::string get_exposure_mode() const {
    auto mode = cam_ptr->ExposureMode.GetValue();
    if (mode == ExposureModeEnums::ExposureMode_Off) {
      return "Off";
    } else if (mode == ExposureModeEnums::ExposureMode_Timed) {
      return "Timed";
    } else if (mode == ExposureModeEnums::ExposureMode_TriggerWidth) {
      return "TriggerWidth";
    } else if (mode == ExposureModeEnums::ExposureMode_TriggerControlled) {
      return "TriggerControlled";
    } else {
      return "Unknown";
    }
  }

  /**
   * Exposure time in microseconds when Exposure Mode is Timed.
   * Range: 3us ~ 30s
   */
  void set_exposure_time(double exposure_time) {
    cam_ptr->ExposureTime.SetValue(exposure_time);
  }
  double get_exposure_time() const {
    return cam_ptr->ExposureTime.GetValue();
  }

  /**
   * Sets the automatic exposure mode when ExposureMode is Timed. The exact
   * algorithm used to implement this control is device-specific. There are three
   * automatic exposure modes:
   *   + Off - Exposure duration is user controlled using ExposureTime.
   *   + Once - Exposure duration is adapted once by the device. Once it has
   *            converged, it returns to the Off state.
   *   + Continuous - Exposure duration is constantly adapted by the device to
   *                  maximize the dynamic range.
   */
  void set_automatic_exposure_mode(const std::string &mode) {
    auto mode_value = ExposureAutoEnums::ExposureAuto_Continuous;
    if (mode == "Off") {
      mode_value = ExposureAutoEnums::ExposureAuto_Off;
    } else if (mode == "Once") {
      mode_value = ExposureAutoEnums::ExposureAuto_Once;
    } else if (mode == "Continuous") {
      mode_value = ExposureAutoEnums::ExposureAuto_Continuous;
    } else {
      throw std::invalid_argument("Invalid argument: mode = " + mode + ".");
    }
    cam_ptr->ExposureAuto.SetValue(mode_value);
  }
  std::string get_automatic_exposure_mode() const {
    auto mode = cam_ptr.ExposureAuto.GetValue();
    if (mode == ExposureAutoEnums::ExposureAuto_Off) {
      return "Off";
    } else if (mode == ExposureAutoEnums::ExposureAuto_Once) {
      return "Once";
    } else if (mode == ExposureAutoEnums::ExposureAuto_Continuous) {
      return "Continuous";
    } else {
      return "Unknown";
    }
  }

  /**
   * enable automatic frame rate control. AcquisitionFrameRate can be used to
   * manually control the frame rate.
   */
  void enable_frame_rate_auto() {
    CBooleanPtr AcquisitionFrameRateEnable = node_map->GetNode("AcquisitionFrameRateEnable");
    if (!IsAvailable(AcquisitionFrameRateEnable) || !IsReadable(AcquisitionFrameRateEnable)) {
      std::cout << "Unable to enable automatic frame rate control." << std::endl;
      return;
    }
    AcquisitionFrameRateEnable->SetValue(false);
  }

  /**
   * disable automatic frame rate control.
   */
  void disable_frame_rate_auto() {
    CBooleanPtr AcquisitionFrameRateEnable = node_map->GetNode("AcquisitionFrameRateEnable");
    if (!IsAvailable(AcquisitionFrameRateEnable) || !IsReadable(AcquisitionFrameRateEnable)) {
      std::cout << "Unable to disable automatic frame rate control." << std::endl;
      return;
    }
    AcquisitionFrameRateEnable->SetValue(true);
  }

  /**
   * set user controlled acquisition frame rate in Hertz.
   */
  void set_frame_rate(double frame_rate) {
    cam_ptr->AcquisitionFrameRate.SetValue(frame_rate);
  }
  double get_frame_rate() const {
    return cam_ptr->AcquisitionFrameRate.GetValue();
  }

  /**
   * Resulting frame rate in Hertz. If this does not equal the Acquisition Frame
   * Rate it is because the Exposure Time is greater than the frame time.
   */
  double get_actual_frame_rate() const {
    CFloatPtr AcquisitionResultingFrameRate = node_map->GetNode("AcquisitionResultingFrameRate");
    if (!IsAvailable(AcquisitionResultingFrameRate) || !IsReadable(AcquisitionResultingFrameRate)) {
      std::cout << "Unable to get actual frame rate." << std::endl;
      return -1;
    }
    return AcquisitionResultingFrameRate->GetValue();
  }

  /**
   * Selects the type of trigger to configure. There are thirteen selectors:
   *   + AcquisitionStart - Selects a trigger that starts the Acquisition of one
   *                        or many frames according to AcquisitionMode.
   *   + AcquisitionEnd - Selects a trigger that ends the Acquisition of one or
   *                      many frames according to AcquisitionMode.
   *   + AcquisitionActive - Selects a trigger that controls the duration of the
   *                         Acquisition of one or many frames. The Acquisition
   *                         is activated when the trigger signal becomes active
   *                         and terminated when it goes back to the inactive state.
   *   + FrameStart - Selects a trigger starting the capture of one frame.
   *   + FrameEnd - Selects a trigger ending the capture of one frame (mainly
   *                used in line scan mode).
   *   + FrameActive - Selects a trigger controlling the duration of one frame
   *                   (mainly used in line scan mode).
   *   + FrameBurstStart - Selects a trigger starting the capture of the bursts
   *                       of frames in an acquisition. AcquisitionBurstFrameCount
   *                       controls the length of each burst unless a FrameBurstEnd
   *                       trigger is active. The total number of frames captured
   *                       is also conditioned by AcquisitionFrameCount if
   *                       AcquisitionMode is MultiFrame.
   *   + FrameBurstEnd - Selects a trigger ending the capture of the bursts of
   *                     frames in an acquisition.
   *   + FrameBurstActive - Selects a trigger controlling the duration of the
   *                        capture of the bursts of frames in an acquisition.
   *   + LineStart - Selects a trigger starting the capture of one Line of a
   *                 Frame (mainly used in line scan mode).
   *   + ExposureStart - Selects a trigger controlling the start of the exposure
   *                     of one Frame (or Line).
   *   + ExposureEnd - Selects a trigger controlling the end of the exposure of
   *                   one Frame (or Line).
   *   + ExposureActive - Selects a trigger controlling the duration of the
   *                      exposure of one frame (or Line).
   */
  void set_trigger_selector(const std::string &selector) {
    auto selector_value = TriggerSelectorEnums::TriggerSelector_FrameStart;
    if (selector == "AcquisitionStart") {
      selector_value = TriggerSelectorEnums::TriggerSelector_AcquisitionStart;
    } else if (selector == "AcquisitionEnd") {
      selector_value = TriggerSelectorEnums::TriggerSelector_AcquisitionEnd;
    } else if (selector == "AcquisitionActive") {
      selector_value = TriggerSelectorEnums::TriggerSelector_AcquisitionActive;
    } else if (selector == "FrameStart") {
      selector_value = TriggerSelectorEnums::TriggerSelector_FrameStart;
    } else if (selector == "FrameEnd") {
      selector_value = TriggerSelectorEnums::TriggerSelector_FrameEnd;
    } else if (selector == "FrameActive") {
      selector_value = TriggerSelectorEnums::TriggerSelector_FrameActive;
    } else if (selector == "FrameBurstStart") {
      selector_value = TriggerSelectorEnums::TriggerSelector_FrameBurstStart;
    } else if (selector == "FrameBurstEnd") {
      selector_value = TriggerSelectorEnums::TriggerSelector_FrameBurstEnd;
    } else if (selector == "FrameBurstActive") {
      selector_value = TriggerSelectorEnums::TriggerSelector_FrameBurstActive;
    } else if (selector == "LineStart") {
      selector_value = TriggerSelectorEnums::TriggerSelector_LineStart;
    } else if (selector == "ExposureStart") {
      selector_value = TriggerSelectorEnums::TriggerSelector_ExposureStart;
    } else if (selector == "ExposureEnd") {
      selector_value = TriggerSelectorEnums::TriggerSelector_ExposureEnd;
    } else if (selector == "ExposureActive") {
      selector_value = TriggerSelectorEnums::TriggerSelector_ExposureActive;
    } else {
      throw std::invalid_argument("Invalid argument: selector = " + selector + ".");
    }
    cam_ptr->TriggerSelector.SetValue(selector_value);
  }
  std::string get_trigger_selector() const {
    auto selector = cam_ptr->TriggerSelector.GetValue(selector_value);
    if (selector == TriggerSelectorEnums::TriggerSelector_AcquisitionStart) {
      return "AcquisitionStart";
    } else if (selector == TriggerSelectorEnums::TriggerSelector_AcquisitionEnd) {
      return "AcquisitionEnd";
    } else if (selector == TriggerSelectorEnums::TriggerSelector_AcquisitionActive) {
      return "AcquisitionActive";
    } else if (selector == TriggerSelectorEnums::TriggerSelector_FrameStart) {
      return "FrameStart";
    } else if (selector == TriggerSelectorEnums::TriggerSelector_FrameEnd) {
      return "FrameEnd";
    } else if (selector == TriggerSelectorEnums::TriggerSelector_FrameActive) {
      return "FrameActive";
    } else if (selector == TriggerSelectorEnums::TriggerSelector_FrameBurstStart) {
      return "FrameBurstStart";
    } else if (selector == TriggerSelectorEnums::TriggerSelector_FrameBurstEnd) {
      return "FrameBurstEnd";
    } else if (selector == TriggerSelectorEnums::TriggerSelector_FrameBurstActive) {
      return "FrameBurstActive";
    } else if (selector == TriggerSelectorEnums::TriggerSelector_LineStart) {
      return "LineStart";
    } else if (selector == TriggerSelectorEnums::TriggerSelector_ExposureStart) {
      return "ExposureStart";
    } else if (selector == TriggerSelectorEnums::TriggerSelector_ExposureEnd) {
      return "ExposureEnd";
    } else if (selector == TriggerSelectorEnums::TriggerSelector_ExposureActive) {
      return "ExposureActive";
    } else {
      return "Unknown";
    }
  }

  /**
   * Controls if the selected trigger is active. There are two modes:
   *   + Off - Disables the selected trigger. This is the default mode.
   *   + On - Enable the selected trigger.
   */
  void set_trigger_mode(const std::string &mode) {
    auto mode_value = TriggerModeEnums::TriggerMode_Off;
    if (mode == "Off") {
      mode_value = TriggerModeEnums::TriggerMode_Off;
    } else if (mode == "On") {
      mode_value = TriggerModeEnums::TriggerMode_On;
    } else {
      throw std::invalid_argument("Invalid argument: mode = " + mode + ".");
    }
    cam_ptr->TriggerMode.SetValue(mode_value);
  }
  std::string get_trigger_mode() const {
    auto mode = cam_ptr->TriggerMode.GetValue();
    if (mode == TriggerModeEnums::TriggerMode_Off) {
      return "On";
    } else if (mode == TriggerModeEnums::TriggerMode_On) {
      return "Off";
    } else {
      return "Unknown";
    }
  }

  /**
   * Generates an internal trigger if Trigger Source is set to Software.
   */
  void trigger_software_execute() {
    cam_ptr->TriggerSoftware.Execute();
  }

  /**
   * Specifies the internal signal or physical input Line to use as the trigger
   * source. The selected trigger must have its TriggerMode set to On.
   *
   *  + Software - Specifies that the trigger source will be generated by
   *               software using the TriggerSoftware command.
   *  + SoftwareSignal0 - Specifies that the trigger source will be a signal generated
   *                      by software using the SoftwareSignalPulse command.
   *  + SoftwareSignal1 - Specifies that the trigger source will be a signal generated
   *                      by software using the SoftwareSignalPulse command.
   *  + SoftwareSignal2 - Specifies that the trigger source will be a signal generated
   *                      by software using the SoftwareSignalPulse command.
   *  + Line0 - Specifies which physical line (or pin) and associated I/O control
   *            block to use as external source for the trigger signal.
   *  + Line1 - Specifies which physical line (or pin) and associated I/O control
   *            block to use as external source for the trigger signal.
   *  + Line2 - Specifies which physical line (or pin) and associated I/O control
   *            block to use as external source for the trigger signal.
   *  + Counter0Start - Specifies which of the Counter signal to use as internal
   *                    source for the trigger.
   *  + Counter1Start - Specifies which of the Counter signal to use as internal
   *                    source for the trigger.
   *  + Counter2Start - Specifies which of the Counter signal to use as internal
   *                    source for the trigger.
   *  + Counter0End - Specifies which of the Counter signal to use as internal
   *                  source for the trigger.
   *  + Counter1End - Specifies which of the Counter signal to use as internal
   *                  source for the trigger.
   *  + Counter2End - Specifies which of the Counter signal to use as internal
   *                  source for the trigger.
   *  + Timer0Start - Specifies which Timer signal to use as internal source for
   *                  the trigger.
   *  + Timer1Start - Specifies which Timer signal to use as internal source for
   *                  the trigger.
   *  + Timer2Start - Specifies which Timer signal to use as internal source for
   *                  the trigger.
   *  + Timer0End - Specifies which Timer signal to use as internal source for
   *                the trigger.
   *  + Timer1End - Specifies which Timer signal to use as internal source for
   *                the trigger.
   *  + Timer2End - Specifies which Timer signal to use as internal source for
   *                the trigger.
   *  + Encoder0 - Specifies which Encoder signal to use as internal source for
   *               the trigger.
   *  + Encoder1 - Specifies which Encoder signal to use as internal source for
   *               the trigger.
   *  + Encoder2 - Specifies which Encoder signal to use as internal source for
   *               the trigger.
   *  + UserOutput0 - Specifies which User Output bit signal to use as internal
   *                  source for the trigger.
   *  + UserOutput1 - Specifies which User Output bit signal to use as internal
   *                  source for the trigger.
   *  + UserOutput2 - Specifies which User Output bit signal to use as internal
   *                  source for the trigger.
   *  + Action0 - Specifies which Action command to use as internal source for
   *              the trigger.
   *  + Action1 - Specifies which Action command to use as internal source for
   *              the trigger.
   *  + Action2 - Specifies which Action command to use as internal source for
   *              the trigger.
   *  + LinkTrigger0 - Specifies which Link Trigger to use as  source for the
   *                   trigger (received from the transport layer).
   *  + LinkTrigger1 - Specifies which Link Trigger to use as  source for the
   *                   trigger (received from the transport layer).
   *  + LinkTrigger2 - Specifies which Link Trigger to use as  source for the
   *                   trigger (received from the transport layer).
   *  + CC1 - Index of the Camera Link physical line and associated I/O control
   *          block to use. This ensures a direct mapping between the lines on
   *          the frame grabber and on the camera. Applicable to CameraLink
   *          products only.
   *  + CC2 - Index of the Camera Link physical line and associated I/O control
   *          block to use. This ensures a direct mapping between the lines on
   *          the frame grabber and on the camera. Applicable to CameraLink
   *          products only.
   *  + CC3 - Index of the Camera Link physical line and associated I/O control
   *          block to use. This ensures a direct mapping between the lines on
   *          the frame grabber and on the camera. Applicable to CameraLink
   *          products only.
   *  + CC4 - Index of the Camera Link physical line and associated I/O control
   *          block to use. This ensures a direct mapping between the lines on
   *          the frame grabber and on the camera. Applicable to CameraLink
   *          products only.
   */
  void set_trigger_source(const std::string &source) {
    // TODO: add other source
    auto source_value = TriggerSourceEnums::TriggerSource_Software;
    if (source == "Software") {
      source_value = TriggerSourceEnums::TriggerSource_Software;
    } else if (source == "Line0") {
      source_value = TriggerSourceEnums::TriggerSource_Line0;
    } else {
      throw std::invalid_argument("Invalid argument: source = " + source + ".");
    }
    cam_ptr->TriggerSource.SetValue(source_value);
  }
  std::string get_trigger_source() const {
    auto source = cam_ptr->TriggerSource.GetValue();
    if (source == TriggerSourceEnums::TriggerSource_Software) {
      return "Software";
    } else if (source == TriggerSourceEnums::TriggerSource_Line0) {
      return "Line0";
    } else {
      return "Unknown";
    }
  }

  /**
   * Specifies the activation mode of the trigger.
   *
   *  + RisingEdge - Specifies that the trigger is considered valid on the rising
   *                 edge of the source signal.
   *  + FallingEdge - Specifies that the trigger is considered valid on the falling
   *                  edge of the source signal.
   *  + AnyEdge - Specifies that the trigger is considered valid on the falling
   *              edge of the source signal.
   *  + LevelHigh - Specifies that the trigger is considered valid as long as the
   *                level of the source signal is high.
   *  + LevelLow - Specifies that the trigger is considered valid as long as the
   *               level of the source signal is low.
   */
  void set_trigger_activation(const std::string &activation) {
    auto activation_value = TriggerActivationEnums::TriggerActivation_RisingEdge;
    if (activation == "RisingEdge") {
      activation_value = TriggerActivationEnums::TriggerActivation_RisingEdge;
    } else if (activation == "FallingEdge") {
      activation_value = TriggerActivationEnums::TriggerActivation_FallingEdge;
    } else if (activation == "AnyEdge") {
      activation_value = TriggerActivationEnums::TriggerActivation_AnyEdge;
    } else if (activation == "LevelHigh") {
      activation_value = TriggerActivationEnums::TriggerActivation_LevelHigh;
    } else if (activation == "LevelLow") {
      activation_value = TriggerActivationEnums::TriggerActivation_LevelLow;
    } else {
      throw std::invalid_argument("Invalid argument: activation = " + activation + ".");
    }
    cam_ptr->TriggerActivation.SetValue(activation_value);
  }
  std::string get_trigger_activation() const {
    auto activation = cam_ptr->TriggerActivation.GetValue();
    if (activation == TriggerActivationEnums::TriggerActivation_RisingEdge) {
      return "RisingEdge";
    } else if (activation == TriggerActivationEnums::TriggerActivation_FallingEdge) {
      return "FallingEdge";
    } else if (activation == TriggerActivationEnums::TriggerActivation_AnyEdge) {
      return "AnyEdge";
    } else if (activation == TriggerActivationEnums::TriggerActivation_LevelHigh) {
      return "LevelHigh";
    } else if (activation == TriggerActivationEnums::TriggerActivation_LevelLow) {
      return "LevelLow";
    } else {
      return "Unknown";
    }
  }

  /**
   * Specifies the type trigger overlap permitted with the previous frame or line.
   * This defines when a valid trigger will be accepted (or latched) for a new
   * frame or a new line.
   *
   *  + Off - No trigger overlap is permitted.
   *  + ReadOut - Trigger is accepted immediately after the exposure period.
   *  + PreviousFrame - Trigger is accepted (latched) at any time during the
   *                    capture of the previous frame.
   *  + PreviousLine - Trigger is accepted (latched) at any time during the
   *                   capture of the previous line.
   */
  void set_trigger_overlap(const std::string &overlap) {
    auto overlap_value = TriggerOverlapEnums::TriggerOverlap_Off;
    if (overlap == "Off") {
      overlap_value = TriggerOverlapEnums::TriggerOverlap_Off;
    } else if (overlap == "ReadOut") {
      overlap_value = TriggerOverlapEnums::TriggerOverlap_ReadOut;
    } else if (overlap == "PreviousFrame") {
      overlap_value = overlap_value = TriggerOverlapEnums::TriggerOverlap_PreviousFrame;
    } else if (overlap == "PreviousLine") {
      overlap_value = overlap_value = TriggerOverlapEnums::TriggerOverlap_PreviousLine;
    } else {
      throw std::invalid_argument("Invalid argument: overlap = " + overlap + ".");
    }
    cam_ptr->TriggerOverlap.SetValue(overlap_value);
  }
  std::string get_trigger_overlap() const {
    auto overlap = cam_ptr->TriggerOverlap.GetValue();
    if (overlap == TriggerOverlapEnums::TriggerOverlap_Off) {
      return "Off";
    } else if (overlap == TriggerOverlapEnums::TriggerOverlap_ReadOut) {
      return "ReadOut";
    } else if (overlap == TriggerOverlapEnums::TriggerOverlap_PreviousFrame) {
      return "PreviousFrame";
    } else if (overlap == TriggerOverlapEnums::TriggerOverlap_PreviousLine) {
      return "PreviousLine";
    } else {
      return "Unknown";
    }
  }

  /**
   * Specifies the delay in microseconds (µs) to apply after the trigger
   * reception before activating it.
   */
  void set_trigger_delay(double delay) {
    cam_ptr->TriggetDelay.SetValue(delay);
  }
  double get_trigger_delay() const {
    return cam_ptr->TriggetDelay.GetValue();
  }

  /**
   * Sets the shutter mode of the device.
   *
   *  + Global - The shutter opens and closes at the same time for all pixels.
   *             All the pixels are exposed for the same length of time at the
   *             same time.
   *  + Rolling - The shutter opens and closes sequentially for groups (typically
   *              lines) of pixels. All the pixels are exposed for the same length
   *              of time but not at the same time.
   *  + GlobalReset - The shutter opens at the same time for all pixels but ends
   *                  in a sequential manner. The pixels are exposed for different
   *                  lengths of time.
   */
  void set_sensor_shutter_mode(const std::string &mode) {
    auto mode_value = SensorShutterModeEnums::SensorShutterMode_Global;
    if (mode == "Global") {
      mode_value = SensorShutterModeEnums::SensorShutterMode_Global;
    } else if (mode == "Rolling") {
      mode_value = SensorShutterModeEnums::SensorShutterMode_Rolling;
    } else if (mode == "GlobalReset") {
      mode_value = SensorShutterModeEnums::SensorShutterMode_GlobalReset;
    } else {
      throw std::invalid_argument("Invalid argument: mode = " + mode + ".");
    }
    cam_ptr->SensorShutterMode.SetValue(mode_value);
  }
  std::string get_sensor_shutter_mode() const {
    auto mode = cam_ptr->SensorShutterMode.GetValue();
    if (mode == SensorShutterModeEnums::SensorShutterMode_Global) {
      return "Global";
    } else if (mode == SensorShutterModeEnums::SensorShutterMode_Rolling) {
      return "Rolling";
    } else if (mode == SensorShutterModeEnums::SensorShutterMode_GlobalReset) {
      return "GlobalReset";
    } else {
      return "Unknown";
    }
  }

  /**************************** Analog Control ********************************/

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

  /*************************** Image Format Control ***************************/

  /***************************** Device Control *******************************/

  /************************** Transport Layer Control *************************/

  /**************************** Sequencer Control *****************************/

  /*********************** Color Transformation Control ***********************/

  /***************************** Chunk Data Control ***************************/

  /***************************** Digital IO Control ***************************/

  /************************ Counter And Timer Control *************************/

  /*************************** Logic Block Control ****************************/

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
  void enable_exposure_auto() {
    this->set_automatic_exposure_mode("Continuous");
  }
  void disable_exposure_auto() {
    this->set_automatic_exposure_mode("Off");
    this->set_exposure_mode("Timed");
  }
  void set_exposure_upperbound(double value) {
    CFloatPtr AutoExposureExposureTimeUpperLimit = node_map->GetNode("AutoExposureExposureTimeUpperLimit");
    AutoExposureExposureTimeUpperLimit->SetValue(value);
  }
  void enable_hardware_trigger(const std::string &source = "Line0", const std::string &selector = "FrameStart", const std::string &activation = "RisingEdge") {
    this->disable_trigger();
    this->set_trigger_source(source);
    this->set_trigger_mode("On");
    this->set_trigger_selector(selector);
    this->set_trigger_activation(activation);
  }
  void enable_software_trigger() {
    this->disable_trigger();
    this->set_trigger_source("Software");
    this->set_trigger_mode("On");
  }
  void disable_trigger() {
    this->set_trigger_mode("Off");
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
    filename << prefix << (sys_ts / (60 * 1000000)) << '/';
    mkdir(filename.str().c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    filename << std::setfill('0') << std::setw(8) << idx << '-';
    filename << sys_ts << '-' << img_ts << ".jpg";
    cv::imwrite(filename.str().c_str(), img);
  }
};
