#include "camera_wrapper.hpp"
#include <csignal>

volatile sig_atomic_t flag = 0;

void signal_handler(int sigal_number) {
  std::cout << sigal_number << std::endl;
  if (sigal_number == 2) flag = 1;
}

int main() {
  flag = 0;
  signal(SIGINT, signal_handler);
  camera_wrapper cam1("16290191"), cam2("16276918");
  cam1.print_device_info();
  cam2.print_device_info();
  cam1.disable_trigger();
  cam2.disable_trigger();
  cam1.disable_frame_rate_auto();
  cam2.disable_frame_rate_auto();
  cam1.disable_exposure_auto();
  cam2.disable_exposure_auto();
  cam1.set_frame_rate(20);
  cam2.set_frame_rate(20);
  cam1.set_exposure_time(5000);
  cam2.set_exposure_time(5000);
  cam1.start();
  cam2.start();
  try {
    while (true) {
      cv::Mat img1 = cam1.grab_next_image("bgr"), dst1;
      cv::Mat img2 = cam2.grab_next_image("bgr"), dst2;
      cv::resize(img1, dst1, cv::Size(0, 0), 0.5, 0.5);
      cv::resize(img2, dst2, cv::Size(0, 0), 0.5, 0.5);
      std::vector<cv::Mat> channels1, channels2;
      cv::split(dst1, channels1);
      cv::split(dst2, channels2);
      std::vector<cv::Mat> channels3 = {channels1[0], channels1[1], channels2[2]};
      cv::Mat merged;
      cv::merge(channels3, merged);
      cv::imshow("cam1", dst1);
      cv::imshow("cam2", dst2);
      cv::imshow("merged", merged);
      cv::waitKey(1);
      if (flag) break;
    }
  } catch (Spinnaker::Exception &e) {
    std::cout << "Error: " << e.what() << std::endl;
  }
  cam1.end();
  cam2.end();
  return 0;
}
