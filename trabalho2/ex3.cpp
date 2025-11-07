#include <iostream>
#include <fstream>
#include <vector>
#include "golomb.h"

void test_coding(int m, NegativeHandling mode, const std::vector<int>& data) {
    std::cout << "--- Testando com m=" << m << " e modo ";
    std::cout << (mode == NegativeHandling::INTERLEAVING ? "Interleaving" : "Sign/Magnitude") << " ---\n";

    const char* filename = "test.bin";

    try {
        std::fstream fs_write(filename, std::ios::binary | std::ios::out);
        if (!fs_write) {
            std::cerr << "Erro ao abrir ficheiro para escrita\n"; return;
        }
        BitStream bs_write(fs_write, STREAM_WRITE);
        Golomb g(m, mode);

        std::cout << "Codificando: ";
        for (int val : data) {
            std::cout << val << " ";
            g.encode(val, bs_write);
        }
        std::cout << "\n";
        bs_write.close(); 

    } catch (const std::exception& e) {
        std::cerr << "Erro na codificação: " << e.what() << '\n';
        return;
    }

    try {
        std::fstream fs_read(filename, std::ios::binary | std::ios::in);
        if (!fs_read) {
            std::cerr << "Erro ao abrir ficheiro para leitura\n"; return;
        }
        BitStream bs_read(fs_read, STREAM_READ);
        Golomb g(m, mode);

        std::cout << "Descodificado: ";
        std::vector<int> decoded_data;
        for (size_t i = 0; i < data.size(); ++i) {
            int val = g.decode(bs_read);
            decoded_data.push_back(val);
            std::cout << val << " ";
        }
        std::cout << "\n";
        bs_read.close();

        if (data == decoded_data) {
            std::cout << "VERIFICAÇÃO: SUCESSO!\n\n";
        } else {
            std::cout << "VERIFICAÇÃO: FALHOU!\n\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Erro na descodificação: " << e.what() << '\n';
    }
}

int main() {
    std::vector<int> test_data = {0, 1, -1, 2, -2, 3, -3, 10, -15, 50, -100};

    test_coding(3, NegativeHandling::INTERLEAVING, test_data);
    test_coding(3, NegativeHandling::SIGN_AND_MAGNITUDE, test_data);
    
    test_coding(10, NegativeHandling::INTERLEAVING, test_data);
    test_coding(10, NegativeHandling::SIGN_AND_MAGNITUDE, test_data);
    
    test_coding(1, NegativeHandling::INTERLEAVING, {0, 1, 2, 3, 5, 10});

    return 0;
}