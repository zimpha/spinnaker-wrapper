#include "camera_wrapper.hpp"
#include <chrono>

int main() {
    camera_wrapper cam("16276918");
    cam.start();
    auto tic = std::chrono::high_resolution_clock::now();
    cam.set_frame_rate(100);
    cam.set_exposure_time(1000);
    double total = 0;
    for (int i = 0; i < 1000; ++i) {
        cv::Mat img = cam.grab_next_image("gray"), dst;
        cv::resize(img, dst, cv::Size(0, 0), 0.5, 0.5);
        auto tac = std::chrono::high_resolution_clock::now();
        std::cout << std::chrono::duration_cast<std::chrono::microseconds>(tac - tic).count() / 1000000.0 << ' ' << cam.get_frame_rate() << ' ' << cam.get_exposure_time() << std::endl;
        total += std::chrono::duration_cast<std::chrono::microseconds>(tac - tic).count() / 1000000.0;
        tic = tac;
        cv::imshow("video", dst);
        cv::waitKey(1);
    }
    std::cout << 1000 / total << " fps" << std::endl;
    cam.end();
    return 0;
}
