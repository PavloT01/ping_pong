/* ================================================================
 * Ping Pong — Jeu 2 joueurs en C++ avec SDL2 (sans SDL2_ttf)
 * Auteur : PavloT01
 *
 * Compilation :
 *   g++ -std=c++17 -o pong pong.cpp $(sdl2-config --cflags --libs)
 *
 * Contrôles :
 *   Joueur 1 (gauche) : W = monter, S = descendre
 *   Joueur 2 (droite) : ↑ = monter, ↓ = descendre
 *   Espace / Entrée   : lancer la balle
 *   R                 : rejouer après fin de partie
 *   Échap             : quitter
 * ================================================================ */

#include <SDL2/SDL.h>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <vector>
#include <string>

/* ----------------------------------------------------------------
 * Constantes
 * ---------------------------------------------------------------- */
static const int   WIN_W         = 900;
static const int   WIN_H         = 600;
static const int   FPS           = 60;
static const int   WIN_SCORE     = 7;

static const int   PADDLE_W      = 14;
static const int   PADDLE_H      = 90;
static const int   PADDLE_SPEED  = 7;
static const int   PADDLE_MARGIN = 24;

static const float BALL_RADIUS   = 10.0f;
static const float BALL_SPEED_0  = 5.5f;
static const float BALL_ACCEL    = 0.35f;
static const float BALL_MAX_SPD  = 14.0f;

/* ----------------------------------------------------------------
 * Couleurs
 * ---------------------------------------------------------------- */
struct Color { Uint8 r, g, b, a; };

static const Color C_BG    = { 15,  15,  20,  255 };
static const Color C_NET   = { 60,  60,  80,  200 };
static const Color C_WHITE = { 240, 240, 245, 255 };
static const Color C_P1    = { 80,  180, 255, 255 };
static const Color C_P2    = { 255, 120, 80,  255 };
static const Color C_BALL  = { 255, 220, 60,  255 };
static const Color C_TRAIL = { 255, 220, 60,  60  };

/* ----------------------------------------------------------------
 * Polices pixel (5x7) pour les chiffres 0-9
 * Chaque chiffre = 7 lignes de 5 bits
 * ---------------------------------------------------------------- */
static const Uint8 DIGITS[10][7] = {
    { 0b01110, 0b10001, 0b10011, 0b10101, 0b11001, 0b10001, 0b01110 }, /* 0 */
    { 0b00100, 0b01100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110 }, /* 1 */
    { 0b01110, 0b10001, 0b00001, 0b00110, 0b01000, 0b10000, 0b11111 }, /* 2 */
    { 0b11111, 0b00010, 0b00100, 0b00110, 0b00001, 0b10001, 0b01110 }, /* 3 */
    { 0b00010, 0b00110, 0b01010, 0b10010, 0b11111, 0b00010, 0b00010 }, /* 4 */
    { 0b11111, 0b10000, 0b11110, 0b00001, 0b00001, 0b10001, 0b01110 }, /* 5 */
    { 0b01110, 0b10000, 0b10000, 0b11110, 0b10001, 0b10001, 0b01110 }, /* 6 */
    { 0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b01000, 0b01000 }, /* 7 */
    { 0b01110, 0b10001, 0b10001, 0b01110, 0b10001, 0b10001, 0b01110 }, /* 8 */
    { 0b01110, 0b10001, 0b10001, 0b01111, 0b00001, 0b00001, 0b01110 }, /* 9 */
};

static void drawDigit(SDL_Renderer* ren, int digit, int x, int y,
                      int scale, Color c)
{
    SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a);
    for (int row = 0; row < 7; row++) {
        for (int col = 0; col < 5; col++) {
            if (DIGITS[digit][row] & (0b10000 >> col)) {
                SDL_Rect px{ x + col * scale, y + row * scale,
                             scale, scale };
                SDL_RenderFillRect(ren, &px);
            }
        }
    }
}

static void drawNumber(SDL_Renderer* ren, int num, int x, int y,
                       int scale, Color c)
{
    std::string s = std::to_string(num);
    int offset = 0;
    for (char ch : s) {
        drawDigit(ren, ch - '0', x + offset, y, scale, c);
        offset += 6 * scale; /* 5 pixels + 1 espace */
    }
}

/* ----------------------------------------------------------------
 * Utilitaires SDL
 * ---------------------------------------------------------------- */
static void setColor(SDL_Renderer* ren, Color c) {
    SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a);
}

static void fillRect(SDL_Renderer* ren, int x, int y,
                     int w, int h, Color c)
{
    setColor(ren, c);
    SDL_Rect r{ x, y, w, h };
    SDL_RenderFillRect(ren, &r);
}

static void fillCircle(SDL_Renderer* ren, int cx, int cy,
                       int rad, Color c)
{
    setColor(ren, c);
    for (int dy = -rad; dy <= rad; dy++)
        for (int dx = -rad; dx <= rad; dx++)
            if (dx*dx + dy*dy <= rad*rad)
                SDL_RenderDrawPoint(ren, cx+dx, cy+dy);
}

/* ----------------------------------------------------------------
 * Classe Paddle
 * ---------------------------------------------------------------- */
class Paddle {
public:
    float x, y;
    int   w, h;
    Color color;

    Paddle(float px, float py, Color col)
        : x(px), y(py), w(PADDLE_W), h(PADDLE_H), color(col) {}

    void moveUp()   { y -= PADDLE_SPEED; if (y < 0) y = 0; }
    void moveDown() { y += PADDLE_SPEED;
                      if (y + h > WIN_H) y = (float)(WIN_H - h); }

    SDL_Rect rect() const {
        return SDL_Rect{ (int)x, (int)y, w, h };
    }

    void draw(SDL_Renderer* ren) const {
        fillRect(ren, (int)x, (int)y, w, h, color);
        /* Reflet lumineux */
        Color bright = {
            (Uint8)std::min(255, (int)color.r + 60),
            (Uint8)std::min(255, (int)color.g + 60),
            (Uint8)std::min(255, (int)color.b + 60),
            200
        };
        fillRect(ren, (int)x, (int)y, 3, h, bright);
    }
};

/* ----------------------------------------------------------------
 * Classe Ball
 * ---------------------------------------------------------------- */
class Ball {
public:
    float x, y, vx, vy;
    int   radius;

    struct TrailPoint { float x, y; };
    std::vector<TrailPoint> trail;
    static const int TRAIL_LEN = 10;

    Ball() : x(WIN_W/2.0f), y(WIN_H/2.0f),
             vx(0), vy(0), radius((int)BALL_RADIUS) {
        trail.reserve(TRAIL_LEN);
    }

    void launch() {
        float angle = ((rand() % 61) - 30) * 3.14159f / 180.0f;
        int   dir   = (rand() % 2) ? 1 : -1;
        vx = dir * BALL_SPEED_0 * std::cos(angle);
        vy = BALL_SPEED_0 * std::sin(angle);
    }

    void reset() {
        x = WIN_W / 2.0f;
        y = WIN_H / 2.0f;
        vx = vy = 0;
        trail.clear();
    }

    void update() {
        trail.push_back({ x, y });
        if ((int)trail.size() > TRAIL_LEN)
            trail.erase(trail.begin());

        x += vx;
        y += vy;

        if (y - radius < 0)      { y = (float)radius;        vy =  std::abs(vy); }
        if (y + radius > WIN_H)  { y = WIN_H - (float)radius; vy = -std::abs(vy); }
    }

    bool collide(const Paddle& p) const {
        SDL_Rect r  = p.rect();
        float nearX = std::max((float)r.x,
                      std::min(x, (float)(r.x + r.w)));
        float nearY = std::max((float)r.y,
                      std::min(y, (float)(r.y + r.h)));
        float dx = x - nearX, dy = y - nearY;
        return (dx*dx + dy*dy) <= (float)(radius * radius);
    }

    void bounceOffPaddle(const Paddle& p) {
        SDL_Rect r    = p.rect();
        float center  = r.y + r.h / 2.0f;
        float rel     = (y - center) / (r.h / 2.0f);
        float angle   = rel * 60.0f * 3.14159f / 180.0f;
        float speed   = std::sqrt(vx*vx + vy*vy) + BALL_ACCEL;
        if (speed > BALL_MAX_SPD) speed = BALL_MAX_SPD;
        float dir     = (vx > 0) ? -1.0f : 1.0f;
        vx = dir * speed * std::cos(angle);
        vy = speed * std::sin(angle);
    }

    void draw(SDL_Renderer* ren) const {
        /* Traînée */
        for (int i = 0; i < (int)trail.size(); i++) {
            float alpha = (float)(i + 1) / trail.size();
            int   rad   = (int)(radius * alpha * 0.7f);
            if (rad < 1) rad = 1;
            Color tc    = C_TRAIL;
            tc.a        = (Uint8)(80 * alpha);
            fillCircle(ren, (int)trail[i].x, (int)trail[i].y, rad, tc);
        }
        fillCircle(ren, (int)x, (int)y, radius, C_BALL);
        /* Reflet */
        fillCircle(ren, (int)x - radius/3, (int)y - radius/3,
                   std::max(1, radius/3), {255, 255, 200, 180});
    }
};

/* ----------------------------------------------------------------
 * États du jeu
 * ---------------------------------------------------------------- */
enum class GameState { WAITING, PLAYING, SCORED, GAMEOVER };

/* ----------------------------------------------------------------
 * Classe Game
 * ---------------------------------------------------------------- */
class Game {
public:
    Game()
        : p1(PADDLE_MARGIN,
             WIN_H/2.0f - PADDLE_H/2.0f, C_P1),
          p2(WIN_W - PADDLE_MARGIN - PADDLE_W,
             WIN_H/2.0f - PADDLE_H/2.0f, C_P2),
          score1(0), score2(0),
          state(GameState::WAITING),
          winner(0), pauseTimer(0),
          ren(nullptr), win(nullptr)
    {}

    ~Game() { cleanup(); }

    bool init() {
        srand((unsigned)time(nullptr));
        if (SDL_Init(SDL_INIT_VIDEO) != 0) return false;

        win = SDL_CreateWindow(
            "Ping Pong — 2 Joueurs",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            WIN_W, WIN_H, SDL_WINDOW_SHOWN
        );
        if (!win) return false;

        ren = SDL_CreateRenderer(win, -1,
            SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!ren) return false;

        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        return true;
    }

    void run() {
        const Uint32 frameMs = 1000 / FPS;
        bool running = true;
        SDL_Event e;

        while (running) {
            Uint32 start = SDL_GetTicks();

            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_QUIT) running = false;
                handleEvent(e);
            }

            update();
            render();

            Uint32 elapsed = SDL_GetTicks() - start;
            if (elapsed < frameMs) SDL_Delay(frameMs - elapsed);
        }
    }

private:
    Paddle    p1, p2;
    Ball      ball;
    int       score1, score2;
    GameState state;
    int       winner;
    int       pauseTimer;
    SDL_Renderer* ren;
    SDL_Window*   win;

    /* ---- Événements ---- */
    void handleEvent(const SDL_Event& e) {
        if (e.type != SDL_KEYDOWN) return;
        SDL_Keycode k = e.key.keysym.sym;

        if (k == SDLK_ESCAPE) { SDL_Event q; q.type = SDL_QUIT;
                                 SDL_PushEvent(&q); }

        if (state == GameState::WAITING &&
            (k == SDLK_SPACE || k == SDLK_RETURN)) {
            ball.launch();
            state = GameState::PLAYING;
        }

        if (state == GameState::GAMEOVER && k == SDLK_r)
            resetGame();
    }

    /* ---- Logique ---- */
    void update() {
        const Uint8* keys = SDL_GetKeyboardState(nullptr);

        if (state == GameState::PLAYING ||
            state == GameState::WAITING) {
            if (keys[SDL_SCANCODE_W])    p1.moveUp();
            if (keys[SDL_SCANCODE_S])    p1.moveDown();
            if (keys[SDL_SCANCODE_UP])   p2.moveUp();
            if (keys[SDL_SCANCODE_DOWN]) p2.moveDown();
        }

        if (state == GameState::SCORED) {
            if (--pauseTimer <= 0) {
                ball.reset();
                state = GameState::WAITING;
            }
            return;
        }

        if (state != GameState::PLAYING) return;

        ball.update();

        /* Collisions raquettes */
        if (ball.vx < 0 && ball.collide(p1)) {
            ball.bounceOffPaddle(p1);
            ball.x = p1.x + p1.w + ball.radius + 1.0f;
        }
        if (ball.vx > 0 && ball.collide(p2)) {
            ball.bounceOffPaddle(p2);
            ball.x = p2.x - ball.radius - 1.0f;
        }

        /* Points */
        if (ball.x - ball.radius < 0)      addPoint(2);
        if (ball.x + ball.radius > WIN_W)  addPoint(1);
    }

    void addPoint(int player) {
        if (player == 1) score1++;
        else             score2++;

        if (score1 >= WIN_SCORE) { winner = 1; state = GameState::GAMEOVER; }
        else if (score2 >= WIN_SCORE) { winner = 2; state = GameState::GAMEOVER; }
        else { state = GameState::SCORED; pauseTimer = FPS * 2; }
    }

    void resetGame() {
        score1 = score2 = 0;
        winner = 0;
        ball.reset();
        p1.y = WIN_H/2.0f - PADDLE_H/2.0f;
        p2.y = WIN_H/2.0f - PADDLE_H/2.0f;
        state = GameState::WAITING;
    }

    /* ---- Rendu ---- */
    void render() {
        setColor(ren, C_BG);
        SDL_RenderClear(ren);

        drawNet();
        p1.draw(ren);
        p2.draw(ren);
        ball.draw(ren);
        drawScore();

        if (state == GameState::WAITING)
            drawMessage("ESPACE pour commencer",
                        WIN_H/2 + 50, C_WHITE);

        if (state == GameState::GAMEOVER)
            drawGameOver();

        SDL_RenderPresent(ren);
    }

    void drawNet() {
        int segH = 20, segGap = 10;
        int nx   = WIN_W/2 - 2;
        for (int y = 0; y < WIN_H; y += segH + segGap)
            fillRect(ren, nx, y, 4, segH, C_NET);
    }

    void drawScore() {
        int scale = 8;
        /* Score joueur 1 — à gauche du centre */
        int w1 = (int)(std::to_string(score1).size()) * 6 * scale;
        drawNumber(ren, score1, WIN_W/2 - 30 - w1, 20, scale, C_P1);
        /* Score joueur 2 — à droite du centre */
        drawNumber(ren, score2, WIN_W/2 + 30,       20, scale, C_P2);
    }

    /* Affiche un texte simple en pixels 4x */
    void drawMessage(const char* msg, int y, Color c) {
        /* On affiche les chiffres; pour du texte simple on dessine
           une barre de couleur avec le score ou on utilise des
           rectangles basiques comme indicateur visuel */
        (void)msg;
        /* Ligne décorative clignotante */
        Uint32 t = SDL_GetTicks();
        if ((t / 500) % 2 == 0) {
            fillRect(ren, WIN_W/2 - 140, y, 280, 4, c);
            fillRect(ren, WIN_W/2 - 100, y + 10, 200, 4, c);
        }
    }

    void drawGameOver() {
        /* Fond semi-transparent */
        setColor(ren, {0, 0, 0, 160});
        SDL_Rect ov{ 0, 0, WIN_W, WIN_H };
        SDL_RenderFillRect(ren, &ov);

        Color wc = (winner == 1) ? C_P1 : C_P2;

        /* "JOUEUR X GAGNE" en gros pixels */
        int scale = 10;
        /* "P" + numéro du gagnant */
        drawDigit(ren, winner, WIN_W/2 - 25, WIN_H/2 - 80,
                  scale, wc);

        /* Bordure décorative autour du chiffre */
        setColor(ren, wc);
        SDL_Rect border{ WIN_W/2 - 50, WIN_H/2 - 100, 100, 90 };
        SDL_RenderDrawRect(ren, &border);

        /* "R pour rejouer" — barre clignotante */
        Uint32 t = SDL_GetTicks();
        if ((t / 600) % 2 == 0) {
            fillRect(ren, WIN_W/2 - 80, WIN_H/2 + 30,
                     160, 5, C_WHITE);
            fillRect(ren, WIN_W/2 - 50, WIN_H/2 + 44,
                     100, 5, C_WHITE);
        }
    }

    void cleanup() {
        if (ren) SDL_DestroyRenderer(ren);
        if (win) SDL_DestroyWindow(win);
        SDL_Quit();
    }
};

/* ----------------------------------------------------------------
 * main
 * ---------------------------------------------------------------- */
int main() {
    Game game;
    if (!game.init()) {
        SDL_Log("Erreur init: %s", SDL_GetError());
        return 1;
    }
    game.run();
    return 0;
}
