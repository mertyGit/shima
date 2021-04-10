#ifndef SHIMA_H
#define SHIMA_H
#include "sil.h"

/* game states */
#define SELECTPUZZLE   1
#define SOLVINGPUZZLE  2
#define SOLVED         3
#define WARNBACK       4
#define WARNRESTART    5

#define PARSEKEYWORD   0
#define PARSESTART     1
#define PARSEGOAL      2

#define MOVEALL        0
#define MOVESHORTEST   1
#define MOVELONGEST    2


#define PUZZLEDIR "puzzles/"

#define PIECECODES "_.+ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"

typedef struct _PIECE {
  int code;
  int width;
  int height;
  BYTE up;
  BYTE right;
  BYTE down;
  BYTE left;
  int origx;
  int origy;
  int x;
  int y;
} PIECE ;

typedef struct _PUZZLE {
  char title[256];
  char goal[256];
  char creator[256];
  char preview[256];
  char background[256];
  char pieces[256];
  int  posx;
  int  posy;
  BYTE start[100][100];
  BYTE finish[100][100];
  BYTE pos[100][100];
  int difficulty;
  int maxx;
  int maxy;
  int shufflefirst;
  int movement;
  int pw;
  int ph;
  SILLYR *pieceLyr[65]; /* max. amount of pieces = length of PIECECODES */
  PIECE piece[65];
} PUZZLE ;

#endif
