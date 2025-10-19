#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <sndfile.h>
#include "bit_stream.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

class DCTAudioEncoder {
private:
    static constexpr int BLOCK_SIZE = 1024;  // DCT block size
    
    int m_sample_rate;
    int m_num_samples;
    double m_quantization_step;
    
    // DCT-II transform
    void dct(const std::vector<double>& input, std::vector<double>& output) {
        int N = input.size();
        output.resize(N);
        
        for (int k = 0; k < N; k++) {
            double sum = 0.0;
            for (int n = 0; n < N; n++) {
                sum += input[n] * cos(M_PI * k * (2 * n + 1) / (2.0 * N));
            }
            // α(k) = 1/√2 for k=0, α(k) = 1 otherwise
            double alpha = (k == 0) ? (1.0 / sqrt(2.0)) : 1.0;
            output[k] = sqrt(2.0 / N) * alpha * sum;
        }
    }
    
    // quantize coefficients
    std::vector<int> quantize_coefficients(const std::vector<double>& coeffs) {
        std::vector<int> quantized(coeffs.size());
        
        for (size_t i = 0; i < coeffs.size(); i++) {
            // frequency-weighted quantization step
            double freq_weight = 1.0 + (i * 2.0) / coeffs.size();
            double q_step = m_quantization_step * freq_weight;
       
            quantized[i] = static_cast<int>(round(coeffs[i] / q_step));
        }
        
        return quantized;
    }
    
    // encode coefficients to bit stream
    void encode_coefficients(BitStream& bs, const std::vector<int>& coeffs) {
        for (int coeff : coeffs) {
            // sign-magnitude representation
            int abs_val = abs(coeff);
            int sign = (coeff < 0) ? 1 : 0;
            
            // determine number of bits needed
            int bits_needed = 0;
            int temp = abs_val;
            while (temp > 0) {
                bits_needed++;
                temp >>= 1;
            }
            
            // limit to max 15 bits AND clamp the value
            if (bits_needed > 15) {
                bits_needed = 15;
                abs_val = (1 << 15) - 1; 
            }
       
            bs.write_n_bits(bits_needed, 4);
            
            if (bits_needed > 0) {
                bs.write_bit(sign);
                bs.write_n_bits(abs_val, bits_needed);
            }
        }
    }

public:
    DCTAudioEncoder(double quality = 0.5) {
        m_quantization_step = 1.0 * pow(10.0, -quality * 2.0);  
    }
    
    bool encode(const std::string& input_file, const std::string& output_file) {
        
        SF_INFO sf_info;
        sf_info.format = 0;
        SNDFILE* snd_file = sf_open(input_file.c_str(), SFM_READ, &sf_info);
        
        if (!snd_file) {
            std::cerr << "Error: Cannot open input file: " << input_file << std::endl;
            return false;
        }
        
        if (sf_info.channels != 1) {
            std::cerr << "Error: Only mono audio is supported" << std::endl;
            sf_close(snd_file);
            return false;
        }
        
        m_sample_rate = sf_info.samplerate;
        m_num_samples = sf_info.frames;
        
        std::cout << "Input: " << input_file << std::endl;
        std::cout << "Sample rate: " << m_sample_rate << " Hz" << std::endl;
        std::cout << "Samples: " << m_num_samples << std::endl;
        std::cout << "Duration: " << (double)m_num_samples / m_sample_rate << " seconds" << std::endl;
        
        // read samples
        std::vector<double> samples(m_num_samples);
        sf_read_double(snd_file, samples.data(), m_num_samples);
        sf_close(snd_file);
        
        // open output bit stream
        std::fstream fs(output_file, std::ios::binary | std::ios::out);
        if (!fs.is_open()) {
            std::cerr << "Error: Cannot create output file: " << output_file << std::endl;
            return false;
        }
        
        BitStream bs(fs, STREAM_WRITE);
     

        bs.write_n_bits(m_sample_rate, 32);
        bs.write_n_bits(m_num_samples, 32);
        bs.write_n_bits(BLOCK_SIZE, 16);
        
        // write quantization step
        uint32_t q_step_fixed = static_cast<uint32_t>(m_quantization_step * 1000.0);
        bs.write_n_bits(q_step_fixed, 32);
        
        // process blocks
        int num_blocks = (m_num_samples + BLOCK_SIZE - 1) / BLOCK_SIZE;
        std::cout << "Processing " << num_blocks << " blocks..." << std::endl;
        
        for (int block = 0; block < num_blocks; block++) {
            int start = block * BLOCK_SIZE;
            int end = std::min(start + BLOCK_SIZE, m_num_samples);
            int actual_size = end - start;
            
            // prepare block
            std::vector<double> block_data(BLOCK_SIZE, 0.0);
            for (int i = 0; i < actual_size; i++) {
                block_data[i] = samples[start + i];
            }
            
            // apply DCT
            std::vector<double> dct_coeffs;
            dct(block_data, dct_coeffs);
            
            // quantize
            std::vector<int> quantized = quantize_coefficients(dct_coeffs);
            
            // encode
            encode_coefficients(bs, quantized);
            
            if ((block + 1) % 100 == 0) {
                std::cout << "  Processed " << (block + 1) << "/" << num_blocks << " blocks" << std::endl;
            }
        }
        
        bs.close();
        fs.close();
        
        
        std::ifstream::pos_type output_size = std::ifstream(output_file, std::ios::binary | std::ios::ate).tellg();
        size_t input_size = m_num_samples * sizeof(int16_t);
        double ratio = (double)input_size / output_size;
        
        std::cout << "\nEncoding complete!" << std::endl;
        std::cout << "Output: " << output_file << std::endl;
        std::cout << "Compressed size: " << output_size << " bytes" << std::endl;
        std::cout << "Compression ratio: " << ratio << ":1" << std::endl;
        
        return true;
    }
};

int main(int argc, char* argv[]) {
    if (argc < 3 || argc > 4) {
        std::cerr << "Usage: " << argv[0] << " <input.wav> <output.dct> [quality]" << std::endl;
        std::cerr << "  quality: 0.0 (low) to 1.0 (high), default: 0.5" << std::endl;
        return 1;
    }
    
    std::string input_file = argv[1];
    std::string output_file = argv[2];
    double quality = (argc == 4) ? std::stod(argv[3]) : 0.5;
    
    if (quality < 0.0 || quality > 1.0) {
        std::cerr << "Error: Quality must be between 0.0 and 1.0" << std::endl;
        return 1;
    }
    
    DCTAudioEncoder encoder(quality);
    if (!encoder.encode(input_file, output_file)) {
        return 1;
    }
    
    return 0;
}