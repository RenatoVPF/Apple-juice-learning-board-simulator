#include <raylib.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <stdexcept>
#include <thread>


class Chip555 {
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

    bool isHigh() const { return stateHigh; }

    double getFrequency() const { return freq; }
    double getPeriod() const { return period; }

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
};


//  Chip4017 (contador 1-de-N por bitmask)

class Chip4017 {
public:
    explicit Chip4017(unsigned limitReset)
        : LimitReset(limitReset) {
        if (LimitReset < 1 || LimitReset > 10) {
            throw std::invalid_argument("LimitReset precisa estar entre 1 e 10.");
        }
        reset();
    }

    void shift() {
        Out >>= 1;
        if (Out == 0) {
            Out = 1u << (LimitReset - 1);
        }
    }

    void reset() {
        Out = 1u << (LimitReset - 1);
    }

    uint32_t getOut() const { return Out; }
    unsigned getLimitReset() const { return LimitReset; }

private:
    unsigned LimitReset;
    uint32_t Out{0};
};


//  Helpers de desenho (visual "placa")

static void DrawLedGlow(Vector2 center, float radius, Color core, Color glow) {
    // Ajustes finos do glow
    const int rings = 18;        // mais anéis
    const float step = 1.5f;     // anéis mais próximos (área menor por anel)
    const float gamma = 1.5f;    // curva para suavizar a queda do alpha

    for (int i = rings; i >= 1; --i) {
        float t = (float)i / (float)rings;      // 1..0
        float r = radius + i * step;

        // Curva de queda do brilho: mais suave perto do LED e decai no fim
        float falloff = powf(t, gamma);         // 0..1 (mais controlado)
        unsigned char a = (unsigned char)(glow.a * falloff * 0.2f);

        Color c = glow;
        c.a = a;
        DrawCircleV(center, r, c);
    }

    // Núcleo do LED
    DrawCircleV(center, radius, core);

    // “brilho interno” (highlight) para parecer LED real
    Color hi = core;
    hi.a = 90;
    DrawCircleV((Vector2){ center.x - radius * 0.25f, center.y - radius * 0.25f }, radius * 0.45f, hi);

    // Borda
    DrawCircleLines((int)center.x, (int)center.y, radius, Fade(BLACK, 0.30f));
}


static void DrawPanel(Rectangle rec) {
    // fundo geral
    ClearBackground((Color){ 18, 20, 24, 255 });

    // “placa”
    DrawRectangleRounded(rec, 0.15f, 16, (Color){ 30, 34, 42, 255 });
    DrawRectangleRoundedLinesEx(rec, 0.15f, 16, 2.0f, Fade(RAYWHITE, 0.10f));

    // textura leve (linhas)
    for (int x = (int)rec.x + 10; x < (int)(rec.x + rec.width); x += 22) {
        DrawLine(x, (int)rec.y + 8, x, (int)(rec.y + rec.height) - 8, Fade(RAYWHITE, 0.02f));
    }
}

int main() {
   
    // Valores do 555: ajuste para mudar velocidade
    const unsigned LAMPADAS = 8; // 1..10
    const double R1 = 10000.0;   // 10k
    const double R2 = 10000.0;   // 10k
    const double C  = 11e-6;     // 11uF (mais alto = mais lento)

    InitWindow(1200, 500, "Simulador - Lampadas (4017 + 555)");
    SetTargetFPS(60);

    Chip4017 chip4017(LAMPADAS);
    Chip555  chip555(R1, R2, C);

    std::atomic<bool> running{true};
    std::atomic<bool> ligado{false};

    std::mutex mtx; // protege chip4017 contra reset/shift simultâneos

    
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

    
    while (!WindowShouldClose()) {
       
        if (IsKeyPressed(KEY_ENTER)) {
            ligado.store(!ligado.load());
        }
        if (IsKeyPressed(KEY_R)) {
            std::lock_guard<std::mutex> lock(mtx);
            chip4017.reset();
        }
        if (IsKeyPressed(KEY_ZERO)) {
            running.store(false);
            break;
        }

        // Pega estado do 4017 (com lock pra não ler no meio de shift/reset)
        uint32_t bits = 0;
        {
            std::lock_guard<std::mutex> lock(mtx);
            bits = chip4017.getOut();
        }

        
        BeginDrawing();

        Rectangle panel = { 60, 80, 1080, 320 };
        DrawPanel(panel);

        // Cabeçalho
        const char* status = ligado.load() ? "LIGADO" : "DESLIGADO";
        DrawText(TextFormat("Status: %s  |  ENTER liga/desliga  |  R reset  |  0 sair", status),
                 40, 20, 18, Fade(RAYWHITE, 0.85f));

        // Indicador do clock (ponto que muda com isHigh)
        Color clk = chip555.isHigh() ? (Color){ 50, 220, 130, 255 } : (Color){ 200, 60, 60, 255 };
        DrawCircle(830, 30, 7, clk);
        DrawText("CLK", 845, 24, 16, Fade(RAYWHITE, 0.70f));

        // Info 555
        DrawText(TextFormat("555: f=%.2f Hz | T=%.3f s", chip555.getFrequency(), chip555.getPeriod()),
                 60, 80, 18, Fade(RAYWHITE, 0.55f));

        // LEDs
        float baseY = 280.0f;
        float radius = 32.0f;

        // Distribuição automática
        float margem = 120.0f;
        float areaUtil = 1200.0f - 2 * margem;
        float gap = areaUtil / (LAMPADAS - 1);
        float startX = margem;


        // pulsação leve pra “parecer LED”
        float t = (float)GetTime();
        float breathe = 0.5f + 0.5f * sinf(t * 3.2f); // 0..1

        for (int i = (int)LAMPADAS - 1; i >= 0; --i) {
            bool on = ((bits >> i) & 1u) != 0;

            int idx = (int)LAMPADAS - 1 - i; // 0..N-1 (ordem visual)
            Vector2 c = { startX + idx * gap, baseY };

            // base “apagado”
            Color offCore = (Color){ 120, 125, 135, 255 };
            Color offGlow = (Color){ 120, 125, 135, 0 };

          
           
            unsigned char aGlow = (unsigned char)(70 + 90 * breathe);
            Color onCore = (Color){ 70, 255, 130, 220 };
            Color onGlow = (Color){ 70, 255, 130, aGlow };

            if (on) DrawLedGlow(c, radius, onCore, onGlow);
            else    DrawLedGlow(c, radius, offCore, offGlow);

            DrawText(TextFormat("L%d", idx + 1), (int)(c.x - 14), (int)(c.y + 52), 18, Fade(RAYWHITE, 0.60f));
        }

       
        DrawText("Dica: aumente C (ex.: 47uF) para ficar mais lento; diminua C (ex.: 10uF) para acelerar.",
                 60, 350, 16, Fade(RAYWHITE, 0.45f));

        EndDrawing();
    }

    
    running.store(false);
    if (motor.joinable()) motor.join();
    CloseWindow();
    return 0;
}
