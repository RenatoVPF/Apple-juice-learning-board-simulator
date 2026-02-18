#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <cstdint>
#include <string>
#include <atomic>

// Chipe4017 (versão baseada no modelo: bit "1" deslocando-se para a direita)
class Chip4017{
    public:
        explicit Chip4017(unsigned int limitReset) : LimitReset(limitReset){
            if (LimitReset < 1 || LimitReset > 10) {
                throw std::invalid_argument("O pino de reset tem que estar configurado entre 1 e 10");
            }
            reset();
        }
         void shift() {
        // Desloca o bit ligado para a direita; quando sair, volta ao topo
        Out >>= 1;
        if (Out == 0) {
            Out = 1u << (LimitReset - 1);
            }
        }

        void reset() {
            // Estado inicial: última posição ligada 
            Out = 1u << (LimitReset - 1);
        }

        uint32_t getOut() const { return Out; }
        unsigned getLimitReset() const { return LimitReset; }

    
    
    private:
        unsigned LimitReset;
        uint32_t Out{0};
};


//painel de lampadas(renderização)
class LampPanel {
    public:
        explicit LampPanel(unsigned n) : n_(n) {}

        void draw(uint32_t bits, bool ligado) const {
            clearScreen();

            std::cout << "=== Simulador (estilo placa) ===\n";
            std::cout << "Status: " << (ligado ? "LIGADO" : "DESLIGADO") << "\n\n";

            // Mostra lâmpadas da esquerda para a direita (n_ lâmpadas)
            // Como seu Out usa bit alto primeiro, vamos mapear do bit (n_-1) até 0:
            std::cout << "Lâmpadas: ";
            for (int i = (int)n_ - 1; i >= 0; --i) {
                bool on = (bits >> i) & 1u;
                std::cout << (on ? "● " : "○ ");
            }
            std::cout << "\n\n";

            std::cout << "Controles:\n";
            std::cout << "  ENTER  -> liga/desliga\n";
            std::cout << "  r      -> reset\n";
            std::cout << "  0      -> sair\n";
            std::cout << "\n";
            std::cout.flush();
        }

    private:
        unsigned n_;

        static void clearScreen() {
            // ANSI: limpa tela e volta cursor pro topo (funciona bem no Linux)
            std::cout << "\033[2J\033[H";
        }
};

int main() {
    try {
        const unsigned LAMPADAS = 8;                 
        const auto PERIODO = std::chrono::milliseconds(400); // velocidade do "clock"

        Chip4017 chip4017(LAMPADAS);
        LampPanel painel(LAMPADAS);

        std::atomic<bool> running{true};
        std::atomic<bool> ligado{false};

        // Desenha o estado inicial
        painel.draw(chip4017.getOut(), ligado.load());

        // Thread do "motor": só gera pulsos quando estiver ligado
        std::thread motor([&]{
            while (running.load()) {
                if (ligado.load()) {
                    chip4017.shift();
                    painel.draw(chip4017.getOut(), true);
                    std::this_thread::sleep_for(PERIODO);
                } else {
                    // Quando desligado, dorme um pouco pra não consumir CPU
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }
            }
        });

        // Loop de entrada: ENTER alterna ligado/desligado, "0" sai, "r" reset
        std::string line;
        while (running.load()) {
            std::getline(std::cin, line);

            if (!std::cin) break; // EOF / erro

            if (line == "0") {
                running.store(false);
                break;
            }
            if (line == "r" || line == "R") {
                chip4017.reset();
                painel.draw(chip4017.getOut(), ligado.load());
                continue;
            }
            if (line.empty()) {
                // ENTER: toggle
                bool novo = !ligado.load();
                ligado.store(novo);
                painel.draw(chip4017.getOut(), novo);
                continue;
            }

            // qualquer outra coisa: ignora e redesenha
            painel.draw(chip4017.getOut(), ligado.load());
        }

        running.store(false);
        if (motor.joinable()) motor.join();

        std::cout << "\nEncerrado.\n";
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Erro: " << e.what() << "\n";
        return 1;
    }
}