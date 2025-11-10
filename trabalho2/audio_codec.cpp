#include "audio_codec.h"
#include <iostream>
#include <stdexcept>
#include <numeric>
#include <cstring>
#include <cmath>
#include <algorithm>

// Constructor for encoding
AudioCodec::AudioCodec(std::string in_file, std::string out_file, int m, bool adaptive)
    : m_in_file(in_file), m_out_file(out_file), m_fixed_m(m), m_adaptive(adaptive) {
    if (!adaptive && m <= 0) {
        throw std::invalid_argument("Fixed 'm' must be > 0.");
    }
}

// Constructor for decoding
AudioCodec::AudioCodec(std::string in_file, std::string out_file)
    : m_in_file(in_file), m_out_file(out_file), m_fixed_m(1), m_adaptive(false) {
}

void AudioCodec::write_codec_header(const CodecHeader& header, std::fstream& fs) {
    fs.write(reinterpret_cast<const char*>(&header), sizeof(CodecHeader));
}

CodecHeader AudioCodec::read_codec_header(std::fstream& fs) {
    CodecHeader header;
    fs.read(reinterpret_cast<char*>(&header), sizeof(CodecHeader));
    if (fs.gcount() != sizeof(CodecHeader)) {
        throw std::runtime_error("Failed to read codec header.");
    }
    if (strncmp(header.magic, "GACL", 4) != 0) {
        throw std::runtime_error("Invalid file format (magic number mismatch).");
    }
    return header;
}

int AudioCodec::calculate_m(const std::vector<int>& residuals) {
    if (residuals.empty()) return 1;

    double sum_abs = 0.0;
    for (int res : residuals) {
        sum_abs += std::abs(static_cast<double>(res));
    }
    double avg = sum_abs / residuals.size();

    int m = static_cast<int>(std::round(avg * 0.693147));
    return std::max(1, m); // m must be at least 1
}

void AudioCodec::encode() {
    std::cout << "Encoding " << m_in_file << " to " << m_out_file << "...\n";
    std::cout << "Mode: " << (m_adaptive ? "Adaptive 'm'" : "Fixed 'm' = " + std::to_string(m_fixed_m)) << "\n";

    SF_INFO sf_info;
    memset(&sf_info, 0, sizeof(sf_info));
    
    SNDFILE* wav_in = sf_open(m_in_file.c_str(), SFM_READ, &sf_info);
    if (!wav_in) {
        throw std::runtime_error("libsndfile could not open input file: " + m_in_file);
    }

    if ((sf_info.format & SF_FORMAT_SUBMASK) != SF_FORMAT_PCM_16) {
        sf_close(wav_in);
        throw std::runtime_error("Only 16-bit PCM WAV files are supported.");
    }

    if (sf_info.channels > 2) {
        sf_close(wav_in);
        throw std::runtime_error("Only mono or stereo files are supported.");
    }

    std::cout << "Input: " << sf_info.channels << " channels, " 
              << sf_info.samplerate << " Hz, 16 bits\n";

    std::fstream out_fs(m_out_file, std::ios::out | std::ios::binary);
    if (!out_fs) {
        sf_close(wav_in);
        throw std::runtime_error("Failed to create output file.");
    }

    CodecHeader codec_h;
    codec_h.num_channels = sf_info.channels;
    codec_h.sample_rate = sf_info.samplerate;
    codec_h.total_samples = sf_info.frames;
    codec_h.adaptive = m_adaptive;
    codec_h.fixed_m = static_cast<uint16_t>(m_fixed_m);
    write_codec_header(codec_h, out_fs);

    BitStream bs(out_fs, STREAM_WRITE);
    int initial_m = m_adaptive ? 1 : m_fixed_m;
    Golomb golomb(initial_m, NegativeHandling::INTERLEAVING);

    std::vector<int16_t> sample_buffer(BLOCK_SIZE * sf_info.channels);
    std::vector<int> residual_buffer;
    residual_buffer.reserve(BLOCK_SIZE * sf_info.channels);

    int pred_l = 0, pred_r = 0;
    int l = 0, r = 0;
    int res_l = 0, res_r = 0;
    sf_count_t frames_read = 0;
    size_t total_samples_processed = 0;

    while ((frames_read = sf_readf_short(wav_in, sample_buffer.data(), BLOCK_SIZE)) > 0) {
        
        residual_buffer.clear();
        for (sf_count_t i = 0; i < frames_read; ++i) {
            l = sample_buffer[i * sf_info.channels];
            res_l = l - pred_l;
            residual_buffer.push_back(res_l);
            pred_l = l;

            if (sf_info.channels == 2) {
                r = sample_buffer[i * sf_info.channels + 1];
                pred_r = l;
                res_r = r - pred_r;
                residual_buffer.push_back(res_r);
            }
        }
        total_samples_processed += frames_read;


        if (m_adaptive) {
            int m = calculate_m(residual_buffer);
            bs.write_n_bits(static_cast<uint64_t>(m), 16);
            golomb.set_m(m);
        }

        for (int residual : residual_buffer) {
            golomb.encode(residual, bs);
        }
    }

    bs.close();
    out_fs.close();
    sf_close(wav_in);
    std::cout << "Encoding complete. Processed " << total_samples_processed << " samples per channel.\n";
}

void AudioCodec::decode() {
    std::cout << "Decoding " << m_in_file << " to " << m_out_file << "...\n";

    std::fstream in_fs(m_in_file, std::ios::in | std::ios::binary);
    if (!in_fs) {
        throw std::runtime_error("Failed to open input file.");
    }

    CodecHeader codec_h = read_codec_header(in_fs);
    std::cout << "Input: " << codec_h.num_channels << " channels, " 
              << codec_h.sample_rate << " Hz, " << codec_h.bits_per_sample << " bits\n";
    std::cout << "Mode: " << (codec_h.adaptive ? "Adaptive 'm'" : "Fixed 'm' = " + std::to_string(codec_h.fixed_m)) << "\n";
    std::cout << "Total samples: " << codec_h.total_samples << "\n";

    SF_INFO sf_info_out;
    memset(&sf_info_out, 0, sizeof(sf_info_out));
    sf_info_out.samplerate = codec_h.sample_rate;
    sf_info_out.channels = codec_h.num_channels;
    sf_info_out.frames = codec_h.total_samples;
    sf_info_out.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;

    SNDFILE* wav_out = sf_open(m_out_file.c_str(), SFM_WRITE, &sf_info_out);
    if (!wav_out) {
        in_fs.close();
        throw std::runtime_error("libsndfile could not create output file: " + m_out_file);
    }

    BitStream bs(in_fs, STREAM_READ);
    Golomb golomb(codec_h.fixed_m, NegativeHandling::INTERLEAVING);

    int pred_l = 0, pred_r = 0;
    int l = 0, r = 0;
    int res_l = 0, res_r = 0;
    uint64_t samples_to_decode = codec_h.total_samples;

    std::vector<int16_t> out_buffer;
    out_buffer.reserve(BLOCK_SIZE * codec_h.num_channels);

    while (samples_to_decode > 0) {
        if (codec_h.adaptive) {
            int m = static_cast<int>(bs.read_n_bits(16));
            if (m <= 0) m = 1;
            golomb.set_m(m);
        }

        size_t block_size = std::min((uint64_t)BLOCK_SIZE, samples_to_decode);
        out_buffer.clear();

        for (size_t i = 0; i < block_size; ++i) {
            res_l = golomb.decode(bs);
            l = res_l + pred_l;
            out_buffer.push_back(static_cast<int16_t>(l));
            pred_l = l;

            if (codec_h.num_channels == 2) {
                res_r = golomb.decode(bs);
                pred_r = l;
                r = res_r + pred_r;
                out_buffer.push_back(static_cast<int16_t>(r));
            }
        }
        
        sf_write_short(wav_out, out_buffer.data(), out_buffer.size());

        samples_to_decode -= block_size;
    }

    bs.close();
    in_fs.close();
    sf_close(wav_out);
    std::cout << "Decoding complete.\n";
}