#include "camera_wrapper.hpp"
#include <chrono>
#include <thread>
#include <iostream>
#include <sstream>
#include <csignal>
#include <folly/ProducerConsumerQueue.h>
#include <folly/MPMCQueue.h>

folly::ProducerConsumerQueue<int> trigger_queue(100);
folly::ProducerConsumerQueue<image_t> cam1_image_queue(100);
folly::ProducerConsumerQueue<image_t> cam2_image_queue(100);

volatile sig_atomic_t flag = 0;

void signal_handler(int sigal_number) {
  std::cout << sigal_number << std::endl;
  if (sigal_number == 2) flag = 1;
}

int main() {
  signal(SIGINT, signal_handler);
  camera_wrapper cam2("16276918"), cam1("16290191");
  cam1.enable_software_trigger();
  cam2.enable_software_trigger();
  cam1.disable_exposure_auto();
  cam2.disable_exposure_auto();
  cam1.set_exposure_time(3000);
  cam2.set_exposure_time(3000);
  std::thread trigger_thread([&]() {
    for (int i = 0; ; ++i) {
      while (!trigger_queue.write(i)) {
        continue;
      }
      std::cout << trigger_queue.sizeGuess() << " not processed yet" << std::endl;
      std::this_thread::sleep_for(std::chrono::milliseconds(25));
      if (flag) break;
    }
  });
  std::thread camera_thread([&]() {
    cam1.start();
    cam2.start();
    try {
      uint64_t last = 0, index = 0;
      while (true) {
        int val;
        while (!trigger_queue.read(val) && !flag) {
          continue;
        }
        cam1.trigger_software_execute();
        cam2.trigger_software_execute();
        cv::Mat img1 = cam1.grab_next_image("gray"), dst1;
        cv::Mat img2 = cam2.grab_next_image("gray"), dst2;
        cv::resize(img1, dst1, cv::Size(0, 0), 0.5, 0.5);
        cv::resize(img2, dst2, cv::Size(0, 0), 0.5, 0.5);
        index += 1;
        image_t data1(dst1, index, cam1.get_system_timestamp(), cam1.get_image_timestamp(), "imgs/cam1");
        image_t data2(dst2, index, cam2.get_system_timestamp(), cam2.get_image_timestamp(), "imgs/cam2");
        while (!cam1_image_queue.write(data1)) {
          continue;
        }
        while (!cam2_image_queue.write(data2)) {
          continue;
        }
        auto now = cam1.get_system_timestamp();
        std::cout << 1.0 / ((now - last) / 1000000.0) << " fps" << std::endl;
        last = now;
        if (flag) break;
      }
    } catch (Spinnaker::Exception &e) {
      std::cout << "Error: " << e.what() << std::endl;
    }
    cam1.disable_trigger();
    cam2.disable_trigger();
    cam1.end();
    cam2.end();
  });
  std::thread cam1_image_writer([&]() {
    while (true) {
      image_t image;
      while (!cam1_image_queue.read(image) && !flag) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        continue;
      }
      if (flag) break;
      image.save();
      std::cout << "rest images in cam1 queue: " << cam1_image_queue.sizeGuess() << std::endl;
    }
  });
  std::thread cam2_image_writer([&]() {
    while (true) {
      image_t image;
      while (!cam2_image_queue.read(image) && !flag) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        continue;
      }
      if (flag) break;
      image.save();
      std::cout << "rest images in cam2 queue: " << cam2_image_queue.sizeGuess() << std::endl;
    }
  });
  trigger_thread.join();
  camera_thread.join();
  cam1_image_writer.join();
  cam2_image_writer.join();
  return 0;
}
