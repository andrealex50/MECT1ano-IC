#include <iostream>
#include <vector>
#include <string>
#include <sndfile.hh>
#include <cmath>
#include <numeric>

using namespace std;
constexpr size_t FRAMES_BUFFER_SIZE = 65536;

int main(int argc, char *argv[]) {
    if (argc < 3) {
        cerr << "Usage: " << argv[0] << " <original.wav> <reconstructed.wav>\n";
        return 1;
    }

    string originalFile = argv[1];
    string reconstructedFile = argv[2];

    SndfileHandle origHandle(originalFile);
    if (origHandle.error()) {
        cerr << "Error: could not open original file " << originalFile << endl;
        return 1;
    }

    SndfileHandle reconHandle(reconstructedFile);
    if (reconHandle.error()) {
        cerr << "Error: could not open reconstructed file " << reconstructedFile << endl;
        return 1;
    }

    if (origHandle.frames() != reconHandle.frames() || 
        origHandle.channels() != reconHandle.channels()) {
        cerr << "Error: Files do not have the same number of frames or channels.\n";
        cerr << "Original: " << origHandle.frames() << " frames, " << origHandle.channels() << " channels\n";
        cerr << "Reconstructed: " << reconHandle.frames() << " frames, " << reconHandle.channels() << " channels\n";
        return 1;
    }

    long double sum_signal_power = 0.0;
    long double sum_noise_power = 0.0;

    vector<short> origBuffer(FRAMES_BUFFER_SIZE * origHandle.channels());
    vector<short> reconBuffer(FRAMES_BUFFER_SIZE * reconHandle.channels());

    size_t nFrames;
    while ((nFrames = origHandle.readf(origBuffer.data(), FRAMES_BUFFER_SIZE))) {
        reconHandle.readf(reconBuffer.data(), nFrames);

        origBuffer.resize(nFrames * origHandle.channels());
        reconBuffer.resize(nFrames * reconHandle.channels());

        for (size_t i = 0; i < origBuffer.size(); ++i) {
            long double s_orig = static_cast<long double>(origBuffer[i]);
            long double s_recon = static_cast<long double>(reconBuffer[i]);

            sum_signal_power += s_orig * s_orig;
            long double error = s_orig - s_recon;
            sum_noise_power += error * error;
        }
    }

    // Calcular SNR
    double snr;
    if (sum_noise_power == 0.0) {
        snr = numeric_limits<double>::infinity();
    } else {
        snr = 10.0 * log10(sum_signal_power / sum_noise_power);
    }

    cout << "Signal Power: " << sum_signal_power << endl;
    cout << "Noise Power: " << sum_noise_power << endl;
    cout << "SNR: " << snr << " dB" << endl;

    return 0;
}