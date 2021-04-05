#ifndef SHIMA_H
#define SHIMA_H

#define SELECTPUZZLE   1
#define SOLVINGPUZZLE  2
#define SOLVED         3
#define PARSEKEYWORD   0
#define PARSESTART     1
#define PARSEGOAL      2


typedef struct _PUZZLE {
  char title[256];
  char description[256];
  char creator[256];
  char preview[256];
  char background[256];
  char pieces[256];
  int  posx;
  int  posy;
  char start[100][100];
  char goal[100][100];
  char pos[100][100];
  int maxx;
  int maxy;
  int shufflefirst;
} PUZZLE ;

void showInfo(char *);
void showPreview(SILLYR *, char *);
void insertIni(int,char *);
void getPuzzles();
void puzzleSelection();
UINT lightUp(SILEVENT *);
UINT keyhandler(SILEVENT *);
UINT nextPuzzle(SILEVENT *);
UINT previousPuzzle(SILEVENT *);
void initBackground();
void initPuzzleSelect();
void hidePuzzleSelect();
void puzzleSolving();


#endif
