#ifndef AUDIO_CODEC_H
#define AUDIO_CODEC_H

#include "golomb.h"
#include "bit_stream.h"
#include <string>
#include <vector>
#include <fstream>
#include <sndfile.h>

#pragma pack(push, 1)
struct CodecHeader {
    char magic[4] = {'G', 'A', 'C', 'L'}; 
    uint16_t version = 1;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint16_t bits_per_sample = 16;
    uint64_t total_samples;
    bool adaptive;
    uint16_t fixed_m;
};
#pragma pack(pop)

class AudioCodec {
public:
    AudioCodec(std::string in_file, std::string out_file, int m, bool adaptive);
    
    AudioCodec(std::string in_file, std::string out_file);

    void encode();
    void decode();

private:
    void write_codec_header(const CodecHeader& header, std::fstream& fs);
    CodecHeader read_codec_header(std::fstream& fs);
    int calculate_m(const std::vector<int>& residuals);

    std::string m_in_file;
    std::string m_out_file;
    
    int m_fixed_m;
    bool m_adaptive;

    static const int BLOCK_SIZE = 4096;
};

#endif