#include <Adafruit_SSD1306.h>      // Driver library for 'monochrome' 128x64 and 128x32 OLEDs

#define CONTROLLER_1 1
#define CONTROLLER_2 2
#define BUTTON_PIN 5
#define SOUND_PIN 8

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for SSD1306 display connected using software SPI (default case):
#define OLED_MOSI   9
#define OLED_CLK   10
#define OLED_DC    11
#define OLED_CS    12
#define OLED_RESET 13
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT,
  OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);

// From pypaddle

const float SERVE_DELAY = 1.5;
const byte HEIGHT = 3;
const float V = 1./(262-16);
const byte WIDTH = 4;
const float H = 1./(455-80);
const float BAT_HEIGHT = 16*V;
const float BAT_WIDTH = 4*H;
const float BALL_HEIGHT = 4*V;
const float BALL_WIDTH = 4*H;
const float NET_WIDTH = H;
// HSPEEDS in original is represented as two arrays here (HHITS and HSPEEDS)
const byte HHITS[] = { 0,4,12 };
const float HSPEEDS[] = { 0.26,0.39,0.53 };
const byte VLOADS[] = { 0,1,2,3,3,4,5,6 };
const float VSPEEDS[] = { 0.680,0.455,0.228,0,-0.226,-0.462,-0.695 };
const float TOP_GAP = 6*V;
const float BOTTOM_GAP = 4*V;
const float BAT_1_X_START = 48*H;
const float NET_X_START = 176*H;
const float NET_STRIPE_HEIGHT = 4*V;
const float BAT_2_X_START = 304*H;
const float BALL_X_START = NET_X_START + 6*H;
const float SCORE_1_X_START = 48*H;
const float SCORE_2_X_START = 192*H;
const float SCORE_Y_START = 16*V;
const float DIGIT_PIXEL_V = 4*V;
const float DIGIT_PIXEL_H = 4*H;
/*
 * DIGITS in original is represented as bit masks rather than strings to save space.
 * 
 * The segments are labelled as follows:
 * 
 * #A##
 * F  B
 * #  #
 * #G##
 * #  #
 * E  C
 * #  #
 * #D##
 * 
 * Then each digit is encoded as a byte where bits correspond to segments being on or off.
 * 
 * Digit String   GFE DCBA   Hex
 * 0     ABCDEF  0011 1111   3F
 * 1     BC      0000 0110   06
 * 2     ABGED   0101 1011   5B
 * 3     ABGCD   0100 1111   4F
 * 4     FGBC    0110 0110   66
 * 5     AFGCD   0110 1101   6D
 * 6     ACDEFG  0111 1101   7D
 * 7     ABC     0000 0111   07
 * 8     ABCDEFG 0111 1111   7F
 * 9     ABCDFG  0110 1111   6F
 */
const byte SEGMENTS[][4] = {{0,0,3,0},{3,0,3,3},{3,3,3,7},{0,7,3,7},{0,3,0,7},{0,0,0,3},{0,3,3,3}};
const byte DIGITS[10] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F };

// sounds are separated into two variables (original uses tuples)
const unsigned int HIT_SOUND_FREQUENCY = 491;
const unsigned long HIT_SOUND_DURATION = 16;
const unsigned int SCORE_SOUND_FREQUENCY = 246;
const unsigned long SCORE_SOUND_DURATION = 220;
const unsigned int BOUNCE_SOUND_FREQUENCY = 246;
const unsigned long BOUNCE_SOUND_DURATION = 16;
const float JOY_RANGE = 0.7;

const boolean SILENT = true;

const byte WINDOW_SIZE[] = { SCREEN_WIDTH, SCREEN_HEIGHT };
const float SCALE[] = { float(WINDOW_SIZE[1]) * WIDTH / HEIGHT, float(WINDOW_SIZE[1]) };
const byte CENTER[]= { WINDOW_SIZE[0]/2, WINDOW_SIZE[1]/2 };

float clamp(float x, float m, float M) {
    return max(min(x,M),m);
}

// hits is passed in here (unlike original which may have a bug?)
float getHSpeed(int hits) {
  int len = sizeof(HHITS) / sizeof(HHITS[0]);
  float prev = 0;
  for (int i=0; i<len; i++) {
    if (HHITS[i] > hits) {
      return prev;
    }
    prev = HSPEEDS[i];
  }
  return HSPEEDS[len-1];
}

float getVSpeedLoad(float y) {
  int len = sizeof(VLOADS) / sizeof(VLOADS[0]);
  int i = int((y+0.5)*len+0.5);
  if (i<0) {
    return VLOADS[0];
  } else if (i>=len) {
    return VLOADS[len-1];
  } else {
    return VLOADS[i];
  }
}

int sign(float x) {
  if (x < 0) {
    return -1;
  } else if (x > 0) {
    return 1;
  } else {
    return 0;
  }
}

// TODO: getScale for dynamic window sizing

// https://forum.arduino.cc/t/returning-two-values-from-a-function/90068/13
void toScreenXY(float x, float y, int & sx, int & sy) {
    sx = int(0.5+SCALE[0]*(x-0.5)+CENTER[0]);
    sy = int(0.5+SCALE[1]*(y-0.5)+CENTER[1]);
}

void toScreenWH(float w, float h, int & sw, int & sh) {
    sw = int(0.5+SCALE[0]*w);
    sw = max(sw, 1);
    sh = int(0.5+SCALE[1]*h);
    sh = max(sh, 1);
}

class RectSprite {
  protected:
  float _w;
  float _h;
  float _x;
  float _y;
  float _vx;
  float _vy;

  public:
  RectSprite(float w, float h, float x, float y) {
    _w = w;
    _h = h;
    _x = x;
    _y = y;
    _vx = 0.;
    _vy = 0.;
  }

  void updateXY(float dt) {
    _x += dt * _vx;
    _y += dt * _vy;
  }

  // returns 0 if no impact, 1 if hit
  // pos is y position of hit normalized over self
  int hit(RectSprite* target, float & pos) {
    if (abs(_x-target->_x) >= (_w+target->_w) * 0.5) {
      return 0;
    } else if (abs(_y-target->_y) >= (_h+target->_h) * 0.5) {
      return 0;
    }
    pos =  (_y-target->_y) / _h;
    return 1;
  }

  void draw() {
    int sx, sy, sw, sh;
    toScreenXY(_x, _y, sx, sy);
    toScreenWH(_w, _h, sw, sh);
    display.drawRect(sx, sy, sw, sh, WHITE);
  }
};

class Bat : public RectSprite {
  public:
  byte _index;
  char _direction;

  public:
  Bat(byte index) : RectSprite(BAT_WIDTH, BAT_HEIGHT, 0.5, 0.5) {
    _index = index;
    _direction = (index == 0 ? 1 : -1);
    _x = (index == 0 ? BAT_1_X_START : BAT_2_X_START) + _w*0.5;
  }
  
  void setPosition(float offset) {
    _y = (1.-TOP_GAP-BOTTOM_GAP-_h)*0.5*(offset+1) + TOP_GAP + 0.5*_h;
  }
};

boolean bats = false; // whether bats are active or not
Bat bat0 = Bat(0);
Bat bat1 = Bat(1);

class Ball : public RectSprite {
  byte _hits;
  char _serve_direction;
  float _minY;
  float _maxY;
  float _minX;
  float _maxX;
  float _wait;
  byte _load;
 
  public:
  Ball() : RectSprite(BALL_WIDTH, BALL_HEIGHT, 0.4, 0.) {
    byte load = 0;
    char direction = 1;
    _hits = 0;
    _vx = direction*getHSpeed(_hits);
    _minY = _h*0.5;
    _maxY = 1-_h*0.5;
    _minX = _w*0.5;
    _maxX = 1-_w*0.5;
    _serve_direction = 0;
    _wait = 0;
    setLoad(load);
  }

  void setLoad(int load) {
    _load = load;
    _vy = VSPEEDS[load];
  }

  void draw() {
    if (_wait <= 0) {
      RectSprite :: draw();
    }
  }

  void serve() {
    _wait = SERVE_DELAY;
  }

  void reverseVertical() {
    int len = sizeof(VLOADS) / sizeof(VLOADS[0]);
    setLoad(VLOADS[len-1]-_load);
  }

  int updateXY(float dt) {
    if (_wait > 0) {
      _wait -= dt;
      if (_wait <= 0) {
        _x = BALL_X_START - 0.5*_w;
        if (_serve_direction == 0) {
          _serve_direction = sign(_vx);
        }
        _hits = 0;
        _vx = _serve_direction*getHSpeed(_hits);
      }
    }
    
    RectSprite :: updateXY(dt);

    if (_y>_maxY) {
      _y=clamp(_maxY*2-_y,_minY,_maxY);
      reverseVertical();
      sound(BOUNCE_SOUND_FREQUENCY, BOUNCE_SOUND_DURATION);
    } else if (_y<_minY) {
      _y=clamp(_minY*2-_y,_minY,_maxY);
      reverseVertical();      
      sound(BOUNCE_SOUND_FREQUENCY, BOUNCE_SOUND_DURATION);
    }

    if (_wait <= 0) {
      if (bats) {
        float y;
        int impact;
        impact = bat0.hit(this, y);
        if (impact) {
          sound(HIT_SOUND_FREQUENCY, HIT_SOUND_DURATION);
          _hits += 1;
          _vx = bat0._direction * getHSpeed(_hits);
          setLoad(getVSpeedLoad(y));
        }
        impact = bat1.hit(this, y);
        if (impact) {
          sound(HIT_SOUND_FREQUENCY, HIT_SOUND_DURATION);
          _hits += 1;
          _vx = bat1._direction * getHSpeed(_hits);
          setLoad(getVSpeedLoad(y));
        }
      }
    }

    if (_x < _minX) {
      _vx = abs(_vx);
      _x = _minX;
      if (bats && _wait <= 0) {
        // right scores
        _serve_direction = -1;
        _wait = SERVE_DELAY;
        return 1;
      }
    } else if (_x > _maxX) {
      _vx = -abs(_vx);
      _x = _maxX;
      if (bats && _wait <= 0) {
        // left scores
        _serve_direction = 1;
        _wait = SERVE_DELAY;
        return 0;
      }
    }
    return -1;
  }

};

void drawDigit(float x, float y, int n) {
  int x_, y_;
  toScreenXY(x, y, x_, y_);
  int w, h;
  toScreenWH(DIGIT_PIXEL_H, DIGIT_PIXEL_V, w, h);
  byte d = DIGITS[n];
  for (int i=0; i<=7; i++) {
    if (bitRead(d, i)) {
      int sw = (SEGMENTS[i][2]-SEGMENTS[i][0]+1)*w;
      int sh = (SEGMENTS[i][3]-SEGMENTS[i][1]+1)*h;
      int sx = x_+SEGMENTS[i][0]*w;
      int sy = y_+SEGMENTS[i][1]*h;
      display.drawRect(sx, sy, sw, sh, WHITE);
    }
  }
}

void drawScore(float x, float y, int score) {
  float x_ = x+DIGIT_PIXEL_H*4*3;
  while (1) {
    drawDigit(x_,y,score%10);
    score = score / 10;
    if (score == 0) {
      break;
    }
    x_ -= DIGIT_PIXEL_H*8;
  }
}

byte scores[2] = {0,0};
Ball ball = Ball();

float get_axis(int index) {
  int controllerValue = index == 0 ? analogRead(CONTROLLER_1) : analogRead(CONTROLLER_2); 
  return (controllerValue - 512) / 1024.0;
}

float adjustJoystick(float y) {
  y /= JOY_RANGE;
  return clamp(y,-1,1);
}

void attract() {
  bats = false;
  ball.setLoad(0);
}

void start() {
  bats = true;
  scores[0] = 0;
  scores[1] = 0;
  ball.serve();
}

void net() {
    float y = 0;
    int w, h;
    toScreenWH(NET_WIDTH, NET_STRIPE_HEIGHT, w, h);
    int sx, sy;
    while (y < 1.) {
        toScreenXY(NET_X_START, y, sx, sy);
        display.drawRect(sx, sy, w, h, WHITE);
        y += 2*NET_STRIPE_HEIGHT;
    }
}

void drawBoard() {
//    display.clearDisplay();
    net();
}

void sound(unsigned int frequency, unsigned long duration) {
  if (not SILENT) {
    tone(SOUND_PIN, frequency, duration);
  }
}

unsigned long prev;

void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  Serial.begin(9600);
  Serial.println("PONG");
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 128x64)
  display.clearDisplay();

  prev = millis();
}

void loop() {
  if (not bats && digitalRead(BUTTON_PIN) == LOW) {
    start();
  }

  unsigned long now = millis();
  float dt = (now - prev) / 1000.0;
  drawBoard();
  drawScore(SCORE_1_X_START,SCORE_Y_START,scores[0]);
  drawScore(SCORE_2_X_START,SCORE_Y_START,scores[1]);
  if (bats) {
    bat0.setPosition(adjustJoystick(get_axis(0)));
    bat1.setPosition(adjustJoystick(get_axis(1)));
  }
  int edge = ball.updateXY(dt);
  ball.draw();
  if (bats) {
    bat0.draw();
    bat1.draw();
  }
  if (edge != -1 && bats) {
    sound(SCORE_SOUND_FREQUENCY, SCORE_SOUND_DURATION);
    scores[edge] += 1;
    if (scores[edge] == 11) {
      attract();
    }
  }
  display.display();
  display.clearDisplay();

  prev = now;
}

// End from pypaddle
