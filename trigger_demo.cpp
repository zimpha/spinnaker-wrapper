#include "camera_wrapper.hpp"
#include <chrono>
#include <thread>
#include <iostream>
#include <sstream>
#include <folly/ProducerConsumerQueue.h>
#include <folly/MPMCQueue.h>

folly::ProducerConsumerQueue<int> trigger_queue(100);
folly::ProducerConsumerQueue<image_t> image_queue(100);

int main() {
  camera_wrapper cam1("16276918"), cam2("16290191");
  cam1.enable_software_trigger();
  cam2.enable_software_trigger();
  cam1.disable_exposure_auto();
  cam2.disable_exposure_auto();
  cam1.set_exposure_time(1000);
  cam2.set_exposure_time(1000);
  std::thread trigger_thread([&]() {
    for (int i = 0; i < 100; ++i) {
      while (!trigger_queue.write(i)) {
        continue;
      }
      std::cout << trigger_queue.sizeGuess() << " not processed yet" << std::endl;
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
  });
  std::thread camera_thread([&]() {
    cam1.start();
    cam2.start();
    try {
      uint64_t last = 0;
      for (int i = 0, val; i < 100; ++i) {
        while (!trigger_queue.read(val)) {
          continue;
        }
        cam1.trigger_software_execute();
        cam2.trigger_software_execute();
        cv::Mat img1 = cam1.grab_next_image("gray"), dst1;
        cv::Mat img2 = cam2.grab_next_image("gray"), dst2;
        cv::resize(img1, dst1, cv::Size(0, 0), 0.5, 0.5);
        cv::resize(img2, dst2, cv::Size(0, 0), 0.5, 0.5);
        image_t data1(dst1, i, cam1.get_system_timestamp(), cam1.get_image_timestamp(), "cam1");
        image_t data2(dst2, i, cam2.get_system_timestamp(), cam2.get_image_timestamp(), "cam2");
        while (!image_queue.write(data1)) {
          continue;
        }
        while (!image_queue.write(data2)) {
          continue;
        }
        auto now = cam1.get_system_timestamp();
        std::cout << 1.0 / ((now - last) / 1000000.0) << " fps" << std::endl;
        last = now;
      }
    } catch (Spinnaker::Exception &e) {
      std::cout << "Error: " << e.what() << std::endl;
    }
    cam1.disable_trigger();
    cam2.disable_trigger();
    cam1.end();
    cam2.end();
  });
  std::thread image_writer([&]() {
    while (true) {
      image_t image;
      while (!image_queue.read(image)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        continue;
      }
      image.save();
      std::cout << "rest images in queue: " << image_queue.sizeGuess() << std::endl;
    }
  });
  trigger_thread.join();
  camera_thread.join();
  image_writer.join();
  return 0;
}
