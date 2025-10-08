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
    if (argc < 5) {
        cerr << "Usage: " << argv[0] << " <input.bin> <bits> <output.wav> <template.wav>\n";
        return 1;
    }

    string inBin   = argv[1];
    unsigned bits  = stoi(argv[2]);
    string outFile = argv[3];
    string templateFile = argv[4];

    if (bits > 16 || bits == 0) {
        cerr << "Error: bits must be between 1 and 15\n";
        return 1;
    }

    // Usar ficheiro WAV original como template para metadata
    SndfileHandle templateHandle { templateFile };
    if (templateHandle.error()) {
        cerr << "Error: invalid template file\n";
        return 1;
    }

    SF_INFO sfInfo;
    sfInfo.samplerate = templateHandle.samplerate();
    sfInfo.channels   = templateHandle.channels();
    sfInfo.format     = templateHandle.format();

    // Abrir saída WAV
    SndfileHandle outHandle(outFile, SFM_WRITE, sfInfo.format,
                            sfInfo.channels, sfInfo.samplerate);

    // Abrir ficheiro binário para leitura
    fstream fs(inBin, ios::in | ios::binary);
    if (!fs.is_open()) {
        cerr << "Error: cannot open input file\n";
        return 1;
    }

    BitStream bs(fs, /* rw_status = */ true); // true = Read mode

    vector<short> buffer(FRAMES_BUFFER_SIZE * sfInfo.channels);
    size_t totalFrames = templateHandle.frames();
    size_t framesProcessed = 0;

    while (framesProcessed < totalFrames) {
        size_t framesToRead = min(FRAMES_BUFFER_SIZE, totalFrames - framesProcessed);
        buffer.resize(framesToRead * sfInfo.channels);

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