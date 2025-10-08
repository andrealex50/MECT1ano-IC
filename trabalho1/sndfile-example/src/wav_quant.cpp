//------------------------------------------------------------------------------
//
// wav_quant: Uniform Scalar Quantization of WAV audio
//
// Usage:
//   wav_quant <input.wav> <output.wav> <nbits>
//
// Example:
//   wav_quant input.wav output.wav 4
//   -> output.wav will be 16-bit PCM, but only 16 levels used (4 bits effective)
//
//------------------------------------------------------------------------------
//
// Copyright 2025 University of Aveiro, Portugal, All Rights Reserved.
//
// These programs are supplied free of charge for research purposes only,
// and may not be sold or incorporated into any commercial product. There is
// ABSOLUTELY NO WARRANTY of any sort, nor any undertaking that they are
// fit for ANY PURPOSE WHATSOEVER. Use them at your own risk. If you do
// happen to find a bug, or have modifications to suggest, please report
// the same to Armando J. Pinho, ap@ua.pt. The copyright notice above
// and this statement of conditions must remain an integral part of each
// and every copy made of these files.
//
// Armando J. Pinho (ap@ua.pt)
// IEETA / DETI / University of Aveiro
//
#include <iostream>
#include <cmath>
#include <sndfile.hh>
#include <vector>
#include <string>

// Quantize one sample
inline short quantize(short sample, int nbits, int min_val, double step) {
    int levels = 1 << nbits;

    // Map sample to bin index
    int q = int((sample - min_val) / step);
    if (q < 0) q = 0;
    if (q >= levels) q = levels - 1;

    // Reconstruct value as bin center
    return short(min_val + q * step + step / 2.0);
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <input.wav> <output.wav> <nbits>\n";
        return 1;
    }

    std::string inFile = argv[1];
    std::string outFile = argv[2];
    int nbits = std::stoi(argv[3]);

    if (nbits <= 0 || nbits > 16) {
        std::cerr << "Error: nbits must be between 1 and 16.\n";
        return 1;
    }

    // Open input file
    SndfileHandle infile(inFile);
    if (infile.error()) {
        std::cerr << "Error: could not open input file.\n";
        return 1;
    }

    if ((infile.format() & SF_FORMAT_TYPEMASK) != SF_FORMAT_WAV) {
        std::cerr << "Error: input is not a WAV file.\n";
        return 1;
    }

    if ((infile.format() & SF_FORMAT_SUBMASK) != SF_FORMAT_PCM_16) {
        std::cerr << "Warning: only PCM_16 fully supported, continuing anyway.\n";
    }

    int channels   = infile.channels();
    int samplerate = infile.samplerate();
    int format     = infile.format();

    // Output file (same format but quantized samples)
    SndfileHandle outfile(outFile, SFM_WRITE, format, channels, samplerate);
    if (outfile.error()) {
        std::cerr << "Error: could not open output file.\n";
        return 1;
    }

    // PCM16 range
    int max_bits = 16;
    int max_val = (1 << (max_bits - 1)) - 1;  // +32767
    int min_val = -(1 << (max_bits - 1));     // -32768
    int levels = 1 << nbits;
    double step = double(max_val - min_val + 1) / levels;

    const size_t blocksize = 1024;
    std::vector<short> buffer(blocksize * channels);

    sf_count_t readcount;
    while ((readcount = infile.readf(buffer.data(), blocksize)) > 0) {
        for (int i = 0; i < readcount * channels; i++) {
            buffer[i] = quantize(buffer[i], nbits, min_val, step);
        }
        outfile.writef(buffer.data(), readcount);
    }

    std::cout << "Quantization complete: " << inFile
              << " -> " << outFile
              << " using " << nbits << " bits.\n";

    std::cout << "Output written to: " << outFile << std::endl;

    return 0;
}
