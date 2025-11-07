#include "golomb.h"
#include <iostream>

void Golomb::write_unary(unsigned int n, BitStream& bs) {
    for (unsigned int i = 0; i < n; ++i) {
        bs.write_bit(0);
    }
    bs.write_bit(1);
}

unsigned int Golomb::read_unary(BitStream& bs) {
    unsigned int n = 0;
    int bit;
    
    while ((bit = bs.read_bit()) == 0) {
        n++;
    }

    if (bit == EOF) {
        throw std::runtime_error("EOF atingido ao ler código unário");
    }
    
    return n;
}

void Golomb::encode_unsigned(unsigned int n, BitStream& bs) {
    if (m_m == 1) {
        write_unary(n, bs);
        return;
    }

    unsigned int q = n / m_m;
    unsigned int r = n % m_m;

    write_unary(q, bs);

    int b = (int)std::ceil(std::log2((double)m_m));
    int cutoff = (1 << b) - m_m;

    if (r < (unsigned int)cutoff) {
        bs.write_n_bits(r, b - 1);
    } else {
        bs.write_n_bits(r + cutoff, b);
    }
}

unsigned int Golomb::decode_unsigned(BitStream& bs) {
    if (m_m == 1) {
        return read_unary(bs);
    }

    unsigned int q = read_unary(bs);

    int b = (int)std::ceil(std::log2((double)m_m));
    int cutoff = (1 << b) - m_m;
    unsigned int r;

    unsigned int r_head = bs.read_n_bits(b - 1);

    if (r_head < (unsigned int)cutoff) {
        r = r_head;
    } else {
        int r_tail = bs.read_bit();
        if (r_tail == EOF) {
             throw std::runtime_error("EOF atingido ao descodificar resto Golomb");
        }
        
        unsigned int r_full = (r_head << 1) | r_tail;
        r = r_full - cutoff;
    }

    return q * m_m + r;
}

void Golomb::encode(int n, BitStream& bs) {
    if (m_neg_handling == NegativeHandling::INTERLEAVING) {
        unsigned int mapped_n;
        if (n >= 0) {
            mapped_n = (unsigned int)(2 * n);
        } else {
            mapped_n = (unsigned int)(-2 * n - 1);
        }
        encode_unsigned(mapped_n, bs);

    } else {
        int sign_bit = (n < 0) ? 1 : 0;
        bs.write_bit(sign_bit);
        
        encode_unsigned((unsigned int)std::abs(n), bs);
    }
}

int Golomb::decode(BitStream& bs) {
    if (m_neg_handling == NegativeHandling::INTERLEAVING) {
        unsigned int mapped_n = decode_unsigned(bs);

        if (mapped_n % 2 == 0) {
            return (int)(mapped_n / 2);
        } else {
            return -(int)((mapped_n + 1) / 2);
        }

    } else {
        int sign_bit = bs.read_bit();
        if (sign_bit == EOF) {
            throw std::runtime_error("EOF atingido ao descodificar bit de sinal");
        }

        unsigned int abs_val = decode_unsigned(bs);

        return (sign_bit == 1) ? -(int)abs_val : (int)abs_val;
    }
}