#ifndef GOLOMB_H
#define GOLOMB_H

#include "bit_stream.h"
#include <cmath>
#include <stdexcept>

enum class NegativeHandling {
    SIGN_AND_MAGNITUDE,
    INTERLEAVING
};

class Golomb {
private:
    int m_m;
    NegativeHandling m_neg_handling;

    void encode_unsigned(unsigned int n, BitStream& bs);
    unsigned int decode_unsigned(BitStream& bs);
    void write_unary(unsigned int n, BitStream& bs);
    unsigned int read_unary(BitStream& bs);

public:
    Golomb(int m, NegativeHandling neg_handling = NegativeHandling::INTERLEAVING)
        : m_m(m), m_neg_handling(neg_handling) {
        if (m_m <= 0) {
            throw std::invalid_argument("O parâmetro 'm' do Golomb deve ser > 0");
        }
    }

    void set_m(int m) {
        if (m <= 0) {
            throw std::invalid_argument("O parâmetro 'm' do Golomb deve ser > 0");
        }
        m_m = m;
    }

    int get_m() const {
        return m_m;
    }

    void encode(int n, BitStream& bs);

    int decode(BitStream& bs);
};

#endif