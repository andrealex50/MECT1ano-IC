#include "image_codec.h"
#include <iostream>
#include <stdexcept>
#include <numeric>
#include <cmath>
#include <algorithm>
#include <cstring>
#include <opencv2/imgproc.hpp>
#include <filesystem>
#include <iomanip>

ImageCodec::ImageCodec(std::string in_file, std::string out_file, int m, bool adaptive)
    : m_in_file(in_file), m_out_file(out_file), m_fixed_m(m), m_adaptive(adaptive) {
    if (!adaptive && m <= 0) {
        throw std::invalid_argument("Fixed 'm' must be > 0.");
    }
}

ImageCodec::ImageCodec(std::string in_file, std::string out_file)
    : m_in_file(in_file), m_out_file(out_file), m_fixed_m(1), m_adaptive(false) {
}

void ImageCodec::write_codec_header(const CodecHeader& header, std::fstream& fs) {
    fs.write(reinterpret_cast<const char*>(&header), sizeof(CodecHeader));
}

CodecHeader ImageCodec::read_codec_header(std::fstream& fs) {
    CodecHeader header;
    fs.read(reinterpret_cast<char*>(&header), sizeof(CodecHeader));
    if (fs.gcount() != sizeof(CodecHeader)) {
        throw std::runtime_error("Failed to read codec header.");
    }
    if (strncmp(header.magic, "GICL", 4) != 0) {
        throw std::runtime_error("Invalid file format must be  GICL.");
    }
    return header;
}

int ImageCodec::calculate_m(const std::vector<int>& residuals) {
    if (residuals.empty()) return 1;

    double sum_abs = 0.0;
    for (int res : residuals) {
        sum_abs += std::abs(static_cast<double>(res));
    }
    double avg = sum_abs / residuals.size();

    int m = static_cast<int>(std::round(avg * 0.693147));
    return std::max(1, m);
}

int ImageCodec::get_pixel(const cv::Mat& img, int r, int c) {
    if (r < 0 || c < 0) {
        return 0;
    }
    return (int)img.at<uint8_t>(r, c);
}

int ImageCodec::predict(int A, int B, int C) {
    if (C >= std::max(A, B)) {
        return std::min(A, B);
    } else if (C <= std::min(A, B)) {
        return std::max(A, B);
    } else {
        return A + B - C;
    }
}


void ImageCodec::encode() {
    std::cout << "Encoding " << m_in_file << " to " << m_out_file << "...\n";
    std::cout << "Mode: " << (m_adaptive ? "Adaptive 'm'" : "Fixed 'm' = " + std::to_string(m_fixed_m)) << "\n";

    cv::Mat img = cv::imread(m_in_file, cv::IMREAD_GRAYSCALE);
    if (!img.data) {
        throw std::runtime_error("Could not load image: " + m_in_file);
    }
    if (img.type() != CV_8U) {
        throw std::runtime_error("Only 8-bit grayscale images are supported.");
    }
    std::cout << "Input: " << img.cols << "x" << img.rows << ", 8-bit grayscale\n";

    std::fstream out_fs(m_out_file, std::ios::out | std::ios::binary);
    if (!out_fs) {
        throw std::runtime_error("Failed to create output file.");
    }

    CodecHeader codec_h;
    codec_h.width = img.cols;
    codec_h.height = img.rows;
    codec_h.adaptive = m_adaptive;
    codec_h.fixed_m = static_cast<uint16_t>(m_fixed_m);
    write_codec_header(codec_h, out_fs);

    int num_blocks = (img.rows + BLOCK_SIZE_Y - 1) / BLOCK_SIZE_Y;
    std::vector<std::vector<int>> blocks_residuals(num_blocks);

    for (int r = 0; r < img.rows; ++r) {
        int block_idx = r / BLOCK_SIZE_Y;
        for (int c = 0; c < img.cols; ++c) {
            int A = get_pixel(img, r, c - 1);
            int B = get_pixel(img, r - 1, c);
            int C = get_pixel(img, r - 1, c - 1);

            int P = predict(A, B, C);
            
            int X = (int)img.at<uint8_t>(r, c);
            int residual = X - P;

            blocks_residuals[block_idx].push_back(residual);
        }
    }

    BitStream bs(out_fs, STREAM_WRITE);
    int initial_m = m_adaptive ? 1 : m_fixed_m;
    Golomb golomb(initial_m, NegativeHandling::INTERLEAVING);

    for (const auto& block : blocks_residuals) {
        if (m_adaptive) {
            int m = calculate_m(block);
            bs.write_n_bits(static_cast<uint64_t>(m), 16);
            golomb.set_m(m);
        }

        for (int residual : block) {
            golomb.encode(residual, bs);
        }
    }

    bs.close();
    out_fs.close();
    std::cout << "Encoding complete.\n";

    try {
        uintmax_t in_file_size = std::filesystem::file_size(m_in_file);
        uintmax_t out_file_size = std::filesystem::file_size(m_out_file);

        if (out_file_size > 0) {
            double compression_rate = static_cast<double>(in_file_size) / out_file_size;
            
            std::cout << "\n--- Compression Stats ---\n";
            std::cout << "Original Size:   " << in_file_size << " bytes\n";
            std::cout << "Compressed Size: " << out_file_size << " bytes\n";
            std::cout << "Compression Rate: " << std::fixed << std::setprecision(2) << compression_rate << ":1\n";
        } else {
            std::cout << "\n--- Compression Stats ---\n";
            std::cout << "Original Size:   " << in_file_size << " bytes\n";
            std::cout << "Compressed Size: 0 bytes (Error?)\n";
        }
    } catch (std::filesystem::filesystem_error& e) {
        std::cerr << "Could not get file sizes for stats: " << e.what() << std::endl;
    }
}

void ImageCodec::decode() {
    std::cout << "Decoding " << m_in_file << " to " << m_out_file << "...\n";

    std::fstream in_fs(m_in_file, std::ios::in | std::ios::binary);
    if (!in_fs) {
        throw std::runtime_error("Failed to open input file.");
    }

    CodecHeader codec_h = read_codec_header(in_fs);
    std::cout << "Input: " << codec_h.width << "x" << codec_h.height << "\n";
    std::cout << "Mode: " << (codec_h.adaptive ? "Adaptive 'm'" : "Fixed 'm' = " + std::to_string(codec_h.fixed_m)) << "\n";

    cv::Mat img = cv::Mat(codec_h.height, codec_h.width, CV_8U);

    BitStream bs(in_fs, STREAM_READ);
    int initial_m = codec_h.adaptive ? 1 : codec_h.fixed_m;
    Golomb golomb(initial_m, NegativeHandling::INTERLEAVING);

    for (int r = 0; r < img.rows; ++r) {
        if (codec_h.adaptive && (r % BLOCK_SIZE_Y == 0)) {
            int m = static_cast<int>(bs.read_n_bits(16));
            if (m <= 0) m = 1;
            golomb.set_m(m);
        }

        for (int c = 0; c < img.cols; ++c) {
            int A = get_pixel(img, r, c - 1);
            int B = get_pixel(img, r - 1, c);
            int C = get_pixel(img, r - 1, c - 1);

            int P = predict(A, B, C);

            int residual = golomb.decode(bs);

            int X = residual + P;

            if (X < 0) X = 0;
            if (X > 255) X = 255;
            img.at<uint8_t>(r, c) = static_cast<uint8_t>(X);
        }
    }

    bs.close();
    in_fs.close();

    cv::Mat img_to_save;

    std::string ext = m_out_file.substr(m_out_file.find_last_of("."));
    std::cout << "Converting to BGR for .ppm output...\n";
    cv::cvtColor(img, img_to_save, cv::COLOR_GRAY2BGR);


    if (!cv::imwrite(m_out_file, img_to_save)) {
        throw std::runtime_error("Failed to save decoded image to: " + m_out_file);
    }
    std::cout << "Decoding complete. Saved to " << m_out_file << "\n";
}