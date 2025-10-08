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
#include <vector>
#include <sndfile.hh>
#include "wav_hist.h"

using namespace std;

constexpr size_t FRAMES_BUFFER_SIZE = 65536; // Buffer for reading frames

int main(int argc, char *argv[]) {

    if(argc < 4) {
        cerr << "Usage: " << argv[0] << " <input file> <channel | mid | side> <binExp>\n";
        return 1;
    }

    SndfileHandle sndFile { argv[1] };
    if(sndFile.error()) {
        cerr << "Error: invalid input file\n";
        return 1;
    }

    if((sndFile.format() & SF_FORMAT_TYPEMASK) != SF_FORMAT_WAV) {
        cerr << "Error: file is not in WAV format\n";
        return 1;
    }

    if((sndFile.format() & SF_FORMAT_SUBMASK) != SF_FORMAT_PCM_16) {
        cerr << "Error: file is not in PCM_16 format\n";
        return 1;
    }

    string mode { argv[2] };
    int binExp = 0;

    try {
        binExp = stoi(argv[3]);
        if (binExp < 0) {
            cerr << "Error: bin exponent must be non-negative\n";
            return 1;
        }
    } catch (...) {
        cerr << "Error: invalid bin exponent\n";
        return 1;
    }

    int channel = -1;
    if (mode != "mid" && mode != "side") {
        try {
            channel = stoi(mode);
        } catch (...) {
            cerr << "Error: invalid mode/channel requested\n";
            return 1;
        }

        if (channel >= sndFile.channels()) {
            cerr << "Error: invalid channel requested\n";
            return 1;
        }
    }

    size_t nFrames;
    vector<short> samples(FRAMES_BUFFER_SIZE * sndFile.channels());
    WAVHist hist { sndFile, binExp };

    while((nFrames = sndFile.readf(samples.data(), FRAMES_BUFFER_SIZE))) {
        samples.resize(nFrames * sndFile.channels());
        hist.update(samples);
    }

    if (mode == "mid") {
        hist.dumpMid("mid_hist.txt");
    } else if (mode == "side") {
        hist.dumpSide("side_hist.txt");
    } else {
        string fname = "channel" + to_string(channel) + "_hist.txt";
        hist.dumpChannel(channel, fname);
    }

    return 0;
}



