#include "image_codec.h"
#include <iostream>
#include <string>

void print_usage() {
    std::cerr << "Usage: codec [mode] [options]\n\n"
              << "Modes:\n"
              << "  -e <input.png> -o <output.gicl>   Encode an image file\n"
              << "  -d <input.gicl> -o <output.png>   Decode a GICL file\n"
              << "Encode Options:\n"
              << "  -m <value>     Use fixed Golomb parameter 'm' (e.g., -m 10)\n"
              << "  -a             Use adaptive 'm' (block-based, recommended)\n"
              << "                 (If neither -m nor -a is given, -a is default)\n"
              << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 5) {
        print_usage();
        return 1;
    }

    std::string mode;
    std::string in_file;
    std::string out_file;
    int m = -1;
    bool adaptive = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-e") {
            mode = "encode";
            if (i + 1 < argc) in_file = argv[++i];
        } else if (arg == "-d") {
            mode = "decode";
            if (i + 1 < argc) in_file = argv[++i];
        } else if (arg == "-o") {
            if (i + 1 < argc) out_file = argv[++i];
        } else if (arg == "-m") {
            if (i + 1 < argc) m = std::stoi(argv[++i]);
        } else if (arg == "-a") {
            adaptive = true;
        }
    }

    if (mode.empty() || in_file.empty() || out_file.empty()) {
        std::cerr << "Error: Mode, input file, and output file must be specified.\n";
        print_usage();
        return 1;
    }

    if (mode == "encode" && m == -1 && !adaptive) {
        adaptive = true;
    }
    
    if (mode == "encode" && m != -1 && adaptive) {
        std::cerr << "Error: Cannot use -m and -a at the same time.\n";
        print_usage();
        return 1;
    }

    try {
        if (mode == "encode") {
            ImageCodec codec(in_file, out_file, m, adaptive);
            codec.encode();
        } else {
            ImageCodec codec(in_file, out_file);
            codec.decode();
        }
    } catch (const std::exception& e) {
        std::cerr << "An error occurred: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}