#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <iostream>

int main(int argc, char *argv[]) {
    if (argc != 4) {
        std::cout << "Usage: " << argv[0] << " <input_image> <output_image> <channel_number>\n";
        std::cout << "Channel numbers: 0=Blue, 1=Green, 2=Red (for BGR images)\n";
        return -1;
    }

    const char *input_filename = argv[1];
    const char *output_filename = argv[2];
    int channel = std::atoi(argv[3]);

    if (channel < 0 || channel > 2) {
        std::cout << "Error: Channel number must be 0, 1, or 2\n";
        std::cout << "0=Blue, 1=Green, 2=Red\n";
        return -1;
    }

    // load image
    cv::Mat src = cv::imread(input_filename, cv::IMREAD_COLOR);
    if (src.empty()) {
        std::cout << "Error: Could not load image '" << input_filename << "'\n";
        return -1;
    }

    std::cout << "Image loaded successfully\n";
    std::cout << "Width: " << src.cols << ", Height: " << src.rows << ", Channels: " << src.channels() << "\n";

    // create output single-channel image
    cv::Mat dst(src.rows, src.cols, CV_8UC1);

    std::cout << "Extracting channel " << channel << "...\n";

    // extract channel pixel by pixel
    for (int y = 0; y < src.rows; y++) {
        for (int x = 0; x < src.cols; x++) {
            cv::Vec3b pixel = src.at<cv::Vec3b>(y, x);
            dst.at<uchar>(y, x) = pixel[channel];
        }
    }

    if (cv::imwrite(output_filename, dst)) {
        std::cout << "Channel extracted successfully and saved to '" << output_filename << "'\n";
    } else {
        std::cout << "Error: Could not save image to '" << output_filename << "'\n";
        return -1;
    }

    return 0;
}