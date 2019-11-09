#include "stdafx.h"
#include "svga/svga.h"
#include "common.h"

#include <vector>

char NetColor[] = { 255, 205, 0, 0 };
char BallColor[] = { 255, 255, 255, 0 };
char BrickColor[] = { 
    64, 64, 128, 0,
    128, 128, 192, 0,
    200, 215, 255, 0
};
char BorderColor[4] = { 128, 128, 128, 0 };
char FieldColor[4] = { 160, 80, 128, 0 };
char FlashColor[4] = { 255, 255, 255, 0 };

struct Point { int x, y;};
struct PointF { float x, y;};
struct Brick { Point p; int State;};

std::vector<Brick> Bricks;
Point Ball;
PointF BallF;
PointF BallDir;
Point FieldFrom;
Point FieldTo;
Point Net;
PointF NetF;
const Point NET_SIZE{ 100, 5 };
const float QUANT = 0.005;
const Point BRICK_SIZE = { 20, 20 };
const Point BALL_SIZE = { 6, 6 };

//This function update full screen from scrptr. The array should be at least sv_height*scrpitch bytes size;
void w32_update_screen(void *scrptr,unsigned scrpitch);

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
extern unsigned int sv_width,sv_height;

//These functions called from another thread, when a button is pressed or released
void win32_key_down(unsigned k){
  if(k==VK_F1) game_quited=true;
}
void win32_key_up(unsigned){}

//This is default fullscreen shadow buffer. You can use it if you want to.
static unsigned *shadow_buf=NULL;

void ApplyDirection(PointF& ballF, Point& ball, PointF& dir);
void Flash();

void BallToNet(PointF & ballF, PointF& dir, Point ball) {
    if (ballF.y + BALL_SIZE.y > Net.y 
        && ballF.x >= Net.x && ballF.x + BALL_SIZE.x <= Net.x + NET_SIZE.x) {
        dir.y = -dir.y;
        dir.x = 3 * (BallF.x + BALL_SIZE.x / 2.0 - Net.x - NET_SIZE.x / 2.0) / NET_SIZE.x;
        ApplyDirection(ballF, ball, dir);
    }
}

void init_game(){
    BallF = { 250, 350 };
    BallDir = { 0.7, -1 };
    Ball = { BallF.x, BallF.y };
    FieldFrom = { 50, 50 };
    FieldTo = { sv_width - 50, sv_height - 50 };
    NetF.x = (FieldFrom.x + FieldTo.x - NET_SIZE.x) / 2;
    NetF.y = (FieldTo.y - 2 * NET_SIZE.y);
    Bricks.clear();
    const int nx = (FieldTo.x - FieldFrom.x) / BRICK_SIZE.x, ny = 5;
    const int ax = (FieldTo.x - FieldFrom.x) % BRICK_SIZE.x, ay = 0;
    for (int y = 0; y < ny; ++y)
    for (int x = 0; x < nx; ++x) {
        Bricks.push_back({ { x * BRICK_SIZE.x + FieldFrom.x + ax, 
            y * BRICK_SIZE.y + FieldFrom.y + ay}, 3 });
    }
    if (!shadow_buf)
        shadow_buf=new unsigned [sv_width*sv_height];
    Flash();

}

void close_game(){
  if(shadow_buf) delete shadow_buf;
  shadow_buf=NULL;
}

void draw_rect(const Point& from, const Point& to, char* color) {
    for (int y = from.y; y < to.y; ++y)
    for (int x = from.x; x < to.x; ++x)
        memcpy((char*)shadow_buf + (y * 4 * sv_width + x * 4), color, 4);
}

void draw_rect(const Point& from, const Point& to, char c) {
    char color[4] = { c, 0, 0, 0 };
    for (int y = from.y; y < to.y; ++y)
    for (int x = from.x; x < to.x; ++x) {
        memcpy((char*)shadow_buf + (y * 4 * sv_width + x * 4), color, 4);
    }
}

void Flash() {
    draw_rect(Point{0, 0}, Point{sv_width, sv_height}, FlashColor);
    w32_update_screen(shadow_buf, sv_width * 4);
    Sleep(500);
}

void draw_ball(Point& from) {
    draw_rect(Point{ from.x, from.y },
        Point{ from.x + BALL_SIZE.x, from.y + BALL_SIZE.y}, BallColor);
}

void draw_net(Point& from) {
    draw_rect(Point{ from.x, from.y },
        Point{ from.x + NET_SIZE.x, from.y + NET_SIZE.y }, NetColor);
}


void draw_brick(Brick& brick) {
    Point& from = brick.p;
    draw_rect(from,
        Point{ from.x + BRICK_SIZE.x - 1, from.y + BRICK_SIZE.y - 1}, 
        (char*)BrickColor + (brick.State * 4 - 4));
}
void draw_border() {
    draw_rect(Point{ FieldFrom.x - 5, FieldFrom.y - 5 }, Point{ FieldTo.x + 5, FieldTo.y + 5 }, BorderColor);
    draw_rect(Point{ FieldFrom.x, FieldFrom.y }, Point{ FieldTo.x, FieldTo.y }, FieldColor);
}

//draw the game to screen
void draw_game(){
  if(!shadow_buf)
      return;
  memset(shadow_buf, 0, sv_width*sv_height * 4);
  draw_border();
  draw_net(Net);
  for (auto brick : Bricks) {
      if (brick.State) {
          draw_brick(brick);
      }
  }
  draw_ball(Ball);
  //here you should draw anything you want in to shadow buffer. (0 0) is left top corner
  w32_update_screen(shadow_buf,sv_width*4);
}



int Intersect(const Point& ball, const Point& BALL_SIZE, 
    const Point& brick, const Point& BRICK_SIZE) 
{
    int result = 0;
    if (ball.x + BALL_SIZE.x >= brick.x && ball.y + BALL_SIZE.y >= brick.y
        && brick.x + BRICK_SIZE.x >= ball.x && brick.y + BRICK_SIZE.y >= ball.y)
    {
        
        if ((ball.x + BALL_SIZE.x == brick.x) 
            || (brick.x + BRICK_SIZE.x == ball.x)) result |= 1;
        if ((brick.y + BRICK_SIZE.y == ball.y) 
            || (ball.y + BALL_SIZE.y == brick.y)) result |= 2;

    }
    return result;
}

void ApplyDirection(PointF& ballF, Point& ball, PointF& dir) {
    ballF.x += dir.x; ballF.y += dir.y;
    ball.x = ballF.x; ball.y = ballF.y;
}

void InBound(float& x, const float from, const float to) {
    if (x < from) x = from;
    if (x > to) x = to;
}

//act the game. dt - is time passed from previous act
void act_game(float dt){
    if (gButtons[1]) {
        NetF.x += dt / QUANT;
    }
    if (gButtons[0]) {
        NetF.x -= dt / QUANT;
    }
    InBound(NetF.x, float(FieldFrom.x), float(FieldTo.x - NET_SIZE.x));
    Net.x = NetF.x; Net.y = NetF.y;
    BallToNet(BallF, BallDir, Ball);
    static float t = 0;
    t += dt;
    while (t > QUANT) {
        t -= QUANT;
        ApplyDirection(BallF, Ball, BallDir);
        if (Ball.y + BALL_SIZE.y >= FieldTo.y) {
            init_game();
        }
        if ((Ball.y + BALL_SIZE.y >= FieldTo.y)
            || (Ball.y <= FieldFrom.y))
        {
            BallDir.y = -BallDir.y;
            ApplyDirection(BallF, Ball, BallDir);
        }
        if (Ball.x <= FieldFrom.x || Ball.x + BALL_SIZE.x >= FieldTo.x) {
            BallDir.x = -BallDir.x;
            ApplyDirection(BallF, Ball, BallDir);
        }

        for (size_t i = 0; i < Bricks.size(); ++i) {
            if (Bricks[i].State){
                int intersectResult = Intersect(Ball, BALL_SIZE, Bricks[i].p, BRICK_SIZE);
                if (intersectResult & 1) {
                    BallDir.x = -BallDir.x;
                }
                if (intersectResult & 2) {
                    if (Bricks[i].State)
                    BallDir.y = -BallDir.y;
                }
                if (intersectResult) {
                    ApplyDirection(BallF, Ball, BallDir);
                    --Bricks[i].State;
                }
            }
        }
    }
}
