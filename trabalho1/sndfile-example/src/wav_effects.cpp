

#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <sndfile.hh>

using namespace std;

// Clamp helper
constexpr float CLAMP16(float v) { return max(-32768.0f, min(32767.0f, v)); }

// ECHO
void apply_echo(vector<short>& samples, int channels, int samplerate,
                float delay_ms, float decay, int repeats = 1) {
    int delay_samples = static_cast<int>((delay_ms / 1000.0f) * samplerate);
    vector<float> out(samples.begin(), samples.end());

    for (int r = 1; r <= repeats; ++r) {
        int offset = r * delay_samples;
        for (size_t i = 0; i + offset < samples.size() / channels; ++i) {
            for (int c = 0; c < channels; ++c) {
                size_t src = i * channels + c;
                size_t dst = (i + offset) * channels + c;
                if (dst < out.size())
                    out[dst] += out[src] * powf(decay, r);
            }
        }
    }

    for (size_t i = 0; i < out.size(); ++i)
        samples[i] = static_cast<short>(CLAMP16(out[i]));
}

// AMPLITUDE MODULATION
void apply_am(vector<short>& samples, int channels, int samplerate, float freq) {
    for (size_t i = 0; i < samples.size() / channels; ++i) {
        float t = static_cast<float>(i) / samplerate;
        float mod = 0.5f * (1.0f + sinf(2.0f * M_PI * freq * t));  // [0..1]
        for (int c = 0; c < channels; ++c) {
            size_t idx = i * channels + c;
            float v = samples[idx] * mod;
            samples[idx] = static_cast<short>(CLAMP16(v));
        }
    }
}

// DELAY MODULATION
void apply_delay_mod(vector<short>& samples, int channels, int samplerate,
                     float base_ms, float depth_ms, float freq) {
    vector<float> input(samples.begin(), samples.end());
    vector<short> out(samples.size());

    for (size_t i = 0; i < samples.size() / channels; ++i) {
        float t = static_cast<float>(i) / samplerate;
        float delay_ms = base_ms + depth_ms * sinf(2.0f * M_PI * freq * t);
        float delay_samples = (delay_ms / 1000.0f) * samplerate;

        for (int c = 0; c < channels; ++c) {
            float idx_f = i - delay_samples;
            float delayed = 0.0f;
            if (idx_f >= 1.0f) {
                size_t i0 = static_cast<size_t>(idx_f);
                size_t i1 = i0 + 1;
                float frac = idx_f - i0;
                size_t a = i0 * channels + c;
                size_t b = i1 * channels + c;
                if (b < input.size())
                    delayed = (1.0f - frac) * input[a] + frac * input[b];
                else
                    delayed = input[a];
            }
            size_t idx = i * channels + c;
            float mixed = 0.7f * input[idx] + 0.3f * delayed;
            out[idx] = static_cast<short>(CLAMP16(mixed));
        }
    }

    samples.swap(out);
}

// REVERB
void apply_reverb(vector<short>& samples, int channels, int samplerate,
                  float room_size, float damping) {
    int delay_samples = static_cast<int>(room_size * samplerate / 1000.0f);
    if (delay_samples < 1) delay_samples = 1;
    vector<float> out(samples.size());
    vector<float> buffer(delay_samples * channels, 0.0f);

    size_t buf_idx = 0;
    for (size_t i = 0; i < samples.size() / channels; ++i) {
        for (int c = 0; c < channels; ++c) {
            size_t idx = i * channels + c;
            float input = samples[idx];
            float feedback = buffer[buf_idx * channels + c];
            float y = input + feedback * 0.5f;
            buffer[buf_idx * channels + c] = y * damping;
            out[idx] = y;
        }
        buf_idx = (buf_idx + 1) % delay_samples;
    }

    for (size_t i = 0; i < out.size(); ++i)
        samples[i] = static_cast<short>(CLAMP16(out[i]));
}

// DISTORTION
void apply_distortion(vector<short>& samples, float gain) {
    for (size_t i = 0; i < samples.size(); ++i) {
        float x = samples[i] * gain / 32768.0f;
        float y;
        if (x < -1.0f)
            y = -1.0f;
        else if (x > 1.0f)
            y = 1.0f;
        else
            y = (3.0f / 2.0f) * (x - (x * x * x) / 3.0f);  // soft clip
        samples[i] = static_cast<short>(CLAMP16(y * 32768.0f));
    }
}

// HIGH-PASS FILTER
void apply_highpass(vector<short>& samples, int channels, int samplerate, float cutoff_hz) {
    float RC = 1.0f / (2.0f * M_PI * cutoff_hz);
    float dt = 1.0f / samplerate;
    float alpha = RC / (RC + dt);

    vector<float> prev_in(channels, 0.0f);
    vector<float> prev_out(channels, 0.0f);

    for (size_t i = 0; i < samples.size() / channels; ++i) {
        for (int c = 0; c < channels; ++c) {
            size_t idx = i * channels + c;
            float x = samples[idx];
            float y = alpha * (prev_out[c] + x - prev_in[c]);
            prev_in[c] = x;
            prev_out[c] = y;
            samples[idx] = static_cast<short>(CLAMP16(y));
        }
    }
}

// MAIN
int main(int argc, char *argv[]) {
    if (argc < 5) {
        cerr << "Usage: " << argv[0] << " <input.wav> <output.wav> <effect> <params...>\n";
        cerr << "Effects:\n"
             << "  echo <delay_ms> <decay> [repeats]\n"
             << "  am <freq>\n"
             << "  delay_mod <base_ms> <depth_ms> <freq>\n"
             << "  reverb <room_ms> <damping>\n"
             << "  distortion <gain>\n"
             << "  highpass <cutoff_hz>\n";
        return 1;
    }

    string inFile  = argv[1];
    string outFile = argv[2];
    string effect  = argv[3];

    SndfileHandle inHandle(inFile);
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

    vector<short> samples(inHandle.frames() * channels);
    inHandle.readf(samples.data(), inHandle.frames());

    if (effect == "echo") {
        float delay_ms = stof(argv[4]);
        float decay    = stof(argv[5]);
        int repeats    = (argc >= 7) ? stoi(argv[6]) : 1;
        apply_echo(samples, channels, samplerate, delay_ms, decay, repeats);
    } else if (effect == "am") {
        float freq = stof(argv[4]);
        apply_am(samples, channels, samplerate, freq);
    } else if (effect == "delay_mod") {
        float base_ms  = stof(argv[4]);
        float depth_ms = stof(argv[5]);
        float freq     = stof(argv[6]);
        apply_delay_mod(samples, channels, samplerate, base_ms, depth_ms, freq);
    } else if (effect == "reverb") {
        float room_ms  = stof(argv[4]);
        float damping  = stof(argv[5]);
        apply_reverb(samples, channels, samplerate, room_ms, damping);
    } else if (effect == "distortion") {
        float gain = stof(argv[4]);
        apply_distortion(samples, gain);
    } else if (effect == "highpass") {
        float cutoff = stof(argv[4]);
        apply_highpass(samples, channels, samplerate, cutoff);
    } else {
        cerr << "Unknown effect\n";
        return 1;
    }

    SndfileHandle outHandle(outFile, SFM_WRITE, inHandle.format(), channels, samplerate);
    outHandle.writef(samples.data(), inHandle.frames());

    cout << "Effect applied: " << effect << " -> " << outFile << endl;
    return 0;
}

