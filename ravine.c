#include <string.h>

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
  unsigned lsb = lfsr & 1;   /* Get LSB (x.e., the output bit). */
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

byte newframe[28][32];

void screen_flip() {
  // implemented in assembler - c equivalent is:
  //   memcpy(cellram, newframe, sizeof(newframe));
__asm
  LD HL, #_newframe ; load newframe into HL register
  LD DE, #_cellram  ; load cellram into DE register
  LD BC, #0x0380    ; set BC register to length of cellram
  LDIR              ; copies HL to DE for BC bytes
__endasm;
}

void initialise_walls() {
  byte x = Y_MAX;
  word x1_movement;
  word x2_movement;
  Walls walls;

  walls.x1 = 5;
  walls.x2 = 20;

  while (x >= Y_MIN) {
    x1_movement = rand();
    x2_movement = rand();

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

    newframe[walls.x1][x] = WALL;
    newframe[walls.x2][x] = WALL;

    x--;
  }
}

void initialise_player() {
  player.x = 14;
  player.y = Y_MIN;
  newframe[player.x][player.y] = SHIP;
}

void draw_box() {
  byte x = 0, y = 0, x2 = 27, y2 = 31;

  byte x1 = x;
  newframe[x][y] = BOX_CHARS[2];
  newframe[x2][y] = BOX_CHARS[3];
  newframe[x][y2] = BOX_CHARS[0];
  newframe[x2][y2] = BOX_CHARS[1];
  while (++x < x2) {
    newframe[x][y] = BOX_CHARS[5];
    newframe[x][y2] = BOX_CHARS[4];
  }
  while (++y < y2) {
    newframe[x1][y] = BOX_CHARS[6];
    newframe[x2][y] = BOX_CHARS[7];
  }
}

void handle_player_input() {
  if (LEFT1 && player.x > X_MIN) {
    newframe[player.x][player.y] = ' ';
    player.x -= 1;
    newframe[player.x][player.y] = SHIP;
    //delay(40);
  } else if (RIGHT1 && player.x < X_MAX) {
    newframe[player.x][player.y] = ' ';
    player.x += 1;
    newframe[player.x][player.y] = SHIP;
    //delay(40);
  } else if (UP1 && player.y < Y_MAX) {
    newframe[player.x][player.y] = ' ';
    player.y += 1;
    newframe[player.x][player.y] = SHIP;
    //delay(40);
  } else if (DOWN1 && player.y > Y_MIN) {
    newframe[player.x][player.y] = ' ';
    player.y -= 1;
    newframe[player.x][player.y] = SHIP;
    //delay(40);
  }
}

void game_loop() {
  word x1_movement, x2_movement;
  Walls prev, new;
  byte x, y;
  byte exit_early_counter = 0;

  while (1) {
    x1_movement = rand();
    x2_movement = rand();

    prev.x1 = prev.x2 = new.x1 = new.x2 = 0;

    // move all rows down the screen by one
    for (x = X_MIN + 1; x <= X_MAX; x++) {
      for (y = Y_MIN; y <= Y_MAX + 1; y++) {
        // if the current cell is a wall, 'move' it
        // one row down
        if (getchar(x, y) == WALL) {
          // if we've reached the top of the screen,
          // grab the positions of the walls, so we
          // can generate a new row later
          if (y == Y_MAX) {
            if (prev.x1 == 0) {
              prev.x1 = x;
            } else {
              prev.x2 = x;
            }
          }

          // erase walls on current row,
          // and set them on row below
          newframe[x][y] = ' ';
          if (y > Y_MIN) {
            newframe[x][y - 1] = WALL;
          }

          // collision-detection
          if (player.x == x && player.y == y - 1) {
            for (x = 0; x < 40; x++) {
              palette = 2;
              delay(10);
              palette = 1;
              delay(10);
            }
            return;
          }

          // exit early if both walls have been rendered,
          // and we've subsequently gone through one more
          // loop to clean up the cell to the right
          if (exit_early_counter++ == 2) {
            exit_early_counter++;
          } else if (exit_early_counter++ == 3) {
            break;
          }
        }
      }
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

    newframe[new.x1][Y_MAX] = WALL;
    newframe[new.x2][Y_MAX] = WALL;

    // get user input and draw border
    handle_player_input();
    draw_box();

    // flip the screen
    screen_flip();
  }
}

void main() {
  palette = 1;

  memset(cellram, 0, sizeof(cellram));
  screen_flip();
  memcpy(tileram, font8x8, sizeof(font8x8));

  for (byte x = 0; x < 28; x++) {
    for (byte y = 0; y < 32; y++) {
      newframe[x][y] = ' ';
    }
  }

  draw_box();
  initialise_walls();
  initialise_player();
  screen_flip();

  game_loop();

  while (1);
}
