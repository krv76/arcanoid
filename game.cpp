#include "stdafx.h"
#include "svga/svga.h"
#include "common.h"

#include <vector>

unsigned NetColor = 0x0000cfff;
unsigned BallColor = 0x00ffffff;
unsigned BrickColor[] = { 0x00a04060, 0x00c08080, 0x00ffd7c8 };
unsigned BorderColor = 0x00808080;
unsigned FieldColor = 0x008050a0;
unsigned FlashColor = 0x00a04080;

struct Point { int x, y;};
struct PointF { float x, y;};

const Point NET_SIZE{ 100, 5 };
const float QUANT = 0.005f;
const Point BRICK_SIZE = { 20, 20 };
const Point BALL_SIZE = { 6, 9 };


void draw_rect(const Point& from, const Point& to, unsigned color);

Point iPoint(PointF& p) {
    return Point{ int(p.x), int(p.y) };
}

struct Field {
    Point from, to;
    int border;
    void Init() {
        from = { 50, 50 };
        to = { sv_width - 50, sv_height - 50 };
        border = 5;
    }
    void Draw() {
        draw_rect(Point{ from.x - border, from.y - border },
            Point{ to.x + border, to.y + border }, BorderColor);
        draw_rect(from, to, FieldColor);
    }
};

struct Ball {
    PointF from, to, dir;
    void Draw() {
        draw_rect(iPoint(from), iPoint(to), BallColor);
    }
    void ApplyDirection() {
        from.x += dir.x;
        from.y += dir.y;
        to.x += dir.x;
        to.y += dir.y;
    }
    void Init() {
        from = { 250.f, 350.f };
        to = { 250.f + BALL_SIZE.x, 350.f + BALL_SIZE.y };
        dir = { 0.7, -1 };
    }
};

struct Net {
    PointF from, to, dir;
    void Draw() {
        draw_rect(iPoint(from), iPoint(to), NetColor);
    }
    void Init(Field& field) {
        from.x = (field.to.x + field.from.x - NET_SIZE.x) / 2;
        to.x = from.x + NET_SIZE.x;
        from.y = (field.to.y - 2 * NET_SIZE.y);
        to.y = from.y + NET_SIZE.y;
    }
    void Move(double dx, Field& field) {
        from.x += dx;
        to.x += dx;
        if (to.x >= field.to.x) {
            to.x = field.to.x;
            from.x = to.x - NET_SIZE.x;
        }
        if (from.x < field.from.x) {
            from.x = field.from.x;
            to.x = from.x + NET_SIZE.x;
        }
    }
};

struct Brick { 
    Point from, to; 
    int State; 
    void Draw() {
        draw_rect(from, Point{to.x - 1, to.y - 1}, BrickColor[State - 1]);
    }
};

std::vector<Brick> Bricks;
Ball ball;
Field field;
Net net;
double time;
bool skipAct;
//This function update full screen from scrptr. The array should be at least sv_height*scrpitch bytes size;
void w32_update_screen(void *scrptr, unsigned scrpitch);

//If this variable sets to true - game will quit

extern bool game_quited;

// these variables provide access to joystick and joystick buttons
// In this version joystick is simulated on Arrows and Z X buttons

// [0]-X axis (-501 - left; 501 - right)
// [1]-Y axis (-501 - left; 501 - right)
extern int gAxis[2];
//0 - not pressed; 1 - pressed
extern int gButtons[6];

//sv_width and sv_height variables are width and height of screen
extern unsigned int sv_width, sv_height;

//These functions called from another thread, when a button is pressed or released
void win32_key_down(unsigned k){
  if(k==VK_F1) game_quited=true;
}
void win32_key_up(unsigned){}

//This is default fullscreen shadow buffer. You can use it if you want to.
static unsigned *shadow_buf=NULL;

void Flash();

void BallToNet(Ball& ball, Net& net) {
    if (ball.to.y > net.from.y && ball.from.x >= net.from.x && ball.to.x <= net.to.x) 
    {
        ball.dir.y = -ball.dir.y;
        ball.dir.x = 2.5 * ((ball.from.x + ball.to.x) / 2
            - (net.from.x + net.to.x) / 2) / (net.to.x - net.from.x);
        ball.ApplyDirection();
    }
}

void init_game(){
    if (!shadow_buf)
        shadow_buf = new unsigned[sv_width*sv_height];
    time = 0;
    Flash();
    ball.Init();
    field.Init();
    net.Init(field);
    skipAct = true;

    Bricks.clear();
    const int nx = (field.to.x - field.from.x) / BRICK_SIZE.x, ny = 5;
    const int ax = field.from.x + (field.to.x - field.from.x) % BRICK_SIZE.x;
    const int ay = 30 + field.from.y;
    for (int y = 0; y < ny; ++y)
    for (int x = 0; x < nx; ++x) {
        Bricks.push_back({ 
            {x * BRICK_SIZE.x + ax, y * BRICK_SIZE.y + ay}, 
            {(x + 1) * BRICK_SIZE.x + ax, (y + 1) * BRICK_SIZE.y + ay},
            rand() % 4 
        });
    }
}

void close_game(){
    if(shadow_buf) 
        delete shadow_buf;
    shadow_buf=NULL;
}

void draw_rect(const Point& from, const Point& to, unsigned color) {
    for (int y = from.y; y < to.y; ++y)
    for (int x = from.x; x < to.x; ++x)
        shadow_buf[y * sv_width + x] = color;
}

void Flash() {
    draw_rect(Point{0, 0}, Point{sv_width, sv_height}, FlashColor);
    w32_update_screen(shadow_buf, sv_width * 4);
    Sleep(10);
    draw_rect(Point{ 0, 0 }, Point{ sv_width, sv_height }, FlashColor);
    w32_update_screen(shadow_buf, sv_width * 4);
    Sleep(500);

}

void draw_net(Point& from) {
    draw_rect(Point{ from.x, from.y },
        Point{ from.x + NET_SIZE.x, from.y + NET_SIZE.y }, NetColor);
}

//draw the game to screen
void draw_game(){
    if(!shadow_buf)
        return;
    memset(shadow_buf, 0, sv_width*sv_height * 4);
    field.Draw();
    net.Draw();
    ball.Draw();
    for (auto brick : Bricks) {
        if (brick.State) {
            brick.Draw();
        }
    }
    //here you should draw anything you want in to shadow buffer. (0 0) is left top corner
    w32_update_screen(shadow_buf,sv_width*4);
}



int Intersect(const Ball& ball, Brick& brick) 
{
    int result = 0;
    if (ball.to.x >= brick.from.x && ball.to.y >= brick.from.y
        && brick.to.x >= ball.from.x && brick.to.y >= ball.from.y)
    {
        if (ball.to.x < brick.from.x + 1 || brick.to.x < ball.from.x + 1) 
            result |= 1;
        if (brick.to.y < ball.from.y + 1 || ball.to.y < brick.from.y + 1) 
            result |= 2;
    }
    return result;
}

//act the game. dt - is time passed from previous act
void act_game(float dt){
    if (skipAct) {
        skipAct = false;
        return;
    }
    if (gButtons[1]) {
        net.Move(dt / QUANT, field);
    }
    if (gButtons[0]) {
        net.Move(-dt / QUANT, field);
    }
    BallToNet(ball, net);
    time += dt;
    while (time > QUANT) {
        time -= QUANT;
        ball.ApplyDirection();
        if (ball.to.y >= field.to.y) {
            Sleep(1000);
            init_game();
        }
        if ((ball.to.y >= field.to.y) || (ball.from.y <= field.from.y))
        {
            ball.dir.y = -ball.dir.y;
            ball.ApplyDirection();
        }
        if (ball.from.x <= field.from.x || ball.to.x >= field.to.x) {
            ball.dir.x = -ball.dir.x;
            ball.ApplyDirection();
        }

        for (size_t i = 0; i < Bricks.size(); ++i) {
            if (Bricks[i].State){
                int intersectResult = Intersect(ball, Bricks[i]);
                if (intersectResult & 1) {
                    ball.dir.x = -ball.dir.x;
                }
                if (intersectResult & 2) {
                    if (Bricks[i].State)
                        ball.dir.y = -ball.dir.y;
                }
                if (intersectResult) {
                    ball.ApplyDirection();
                    --Bricks[i].State;
                }
            }
        }
    }
}

