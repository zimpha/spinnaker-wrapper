#include "camera_wrapper.hpp"
#include <chrono>
#include <thread>
#include <folly/ProducerConsumerQueue.h>
#include <folly/MPMCQueue.h>

folly::ProducerConsumerQueue<image_t> image_queue(100);

int main() {
  camera_wrapper cam("16290191");
  cam.print_device_info();
  cam.disable_trigger();
  cam.disable_frame_rate_auto();
  cam.disable_exposure_auto();
  cam.set_frame_rate(50);
  cam.set_exposure_time(1000);
  std::thread camera_thread([&]() {
    cam.start();
    try {
      uint64_t last = 0;
      for (int i = 0; i < 100; ++i) {
        cv::Mat img = cam.grab_next_image("gray"), dst;
        cv::resize(img, dst, cv::Size(0, 0), 0.5, 0.5);
        image_t data(dst, i, cam.get_system_timestamp(), cam.get_image_timestamp(), "cam1");
        while (!image_queue.write(data)) {
          continue;
        }
        auto now = cam.get_system_timestamp();
        std::cout << 1.0 / ((now - last) / 1000000.0) << " fps" << std::endl;
        last = now;
      }
    } catch (Spinnaker::Exception &e) {
      std::cout << "Error: " << e.what() << std::endl;
    }
    cam.end();
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
  camera_thread.join();
  image_writer.join();
  return 0;
}
