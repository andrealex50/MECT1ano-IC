#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <iostream>
#include <cmath>

cv::Mat createNegative(const cv::Mat& src) {
    cv::Mat dst(src.rows, src.cols, src.type());
    
    for (int y = 0; y < src.rows; y++) {
        for (int x = 0; x < src.cols; x++) {
            if (src.channels() == 3) {
                cv::Vec3b pixel = src.at<cv::Vec3b>(y, x);
                dst.at<cv::Vec3b>(y, x) = cv::Vec3b(255 - pixel[0], 255 - pixel[1], 255 - pixel[2]);
            } else {
                uchar pixel = src.at<uchar>(y, x);
                dst.at<uchar>(y, x) = 255 - pixel;
            }
        }
    }
    return dst;
}

cv::Mat mirrorHorizontal(const cv::Mat& src) {
    cv::Mat dst(src.rows, src.cols, src.type());
    
    for (int y = 0; y < src.rows; y++) {
        for (int x = 0; x < src.cols; x++) {
            if (src.channels() == 3) {
                dst.at<cv::Vec3b>(y, x) = src.at<cv::Vec3b>(y, src.cols - 1 - x);
            } else {
                dst.at<uchar>(y, x) = src.at<uchar>(y, src.cols - 1 - x);
            }
        }
    }
    return dst;
}

cv::Mat mirrorVertical(const cv::Mat& src) {
    cv::Mat dst(src.rows, src.cols, src.type());
    
    for (int y = 0; y < src.rows; y++) {
        for (int x = 0; x < src.cols; x++) {
            if (src.channels() == 3) {
                dst.at<cv::Vec3b>(y, x) = src.at<cv::Vec3b>(src.rows - 1 - y, x);
            } else {
                dst.at<uchar>(y, x) = src.at<uchar>(src.rows - 1 - y, x);
            }
        }
    }
    return dst;
}

cv::Mat rotate(const cv::Mat& src, int angle) {
    cv::Mat dst;
    
    if (angle == 90) {
        // 90 degrees clockwise
        dst = cv::Mat(src.cols, src.rows, src.type());
        for (int y = 0; y < src.rows; y++) {
            for (int x = 0; x < src.cols; x++) {
                int new_y = x;
                int new_x = src.rows - 1 - y;
                if (src.channels() == 3) {
                    dst.at<cv::Vec3b>(new_y, new_x) = src.at<cv::Vec3b>(y, x);
                } else {
                    dst.at<uchar>(new_y, new_x) = src.at<uchar>(y, x);
                }
            }
        }
    } else if (angle == 180) {
        dst = cv::Mat(src.rows, src.cols, src.type());
        for (int y = 0; y < src.rows; y++) {
            for (int x = 0; x < src.cols; x++) {
                int new_y = src.rows - 1 - y;
                int new_x = src.cols - 1 - x;
                if (src.channels() == 3) {
                    dst.at<cv::Vec3b>(new_y, new_x) = src.at<cv::Vec3b>(y, x);
                } else {
                    dst.at<uchar>(new_y, new_x) = src.at<uchar>(y, x);
                }
            }
        }
    } else if (angle == 270) {
        // 270 degrees
        dst = cv::Mat(src.cols, src.rows, src.type());
        for (int y = 0; y < src.rows; y++) {
            for (int x = 0; x < src.cols; x++) {
                int new_y = src.cols - 1 - x;
                int new_x = y;
                if (src.channels() == 3) {
                    dst.at<cv::Vec3b>(new_y, new_x) = src.at<cv::Vec3b>(y, x);
                } else {
                    dst.at<uchar>(new_y, new_x) = src.at<uchar>(y, x);
                }
            }
        }
    } else {
        dst = src.clone();
    }
    
    return dst;
}

cv::Mat adjustBrightness(const cv::Mat& src, int delta) {
    cv::Mat dst(src.rows, src.cols, src.type());
    
    for (int y = 0; y < src.rows; y++) {
        for (int x = 0; x < src.cols; x++) {
            if (src.channels() == 3) {
                cv::Vec3b pixel = src.at<cv::Vec3b>(y, x);
                for (int c = 0; c < 3; c++) {
                    int new_val = pixel[c] + delta;
                    if (new_val < 0) new_val = 0;
                    if (new_val > 255) new_val = 255;
                    pixel[c] = static_cast<uchar>(new_val);
                }
                dst.at<cv::Vec3b>(y, x) = pixel;
            } else {
                int pixel = src.at<uchar>(y, x);
                int new_val = pixel + delta;
                if (new_val < 0) new_val = 0;
                if (new_val > 255) new_val = 255;
                dst.at<uchar>(y, x) = static_cast<uchar>(new_val);
            }
        }
    }
    return dst;
}

void printUsage(const char* prog_name) {
    std::cout << "Usage: " << prog_name << " <input_image> <output_image> <operation> [params]\n";
    std::cout << "\nOperations:\n";
    std::cout << "  negative              - Create negative of image\n";
    std::cout << "  mirror_h              - Mirror horizontally\n";
    std::cout << "  mirror_v              - Mirror vertically\n";
    std::cout << "  rotate <angle>        - Rotate by angle (90, 180, or 270)\n";
    std::cout << "  brightness <delta>    - Adjust brightness (positive=lighter, negative=darker)\n";
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        printUsage(argv[0]);
        return -1;
    }

    const char* input_filename = argv[1];
    const char* output_filename = argv[2];
    std::string operation = argv[3];

    cv::Mat src = cv::imread(input_filename, cv::IMREAD_COLOR);
    if (src.empty()) {
        std::cout << "Error: Could not load image '" << input_filename << "'\n";
        return -1;
    }

    std::cout << "Image loaded: " << src.cols << "x" << src.rows << ", " << src.channels() << " channels\n";

    cv::Mat result;

    if (operation == "negative") {
        std::cout << "Creating negative...\n";
        result = createNegative(src);
    } 
    else if (operation == "mirror_h") {
        std::cout << "Mirroring horizontally...\n";
        result = mirrorHorizontal(src);
    } 
    else if (operation == "mirror_v") {
        std::cout << "Mirroring vertically...\n";
        result = mirrorVertical(src);
    } 
    else if (operation == "rotate") {
        if (argc < 5) {
            std::cout << "Error: rotate requires angle parameter (90, 180, or 270)\n";
            return -1;
        }
        int angle = std::atoi(argv[4]);
        if (angle != 90 && angle != 180 && angle != 270) {
            std::cout << "Error: angle must be 90, 180, or 270\n";
            return -1;
        }
        std::cout << "Rotating by " << angle << " degrees...\n";
        result = rotate(src, angle);
    } 
    else if (operation == "brightness") {
        if (argc < 5) {
            std::cout << "Error: brightness requires delta parameter\n";
            return -1;
        }
        int delta = std::atoi(argv[4]);
        std::cout << "Adjusting brightness by " << delta << "...\n";
        result = adjustBrightness(src, delta);
    } 
    else {
        std::cout << "Error: Unknown operation '" << operation << "'\n";
        printUsage(argv[0]);
        return -1;
    }

    if (cv::imwrite(output_filename, result)) {
        std::cout << "Result saved to '" << output_filename << "'\n";
    } else {
        std::cout << "Error: Could not save image to '" << output_filename << "'\n";
        return -1;
    }

    return 0;
}