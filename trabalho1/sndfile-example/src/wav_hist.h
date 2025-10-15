#ifndef WAVHIST_H
#define WAVHIST_H

#include <iostream>
#include <vector>
#include <map>
#include <sndfile.hh>
#include <fstream>

class WAVHist {
  private:
    std::vector<std::map<int, size_t>> channelCounts;  // per-channel
    std::map<int, size_t> midCounts;                   // MID
    std::map<int, size_t> sideCounts;                  // SIDE
    size_t nChannels;
    int binSize;

    inline int bin(int value) const {
        return (value >= 0 ? value / binSize : (value - binSize + 1) / binSize);
    }

  public:
    WAVHist(const SndfileHandle& sfh, int binExp = 0) {
        nChannels = sfh.channels();
        channelCounts.resize(nChannels);
        binSize = 1 << binExp; // support coarse bins
    }

    void update(const std::vector<short>& samples) {
        if (nChannels == 2) {
            for (size_t i = 0; i < samples.size(); i += 2) {
                short L = samples[i];
                short R = samples[i + 1];

                channelCounts[0][bin(L)]++;
                channelCounts[1][bin(R)]++;

                short mid  = (L + R) / 2;
                short side = (L - R) / 2;

                midCounts[bin(mid)]++;
                sideCounts[bin(side)]++;
            }
        } else {
            size_t n = 0;
            for (auto s : samples)
                channelCounts[n++ % nChannels][bin(s)]++;
        }
    }

    void dumpChannel(size_t channel, const std::string &filename) const {
        std::ofstream out(filename);
        if (!out.is_open()) {
            std::cerr << "Error: could not open " << filename << "\n";
            return;
        }
        for (auto [value, count] : channelCounts[channel])
            out << value * binSize << '\t' << count << '\n';
        out.close();
    }

    void dumpMid(const std::string &filename) const {
        if (nChannels != 2) {
            std::cerr << "Warning: MID channel only available for stereo files\n";
            return;
        }
        std::ofstream out(filename);
        if (!out.is_open()) {
            std::cerr << "Error: could not open " << filename << "\n";
            return;
        }
        for (auto [value, count] : midCounts)
            out << value * binSize << '\t' << count << '\n';
        out.close();
    }

    void dumpSide(const std::string &filename) const {
        if (nChannels != 2) {
            std::cerr << "Warning: SIDE channel only available for stereo files\n";
            return;
        }
        std::ofstream out(filename);
        if (!out.is_open()) {
            std::cerr << "Error: could not open " << filename << "\n";
            return;
        }
        for (auto [value, count] : sideCounts)
            out << value * binSize << '\t' << count << '\n';
        out.close();
    }
};

#endif
