#include <iostream>
#include <vector>
#include <sndfile.hh>
#include "bit_stream.h"

using namespace std;
constexpr size_t FRAMES_BUFFER_SIZE = 65536;

// Quantização uniforme simples
short quantize(short sample, unsigned bits) {
    unsigned step = 1u << (16 - bits);
    return (sample / step) * step;
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        cerr << "Usage: " << argv[0] << " <input.wav> <bits> <output.bin>\n";
        return 1;
    }

    string inFile  = argv[1];
    unsigned bits  = stoi(argv[2]);
    string outBin  = argv[3];

    if (bits > 16 || bits == 0) {
        cerr << "Error: bits must be between 1 and 15\n";
        return 1;
    }

    // Abrir ficheiro WAV
    SndfileHandle inFileHandle { inFile };
    if (inFileHandle.error()) {
        cerr << "Error: invalid input file\n";
        return 1;
    }

    // Abrir BitStream para escrita
    fstream fs(outBin, ios::out | ios::binary);
    if (!fs.is_open()) {
        cerr << "Error: cannot open output file\n";
        return 1;
    }

    BitStream bs(fs, /* rw_status = */ false);

    // Cabeçalho
    bs.write_n_bits(bits, 8);                          // número de bits de quantização
    bs.write_n_bits(inFileHandle.channels(), 8);       // número de canais
    bs.write_n_bits(inFileHandle.samplerate(), 32);    // taxa de amostragem
    bs.write_n_bits(inFileHandle.frames(), 32);        // número total de frames

    // Buffer para ler as amostras WAV
    vector<short> buffer(FRAMES_BUFFER_SIZE * inFileHandle.channels());
    size_t nFrames;

    // Loop de leitura e quantização
    while ((nFrames = inFileHandle.readf(buffer.data(), FRAMES_BUFFER_SIZE))) {
        buffer.resize(nFrames * inFileHandle.channels());
        for (auto &s : buffer) {
            short q = quantize(s, bits);
            uint64_t packed = static_cast<uint16_t>(q) >> (16 - bits);
            bs.write_n_bits(packed, bits);
        }
    }

    bs.close();
    fs.close();

    cout << "Encoded file with header written to: " << outBin << endl;
    return 0;
}
