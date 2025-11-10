#ifndef IMAGE_CODEC_H
#define IMAGE_CODEC_H

#include "golomb.h"
#include "bit_stream.h"
#include <string>
#include <vector>
#include <fstream>
#include <opencv2/opencv.hpp>

#pragma pack(push, 1)
struct CodecHeader {
    char magic[4] = {'G', 'I', 'C', 'L'};
    uint16_t version = 1;
    uint32_t width;
    uint32_t height;
    bool adaptive;
    uint16_t fixed_m;
};
#pragma pack(pop)

class ImageCodec {
public:
    ImageCodec(std::string in_file, std::string out_file, int m, bool adaptive);
    
    ImageCodec(std::string in_file, std::string out_file);

    void encode();
    void decode();

private:
    void write_codec_header(const CodecHeader& header, std::fstream& fs);
    CodecHeader read_codec_header(std::fstream& fs);
    int calculate_m(const std::vector<int>& residuals);

    int get_pixel(const cv::Mat& img, int r, int c);
    
    int predict(int A, int B, int C);

    std::string m_in_file;
    std::string m_out_file;
    
    int m_fixed_m;
    bool m_adaptive;

    static const int BLOCK_SIZE_Y = 64;
};

#endif