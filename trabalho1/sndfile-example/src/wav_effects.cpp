//------------------------------------------------------------------------------
//
// wav_effects: apply simple audio effects to WAV files
//
//------------------------------------------------------------------------------
#include <iostream>
#include <vector>
#include <cmath>
#include <sndfile.hh>

using namespace std;

constexpr size_t FRAMES_BUFFER_SIZE = 65536;

void apply_echo(vector<short>& samples, int channels, int samplerate,
                float delay_ms, float decay, int repeats = 1) {
    int delay_samples = static_cast<int>((delay_ms / 1000.0f) * samplerate);
    vector<short> out(samples);

    for (int r = 1; r <= repeats; r++) {
        int offset = r * delay_samples;
        for (size_t i = 0; i + offset * channels < samples.size(); i++) {
            for (int c = 0; c < channels; c++) {
                int idx = i + c;
                int delayed_idx = idx + offset * channels;
                int v = out[delayed_idx] + decay * out[idx];
                out[delayed_idx] = max<short>(-32768, min<short>(32767, v));
            }
        }
    }
    samples.swap(out);
}

void apply_am(vector<short>& samples, int channels, int samplerate, float freq) {
    for (size_t i = 0; i < samples.size(); i += channels) {
        float t = static_cast<float>(i / channels) / samplerate;
        float mod = 0.5f * (1.0f + sinf(2.0f * M_PI * freq * t)); // [0..1]
        for (int c = 0; c < channels; c++) {
            int idx = i + c;
            int v = static_cast<int>(samples[idx] * mod);
            samples[idx] = max<short>(-32768, min<short>(32767, v));
        }
    }
}

void apply_delay_mod(vector<short>& samples, int channels, int samplerate,
                     float base_ms, float depth_ms, float freq) {
    vector<short> out(samples);
    for (size_t i = 0; i < samples.size() / channels; i++) {
        float t = static_cast<float>(i) / samplerate;
        float delay_ms = base_ms + depth_ms * sinf(2.0f * M_PI * freq * t);
        int delay_samples = static_cast<int>((delay_ms / 1000.0f) * samplerate);
        for (int c = 0; c < channels; c++) {
            size_t idx = i * channels + c;
            size_t d_idx = (i >= delay_samples) ? (i - delay_samples) * channels + c : idx;
            int v = (out[idx] + out[d_idx]) / 2;
            out[idx] = static_cast<short>(max(-32768, min(32767, v)));
        }
    }
    samples.swap(out);
}

int main(int argc, char *argv[]) {
    if (argc < 5) {
        cerr << "Usage: " << argv[0] << " <input.wav> <output.wav> <effect> <params...>\n";
        cerr << "Effects:\n"
             << "  echo <delay_ms> <decay>\n"
             << "  multi_echo <delay_ms> <decay> <count>\n"
             << "  am <freq>\n"
             << "  delay_mod <base_ms> <depth_ms> <freq>\n";
        return 1;
    }

    string inFile  = argv[1];
    string outFile = argv[2];
    string effect  = argv[3];

    SndfileHandle inHandle { inFile };
    if (inHandle.error()) {
        cerr << "Error: invalid input file\n";
        return 1;
    }
    if ((inHandle.format() & SF_FORMAT_TYPEMASK) != SF_FORMAT_WAV ||
        (inHandle.format() & SF_FORMAT_SUBMASK) != SF_FORMAT_PCM_16) {
        cerr << "Error: only PCM16 WAV files supported\n";
        return 1;
    }

    int channels   = inHandle.channels();
    int samplerate = inHandle.samplerate();

    SndfileHandle outHandle {
        outFile, SFM_WRITE, inHandle.format(),
        channels, samplerate
    };

    vector<short> samples(inHandle.frames() * channels);
    inHandle.readf(samples.data(), inHandle.frames());

    if (effect == "echo" && argc >= 6) {
        float delay_ms = stof(argv[4]);
        float decay    = stof(argv[5]);
        apply_echo(samples, channels, samplerate, delay_ms, decay, 1);
    } else if (effect == "multi_echo" && argc >= 7) {
        float delay_ms = stof(argv[4]);
        float decay    = stof(argv[5]);
        int count      = stoi(argv[6]);
        apply_echo(samples, channels, samplerate, delay_ms, decay, count);
    } else if (effect == "am" && argc >= 5) {
        float freq = stof(argv[4]);
        apply_am(samples, channels, samplerate, freq);
    } else if (effect == "delay_mod" && argc >= 7) {
        float base_ms  = stof(argv[4]);
        float depth_ms = stof(argv[5]);
        float freq     = stof(argv[6]);
        apply_delay_mod(samples, channels, samplerate, base_ms, depth_ms, freq);
    } else {
        cerr << "Error: invalid effect or parameters\n";
        return 1;
    }

    outHandle.writef(samples.data(), inHandle.frames());
    cout << "Effect applied, saved to " << outFile << endl;
    return 0;
}
