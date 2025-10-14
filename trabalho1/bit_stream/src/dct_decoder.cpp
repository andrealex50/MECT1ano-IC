//-------------------------------------------------------------------------------------------
//
// DCT-based Lossy Audio Decoder
// Decodes audio compressed with DCT-based encoder
//
// Usage: ./dct_decoder input.dct output.wav
//
//-------------------------------------------------------------------------------------------

#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <sndfile.h>
#include "bit_stream.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

class DCTAudioDecoder {
private:
    
    int m_sample_rate;
    int m_num_samples;
    int m_block_size;
    double m_quantization_step;
    
    // DCT-III (inverse of DCT-II)
    void idct(const std::vector<double>& input, std::vector<double>& output) {
        int N = input.size();
        output.resize(N);
        
        for (int n = 0; n < N; n++) {
            double sum = 0.0;
            for (int k = 0; k < N; k++) {
                double coeff = input[k];
                if (k == 0) coeff *= 1.0 / sqrt(2.0);
                sum += coeff * cos(M_PI * k * (n + 0.5) / N);
            }
            output[n] = sum * sqrt(2.0 / N);
        }
    }
    
    // Dequantize coefficients
    std::vector<double> dequantize_coefficients(const std::vector<int>& quantized) {
        std::vector<double> coeffs(quantized.size());
        
        for (size_t i = 0; i < quantized.size(); i++) {
            double freq_weight = 1.0 + (i * 2.0) / quantized.size();
            double q_step = m_quantization_step * freq_weight;
            
            coeffs[i] = quantized[i] * q_step;
        }
        
        return coeffs;
    }
    
    // Decode coefficients from bit stream
    std::vector<int> decode_coefficients(BitStream& bs) { 
        std::vector<int> coeffs(m_block_size);  
        
        for (int i = 0; i < m_block_size; i++) {  
            int bits_needed = bs.read_n_bits(4);
            
            if (bits_needed == 0) {
                coeffs[i] = 0;
            } else {
                // sign bit
                int sign = bs.read_bit();
                
                // magnitude
                int abs_val = bs.read_n_bits(bits_needed);
                
                coeffs[i] = sign ? -abs_val : abs_val;
            }
        }
        
        return coeffs;
    }

public:
    bool decode(const std::string& input_file, const std::string& output_file) {
        std::fstream fs(input_file, std::ios::binary | std::ios::in);
        if (!fs.is_open()) {
            std::cerr << "Error: Cannot open input file: " << input_file << std::endl;
            return false;
        }
        
        BitStream bs(fs, STREAM_READ);
        
        m_sample_rate = bs.read_n_bits(32);
        m_num_samples = bs.read_n_bits(32);
        m_block_size = bs.read_n_bits(16);
        
        uint32_t q_step_fixed = bs.read_n_bits(32);
        m_quantization_step = q_step_fixed / 1000.0;
        
        std::cout << "Decoding audio file..." << std::endl;
        std::cout << "  Input: " << input_file << std::endl;
        std::cout << "  Sample rate: " << m_sample_rate << " Hz" << std::endl;
        std::cout << "  Samples: " << m_num_samples << std::endl;
        std::cout << "  Block size: " << m_block_size << std::endl;
        std::cout << "  Duration: " << (double)m_num_samples / m_sample_rate << " seconds" << std::endl;
        std::cout << "  Quantization step: " << m_quantization_step << std::endl;
        

        std::vector<double> samples(m_num_samples);
        
        // process blocks
        int num_blocks = (m_num_samples + m_block_size - 1) / m_block_size;
        std::cout << "\nProcessing " << num_blocks << " blocks..." << std::endl;
        
        for (int block = 0; block < num_blocks; block++) {
            int start = block * m_block_size;
            int end = std::min(start + m_block_size, m_num_samples);
            int actual_size = end - start;
            
            // decode coefficients
            std::vector<int> quantized = decode_coefficients(bs);  // Remove m_block_size parameter
            
            // dequantize
            std::vector<double> dct_coeffs = dequantize_coefficients(quantized);
            
            // apply inverse DCT
            std::vector<double> block_data;
            idct(dct_coeffs, block_data);
            
            // copy to output 
            for (int i = 0; i < actual_size; i++) {
                samples[start + i] = block_data[i];
            }
            
            if ((block + 1) % 100 == 0 || block == num_blocks - 1) {
                std::cout << "  Progress: " << (block + 1) << "/" << num_blocks 
                          << " blocks (" << (100 * (block + 1) / num_blocks) << "%)" << std::endl;
            }
        }
        
        bs.close();
        fs.close();

        
        SF_INFO sf_info;
        sf_info.samplerate = m_sample_rate;
        sf_info.channels = 1;
        sf_info.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
        
        SNDFILE* snd_file = sf_open(output_file.c_str(), SFM_WRITE, &sf_info);
        if (!snd_file) {
            std::cerr << "Error: Cannot create output file: " << output_file << std::endl;
            return false;
        }
        
        sf_write_double(snd_file, samples.data(), m_num_samples);
        sf_close(snd_file);
        
        std::cout << "\nDecoding complete!" << std::endl;
        std::cout << "  Output: " << output_file << std::endl;
        
        return true;
    }
};

int main(int argc, char* argv[]) {
    std::cout << "DCT Audio Decoder" << std::endl;
    std::cout << "=================" << std::endl << std::endl;
    
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <input.dct> <output.wav>" << std::endl;
        std::cerr << "\nDecodes a compressed DCT audio file back to WAV format." << std::endl;
        return 1;
    }
    
    std::string input_file = argv[1];
    std::string output_file = argv[2];
    
    DCTAudioDecoder decoder;
    if (!decoder.decode(input_file, output_file)) {
        return 1;
    }
    
    return 0;
}