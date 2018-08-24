﻿#include <string.h>

typedef unsigned char byte;
typedef unsigned short word;

// PLATFORM DEFINITION

__sfr __at (0x0) input0;
__sfr __at (0x1) input1;
__sfr __at (0x2) input2;
__sfr __at (0x3) input3;

__sfr __at (0x1) ay8910_reg;
__sfr __at (0x2) ay8910_data;
__sfr __at (0x40) palette;

byte __at (0xe000) cellram[28][32];
byte __at (0xe800) tileram[256][8];

#define LEFT1 !(input1 & 0x10)
#define RIGHT1 !(input1 & 0x20)
#define UP1 !(input1 & 0x40)
#define DOWN1 !(input1 & 0x80)
#define FIRE1 !(input2 & 0x20)
#define COIN1 (input3 & 0x8)
#define START1 !(input2 & 0x10)
#define START2 !(input3 & 0x20)
#define TIMER500HZ (input2 & 0x8)

inline void set8910(byte reg, byte data) {
  ay8910_reg = reg;
  ay8910_data = data;
}

typedef enum {
  AY_PITCH_A_LO, AY_PITCH_A_HI,
  AY_PITCH_B_LO, AY_PITCH_B_HI,
  AY_PITCH_C_LO, AY_PITCH_C_HI,
  AY_NOISE_PERIOD,
  AY_ENABLE,
  AY_ENV_VOL_A,
  AY_ENV_VOL_B,
  AY_ENV_VOL_C,
  AY_ENV_PERI_LO, AY_ENV_PERI_HI,
  AY_ENV_SHAPE
} AY8910Register;


// STARTUP CODE

void main();
void gsinit();

// start routine @ 0x0
void start() {
__asm
  	LD    SP,#0xE800 ; set up stack pointer
        DI		 ; disable interrupts
__endasm;
  	gsinit();
	main();
}

#define INIT_MAGIC 0xdeadbeef
static long is_initialized = INIT_MAGIC;

// set initialized portion of global memory
// by copying INITIALIZER area -> INITIALIZED area
void gsinit() {
  // already initialized? skip it
  if (is_initialized == INIT_MAGIC)
    return;
__asm
; copy initialized data to RAM
	LD    BC, #l__INITIALIZER
	LD    A, B
	LD    DE, #s__INITIALIZED
	LD    HL, #s__INITIALIZER
      	LDIR
__endasm;
}

// PLATFORM CODE

static word lfsr = 1;
word rand() {
  unsigned lsb = lfsr & 1;   /* Get LSB (i.e., the output bit). */
  lfsr >>= 1;                /* Shift register */
  if (lsb) {                 /* If the output bit is 1, apply toggle mask. */
    lfsr ^= 0xB400u;
  }
  return lfsr;
}

void delay(byte msec) {
  while (msec--) {
    while (TIMER500HZ != 0) lfsr++;
    while (TIMER500HZ == 0) lfsr++;
  }
}

#define PE(fg,bg) (((fg)<<5) | ((bg)<<1))

const byte __at (0x4000) color_prom[32] = {
  0xe0,0x60,0x20,0x60, 0xc0,0x60,0x40,0xc0,
  0x20,0x40,0x60,0x80, 0xa0,0xc0,0xe0,0x0e,
  0xe0,0xe0,0xe0,0xe0, 0x60,0x60,0x60,0x60,
  0xe0,0xe0,0xe0,0xe0, 0xe0,0xe0,0xe0,0xe0,
};

/*const byte __at (0x4000) color_prom[32] = {
  PE(7,0),PE(3,0),PE(1,0),PE(3,0),PE(6,0),PE(3,0),PE(2,0),PE(6,0),
  PE(7,0),PE(3,0),PE(1,0),PE(3,0),PE(6,0),PE(3,0),PE(2,0),PE(6,0),
  PE(7,0),PE(3,0),PE(1,0),PE(3,0),PE(6,0),PE(3,0),PE(2,0),PE(6,0),
  PE(7,0),PE(3,0),PE(1,0),PE(3,0),PE(6,0),PE(3,0),PE(2,0),PE(6,0),
};*/

#define LOCHAR 0x0
#define HICHAR 0xff

#define CHAR(ch) (ch-LOCHAR)

void clrscr() {
  memset(cellram, CHAR(' '), sizeof(cellram));
}

byte getchar(byte x, byte y) {
  return cellram[x][y];
}

void putchar(byte x, byte y, byte attr) {
  cellram[x][y] = attr;
}

void putstring(byte x, byte y, const char* string) {
  while (*string) {
    putchar(x++, y, CHAR(*string++));
  }
}

// PC font (code page 437)
//#link "cp437.c"
extern byte font8x8[0x100][8];

const char BOX_CHARS[8] = { 218, 191, 192, 217, 196, 196, 179, 179 };

void draw_box(byte x, byte y, byte x2, byte y2, const char* chars) {
  byte x1 = x;
  putchar(x, y, chars[2]);
  putchar(x2, y, chars[3]);
  putchar(x, y2, chars[0]);
  putchar(x2, y2, chars[1]);
  while (++x < x2) {
    putchar(x, y, chars[5]);
    putchar(x, y2, chars[4]);
  }
  while (++y < y2) {
    putchar(x1, y, chars[6]);
    putchar(x2, y, chars[7]);
  }
}

// GAME CODE

#define SHIP 6
#define WALL 219

#define X_MIN 1
#define X_MAX 26
#define Y_MIN 1
#define Y_MAX 30

typedef struct {
  byte x;
  byte y;
} Player;

typedef struct {
  byte x1;
  byte x2;
} Walls;

Player player;

void handle_player_input() {
  if (LEFT1 && player.x > X_MIN) {
    putchar(player.x, player.y, ' ');
    player.x -= 1;
    delay(40);
  } else if (RIGHT1 && player.x < X_MAX) {
    putchar(player.x, player.y, ' ');
    player.x += 1;
    delay(40);
  } else if (UP1 && player.y < Y_MAX) {
    putchar(player.x, player.y, ' ');
    player.y += 1;
    delay(40);
  } else if (DOWN1 && player.y > Y_MIN) {
    putchar(player.x, player.y, ' ');
    player.y -= 1;
  }

  putchar(player.x, player.y, SHIP);
}

void initialise_walls() {
  byte i = Y_MAX;
  word x1_movement;
  word x2_movement;
  Walls walls;

  walls.x1 = 5;
  walls.x2 = 20;

  while (i >= Y_MIN) {
    x1_movement = rand();
    x2_movement = rand();

    putchar(walls.x1, i, ' ');
    putchar(walls.x2, i, ' ');

    if (x1_movement & 0 == 0 && walls.x1 > X_MIN + 1) {
      walls.x1 -= 1;
    } else if (walls.x1 < walls.x2 - 2) {
      walls.x1 += 1;
    }

    if (x2_movement & 0 == 0 && walls.x2 > walls.x1 + 2) {
      walls.x2 -= 1;
    } else if (walls.x2 < X_MAX - 1) {
      walls.x2 += 1;
    }

    putchar(walls.x1, i, WALL);
    putchar(walls.x2, i, WALL);

    i--;
  }
}

void game_loop() {
  word x1_movement, x2_movement;
  Walls prev, new;
  byte i, j;

  initialise_walls();

  player.x = 14;
  player.y = 5;

  while (1) {
    x1_movement = rand();
    x2_movement = rand();

    prev.x1 = prev.x2 = new.x1 = new.x2 = 0;

    // move existing rows down
    for (i = X_MIN + 1; i <= X_MAX; i++) {
      for (j = Y_MIN + 1; j <= Y_MAX + 1; j++) {
        if (getchar(i, j - 1) == WALL) {
          putchar(i, j - 1, ' ');

          if (j == Y_MAX + 1) {
            if (prev.x1 == 0) {
              prev.x1 = i;
            } else {
              prev.x2 = i;
            }
          }
        }

        if (getchar(i, j) == WALL) {
          putchar(i, j - 1, WALL);
          if (player.x == i && player.y == j - 1) {
            for (i = 0; i < 40; i++) {
              palette = 2;
              delay(10);
              palette = 1;
              delay(10);
            }
            return;
          }
        }
      }

      handle_player_input();
    }

    // new row
    if (x1_movement & 0 == 0 && prev.x1 > X_MIN + 1) {
      new.x1 = prev.x1 - 1;
    } else if (prev.x1 < prev.x2 - 2) {
      new.x1 = prev.x1 + 1;
    } else {
      new.x1 = prev.x1;
    }

    if (x2_movement & 0 == 0 && prev.x2 > prev.x1 + 2) {
      new.x2 = prev.x2 - 1;
    } else if (prev.x2 < X_MAX - 1) {
      new.x2 = prev.x2 + 1;
    } else {
      new.x2 = prev.x2;
    }

    putchar(new.x1, Y_MAX, WALL);
    putchar(new.x2, Y_MAX, WALL);
  }
}

void main() {
  palette = 1;
  memset(cellram, 0, sizeof(cellram));
  memcpy(tileram, font8x8, sizeof(font8x8));
  draw_box(0, 0, 27, 31, BOX_CHARS);
  game_loop();
  while (1);
}