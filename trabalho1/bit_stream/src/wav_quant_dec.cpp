#include <iostream>
#include <vector>
#include <sndfile.hh>
#include "bit_stream.h"

using namespace std;
constexpr size_t FRAMES_BUFFER_SIZE = 65536;

// Reconstrução simples a partir do valor quantizado
short dequantize(uint64_t qValue, unsigned bits) {
    return static_cast<short>(qValue << (16 - bits));
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        cerr << "Usage: " << argv[0] << " <input.bin> <output.wav>\n";
        return 1;
    }

    string inBin   = argv[1];
    string outFile = argv[2];

    // Abrir o ficheiro binário para leitura
    fstream fs(inBin, ios::in | ios::binary);
    if (!fs.is_open()) {
        cerr << "Error: cannot open input file\n";
        return 1;
    }

    BitStream bs(fs, /* rw_status = */ true); // true = Read mode

    //Ler o cabeçalho
    unsigned bits = bs.read_n_bits(8);
    unsigned channels = bs.read_n_bits(8);
    unsigned samplerate = bs.read_n_bits(32);
    unsigned totalFrames = bs.read_n_bits(32);

    // Preparar o ficheiro WAV de saída
    SF_INFO sfInfo;
    sfInfo.samplerate = samplerate;
    sfInfo.channels = channels;
    sfInfo.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;

    SndfileHandle outHandle(outFile, SFM_WRITE, sfInfo.format,
                            sfInfo.channels, sfInfo.samplerate);

    vector<short> buffer(FRAMES_BUFFER_SIZE * channels);
    size_t framesProcessed = 0;

    // Loop de leitura e reconstrução
    while (framesProcessed < totalFrames) {
        size_t framesToRead = min(FRAMES_BUFFER_SIZE, totalFrames - framesProcessed);
        buffer.resize(framesToRead * channels);

        for (auto &s : buffer) {
            uint64_t qValue = bs.read_n_bits(bits);
            s = dequantize(qValue, bits);
        }

        outHandle.writef(buffer.data(), framesToRead);
        framesProcessed += framesToRead;
    }

    bs.close();
    fs.close();

    cout << "Decoded WAV written to: " << outFile << endl;
    return 0;
}
