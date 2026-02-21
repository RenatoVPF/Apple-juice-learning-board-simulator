/*
    O simulador com interface gráfica da placa de aprendizagem, a Apple Juice, foi desenvolvido para o laboratório da FnEsc, no Departamento de Física da UFS. 
    O sistema foi implementado principalmente utilizando o paradigma de programação orientada a objetos, com algumas funcionalidades de caráter procedural.

    A Apple Juice é uma placa de aprendizagem voltada ao estudo de circuitos digitais, construídos com circuitos integrados discretos, como o CD4017, NE555 
    e CD4026. O sistema oferece suporte à entrada de clock externo, permitindo incrementar a contagem na parte do circuito responsável pela decodificação 
    binária para decimal (a parte que possui os CD4026). Além disso, possui um botão de reset para os circuitos CD4017 e um switch para alternar entre o clock 
    externo e o clock interno gerado pelo 555 configurado em modo astável.

    O objetivo deste projeto foi aplicar os conceitos de programação orientada a objetos apresentados em sala de aula na disciplina de POO, sob orientação do 
    professor Carlos Estombelo. Conceitos como tratamento de exceções com try e catch, encapsulamento, herança e polimorfismo foram implementados e utilizados 
    ao longo do desenvolvimento deste código. 

    Responsáveis pelo desenvolvimento deste projeto: Francisco, Renato, Arthur, Matos e Carlos Eduardo 
*/

#include <iostream>             // Entrada e saída padrão (cout, cerr, etc.)
#include <cmath>                // Funções matemáticas (pow, sin, etc.)
#include <cstdint>              // Tipos inteiros com tamanho fixo (uint32_t, int64_t, etc.)
#include <mutex>                // Controle de exclusão mútua para threads (std::mutex, std::lock_guard)
#include <atomic>               // Variáveis atômicas para comunicação segura entre threads (std::atomic)
#include <stdexcept>            // Exceções padrão (std::invalid_argument)
#include <thread>               // Threads do C++ (std::thread)
#include <chrono>               // Controle de tempo e delays (std::chrono::duration, sleep_for)

/*
    Biblioteca gráfica:

    Coloquei esta biblioteca dentro do namespace 'ray' para diferenciá-la de outras bibliotecas.
    Como ela foi escrita em C, não possui namespace nativamente, ao contrário de bibliotecas C++ como 'iostream', que ficam dentro do 'std'.
*/
namespace ray{
    #include <raylib.h>
}


// Simulação do Chip555 configurado em modo astável
class Chip555 {
private:
    double R1, R2, C;
    double tHigh = 0.0;
    double tLow  = 0.0;
    double period = 0.0;
    double freq   = 0.0;
    std::atomic<bool> stateHigh{false};

    void calcTimings() {
        // Modo astável (aprox): tH = 0.693*(R1+R2)*C; tL = 0.693*R2*C
        tHigh = 0.693 * (R1 + R2) * C;
        tLow  = 0.693 * (R2) * C;
        period = tHigh + tLow;
        freq = (period > 0) ? (1.0 / period) : 0.0;
    }

public:
    Chip555(double r1Ohms, double r2Ohms, double cFarads)
        : R1(r1Ohms), R2(r2Ohms), C(cFarads) {
        if (R1 <= 0 || R2 <= 0 || C <= 0) {
            throw std::invalid_argument("R1, R2 e C precisam ser > 0");
        }
        calcTimings();
    }

    // Simula um ciclo de clock (HIGH e LOW) com delays
    void pulse() {
        stateHigh = true;
        std::this_thread::sleep_for(std::chrono::duration<double>(tHigh));

        stateHigh = false;
        std::this_thread::sleep_for(std::chrono::duration<double>(tLow));
    }

    bool isHigh() const { 
        return stateHigh; 
    }

    double getFrequency() const { 
        return freq; 
    }

    double getPeriod() const { 
        return period; 
    }
};


//  Chip4017 (contador johnsson)
class Chip4017 {
private:
    unsigned LimitReset;
    uint32_t Out{0};

public:
    explicit Chip4017(unsigned limitReset)
        : LimitReset(limitReset) {
        if (LimitReset < 1 || LimitReset > 10) {
            throw std::invalid_argument("LimitReset precisa estar entre 1 e 10.");
        }
        reset();
    }

    // método que reage ao pulso do clock deslocando o bit mais significativo para a direita
    void shift() {
        Out >>= 1;
        if (Out == 0) {
            Out = 1u << (LimitReset - 1);
        }
    }

    // método para aplicar o reset no chips
    void reset() {
        Out = 1u << (LimitReset - 1);
    }

    // apenas retornam - não podem alterar o valor
    uint32_t getOut() const { 
        return Out; 
    }

    unsigned getLimitReset() const { 
        return LimitReset; 
    }
};


/*
    Parte do código responsável pela simulação da formação do efeito de luminosidade dos leds para deixa-los mais realistas
*/
static void DrawLedGlow(ray::Vector2 center, float radius, ray::Color core, ray::Color glow) {
    // Ajustes finos do glow
    const int rings = 18;        
    const float step = 1.5f;     
    const float gamma = 1.5f;    

    for (int i = rings; i >= 1; --i) {
        float t = (float)i / (float)rings;      
        float r = radius + i * step;

        // Curva de queda do brilho: mais suave perto do LED e decai no fim 
        float falloff = powf(t, gamma);         
        unsigned char a = (unsigned char)(glow.a * falloff * 0.2f);

        ray::Color c = glow;
        c.a = a;
        DrawCircleV(center, r, c);
    }

    // Núcleo do LED
    ray::DrawCircleV(center, radius, core);

    // “brilho interno” para parecer LED real
    ray::Color hi = core;
    hi.a = 90;
    ray::DrawCircleV((ray::Vector2){ center.x - radius * 0.25f, center.y - radius * 0.25f }, radius * 0.45f, hi);

    // Borda
    ray::DrawCircleLines((int)center.x, (int)center.y, radius, ray::Fade(ray::BLACK, 0.30f));
}



// Desenha a placa: fundo escuro, retângulo arredondado e linhas verticais como textura
static void DrawPanel(ray::Rectangle rec) {
    // fundo geral
    ray::ClearBackground((ray::Color){ 18, 20, 24, 255 });

    // “placa”
    ray::DrawRectangleRounded(rec, 0.15f, 16, (ray::Color){ 30, 34, 42, 255 });
    ray::DrawRectangleRoundedLinesEx(rec, 0.15f, 16, 2.0f, ray::Fade(ray::RAYWHITE, 0.10f));

    // textura leve (linhas)
    for (int x = (int)rec.x + 10; x < (int)(rec.x + rec.width); x += 22) {
        ray::DrawLine(x, (int)rec.y + 8, x, (int)(rec.y + rec.height) - 8, ray::Fade(ray::RAYWHITE, 0.02f));
    }
}



/*
    Esta classe é responsável pela parte principal do simulador: Ele simula o conjunto de todos os circuitos integrados em uma única classe
    Também é responsável pela interface gráfica
*/
class BoardAppleJuice {
public:
    void run() {
        // Valores do 555: ajuste para mudar velocidade
        const unsigned qtLeds = 4; 
        const double R1 = 10000.0;   
        const double R2 = 10000.0;   
        const double C  = 11e-6;    

        // criando a janela do simulador e limitando em 60 FPS para reduzir o consumo da memória RAM
        ray::InitWindow(1200, 1000, "Simulador do Apple Juice");
        ray::SetTargetFPS(60); 

        // aplicando os valores nos circuitos integrados
        Chip4017 chip4017(qtLeds);
        Chip555  chip555(R1, R2, C);

        // Variáveis booleanas seguras para threads: 'running' controla a execução da thread, já 'ligado' controla se o circuito está ativo.
        std::atomic<bool> running{true};
        std::atomic<bool> ligado{false};

        /*
            Thread que simula o funcionamento do circuito em paralelo.
            O Chip555 gera pulsos (clock) e, a cada ciclo, o Chip4017 avança.
            O mutex evita condições de corrida no acesso ao contador.
            Quando desligado, a thread entra em espera para economizar CPU.
        */
        std::mutex mtx; 
        std::thread motor([&]{
            while (running.load()) {
                if (ligado.load()) {
                    chip555.pulse(); 
                    {
                        std::lock_guard<std::mutex> lock(mtx);
                        chip4017.shift();
                    }
                } else {
                    std::this_thread::sleep_for(std::chrono::milliseconds(30));
                }
            }
        });

        while (!ray::WindowShouldClose()) {

            // condicionais de controle do simulador
            if (ray::IsKeyPressed(ray::KEY_ENTER)) {
                ligado.store(!ligado.load());
            }

            if (ray::IsKeyPressed(ray::KEY_R)) {
                std::lock_guard<std::mutex> lock(mtx);
                chip4017.reset();
            }

            if (ray::IsKeyPressed(ray::KEY_ZERO)) {
                running.store(false);
                break;
            }


            /*
                Lê com segurança o estado atual do Chip4017 usando mutex
                para evitar condições de corrida com a thread do motor.
            */
            uint32_t bits = 0;
            {
                std::lock_guard<std::mutex> lock(mtx);
                bits = chip4017.getOut();
            }


            // renderizando na tela com base no que foi calculado anteriormente -> aqui tudo ganha forma no sentido "visual".
            ray::BeginDrawing();

                // Painel da placa
                ray::Rectangle panel = { 60, 80, 1080, 320 };
                DrawPanel(panel);

                // Cabeçalho e status do sistema
                const char* status = ligado.load() ? "LIGADO" : "DESLIGADO";
                ray::DrawText(
                    ray::TextFormat("Status: %s  |  ENTER liga/desliga  |  R reset  |  0 sair", status),
                    40, 20, 18, ray::Fade(ray::RAYWHITE, 0.85f)
                );

                // Indicador do clock do 555
                ray::Color clk = chip555.isHigh() ? (ray::Color){ 50, 220, 130, 255 } : (ray::Color){ 200, 60, 60, 255 };
                ray::DrawCircle(830, 30, 7, clk);
                ray::DrawText("CLK", 845, 24, 16, Fade(ray::RAYWHITE, 0.70f));

                // Informações do CI 555 (frequência e período)
                ray::DrawText(
                    ray::TextFormat("555: f=%.2f Hz | T=%.3f s", chip555.getFrequency(), chip555.getPeriod()),
                    60, 80, 18, ray::Fade(ray::RAYWHITE, 0.55f)
                );

                // Configuração da posição e tamanho dos LEDs
                float baseY = 280.0f;
                float radius = 32.0f;
                float margem = 120.0f;
                float areaUtil = 1200.0f - 2 * margem;
                float gap = areaUtil / (qtLeds - 1);
                float startX = margem;

                // Cálculo de pulsação “breathe” para efeito visual
                float t = (float)ray::GetTime();
                float breathe = 0.5f + 0.5f * sinf(t * 3.2f);

                // Renderização dos LEDs (acendidos ou apagados)
                for (int i = (int)qtLeds - 1; i >= 0; --i) {
                    bool on = ((bits >> i) & 1u) != 0;
                    int idx = (int)qtLeds - 1 - i;
                    ray::Vector2 c = { startX + idx * gap, baseY };
                
                    ray::Color offCore = (ray::Color){ 120, 125, 135, 255 };
                    ray::Color offGlow = (ray::Color){ 120, 125, 135, 0 };
                
                    unsigned char aGlow = (unsigned char)(70 + 90 * breathe);
                    ray::Color onCore = (ray::Color){ 70, 255, 130, 220 };
                    ray::Color onGlow = (ray::Color){ 70, 255, 130, aGlow };
                
                    if (on) {
                        DrawLedGlow(c, radius, onCore, onGlow);
                    } else {
                        DrawLedGlow(c, radius, offCore, offGlow);
                    } 
                
                    ray::DrawText(
                        ray::TextFormat("L%d", idx + 1),
                        (int)(c.x - 14),
                        (int)(c.y + 52),
                        18,
                        ray::Fade(ray::RAYWHITE, 0.60f)
                    );
                }

                // Mensagem de dica para ajuste do capacitor C
                ray::DrawText("Dica: aumente C (ex.: 47uF) para ficar mais lento; diminua C (ex.: 10uF) para acelerar.", 60, 360, 16, ray::Fade(ray::RAYWHITE, 0.45f));
                ray::EndDrawing();
        }

        // Para a thread definindo 'running' como falso e aguarda sua finalização com join se ainda estiver ativa.
        running.store(false);
        if (motor.joinable()) {
            motor.join();
        }
        ray::CloseWindow();
    }
};

// função main: Apenas "orquestra"
int main() {
    BoardAppleJuice appleJuice;
    appleJuice.run();
    return 0;
}
