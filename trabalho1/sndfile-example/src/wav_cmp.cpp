//------------------------------------------------------------------------------
//
// wav_cmp: compare two WAV files (original vs processed)
//
//------------------------------------------------------------------------------
#include <iostream>
#include <vector>
#include <cmath>
#include <sndfile.hh>

using namespace std;

constexpr size_t FRAMES_BUFFER_SIZE = 65536;

struct Stats {
    double mse = 0.0;
    double max_err = 0.0;
    double signal_energy = 0.0;
    double noise_energy = 0.0;
    size_t count = 0;
};

void update_stats(Stats &stats, short orig, short proc) {
    double e = static_cast<double>(orig) - static_cast<double>(proc);
    stats.mse += e * e;
    stats.max_err = max(stats.max_err, fabs(e));
    stats.signal_energy += static_cast<double>(orig) * orig;
    stats.noise_energy  += e * e;
    stats.count++;
}

void print_stats(const string& label, const Stats& stats) {
    double mse = (stats.count > 0) ? stats.mse / stats.count : 0.0;
    double snr = (stats.noise_energy > 0)
        ? 10.0 * log10(stats.signal_energy / stats.noise_energy)
        : INFINITY;

    cout << label << ":\n";
    cout << "  MSE (L2 norm): " << mse << "\n";
    cout << "  Max error (L∞ norm): " << stats.max_err << "\n";
    cout << "  SNR (dB): " << snr << "\n";
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        cerr << "Usage: " << argv[0] << " <original.wav> <processed.wav>\n";
        return 1;
    }

    string origFile = argv[1];
    string procFile = argv[2];

    SndfileHandle origHandle { origFile };
    SndfileHandle procHandle { procFile };

    if (origHandle.error() || procHandle.error()) {
        cerr << "Error: invalid input file(s)\n";
        return 1;
    }

    if (origHandle.samplerate() != procHandle.samplerate() ||
        origHandle.channels()   != procHandle.channels()) {
        cerr << "Error: files must have same sample rate and channels\n";
        return 1;
    }

    size_t channels = origHandle.channels();
    vector<Stats> stats(channels);  // per-channel
    Stats stats_avg;                // average channel (mono)

    vector<short> bufOrig(FRAMES_BUFFER_SIZE * channels);
    vector<short> bufProc(FRAMES_BUFFER_SIZE * channels);

    size_t nFramesOrig, nFramesProc;
    while ((nFramesOrig = origHandle.readf(bufOrig.data(), FRAMES_BUFFER_SIZE))) {
        nFramesProc = procHandle.readf(bufProc.data(), FRAMES_BUFFER_SIZE);
        if (nFramesOrig != nFramesProc) {
            cerr << "Error: files have different lengths\n";
            return 1;
        }

        for (size_t i = 0; i < nFramesOrig * channels; i += channels) {
            // per-channel stats
            for (size_t c = 0; c < channels; c++) {
                update_stats(stats[c], bufOrig[i+c], bufProc[i+c]);
            }

            // average channel
            short avgOrig = 0, avgProc = 0;
            for (size_t c = 0; c < channels; c++) {
                avgOrig += bufOrig[i+c];
                avgProc += bufProc[i+c];
            }
            avgOrig /= channels;
            avgProc /= channels;
            update_stats(stats_avg, avgOrig, avgProc);
        }
    }

    // Print results
    for (size_t c = 0; c < channels; c++) {
        print_stats("Channel " + to_string(c), stats[c]);
    }
    print_stats("Average (mono)", stats_avg);

    return 0;
}
