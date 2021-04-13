#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <dirent.h>
#include <time.h>
#ifdef SIL_W32
  #include <windows.h>
#endif
#include "log.h"
#include "sil.h"
#include "pcg_basic.h"
#include "shima.h"


static SILLYR *bgLyr,*startLyr,*selectLyr,*prevLyr,*nextLyr,*selLyr,*lastLyr,*firstLyr,*btn_prevLyr,*btn_nextLyr;
static SILLYR *creatorLyr,*goalLyr,*titleLyr,*boardLyr,*tmpLyr,*btn_backLyr,*btn_restartLyr,*movesLyr,*mcntLyr;
static SILLYR *btn_okLyr,*btn_cancelLyr,*warnrestartLyr,*warnbackLyr,*starLyr,*winningLyr,*btn_anotherLyr;
static SILLYR *btn_againLyr,*winmovesLyr,*btn_shuffleLyr,*warnshuffleLyr,*btn_infoLyr,*infobackLyr,*infoLyr;
static SILFONT *boogalo18Fnt,*boogalo22Fnt,*boogalonumFnt,*mm32Fnt,*ss16Fnt,*ad24Fnt,*ado24Fnt,*ado28Fnt;
static SILGROUP *selectGrp,*solveGrp,*warningGrp,*winningGrp,*infoGrp;
static int selected=0;
static char **inifiles=NULL;
static int iniamount=0;
static int gamestate=0;
static UINT mcnt=0;

/* all the function declarations */
BYTE piece2idx(char);
char idx2piece(BYTE);
void copyblock(SILLYR *, SILLYR *, int , int , int , int , BYTE );
void parseIni(char *);
void showPreview(SILLYR *,int );
void insertIni(int , char *);
void getPuzzles();
void updateMove();
void resetPuzzle();
void leavePuzzle();
void shufflePuzzle(UINT,UINT);
BYTE checkshift(int ,int ,int ); 
BYTE shiftpiece (int ,int ,int ); 

UINT back(SILEVENT *event);
UINT restart(SILEVENT *event);
UINT another(SILEVENT *event);
UINT again(SILEVENT *event);
UINT lightUp(SILEVENT *event);
UINT keyhandler(SILEVENT *event);
UINT selectPuzzle(SILEVENT *event);
UINT nextPuzzle(SILEVENT *event);
UINT previousPuzzle(SILEVENT *event);
UINT okhandler(SILEVENT *event);
UINT cancelhandler(SILEVENT *event);
UINT dragpiece(SILEVENT *); 
UINT shuffle(SILEVENT *);
UINT info(SILEVENT *);
UINT goplay(SILEVENT *);

void initBackground(); 

void initPuzzleSelect(); 
void initPuzzleSolving(); 
void initWarning(); 
void initWinning(); 
void initInfo(); 

void puzzleSelect(); 
void puzzleSolving(); 
void warning(BYTE type); 
void winning(); 
void displayInfo();


PUZZLE puzzle;

/* parse piece code to index number */

BYTE piece2idx(char piece) {
  const char *pc=PIECECODES;
  for (int i=0;i<strlen(pc);i++) {
    if (piece==pc[i]) return i;
  }
  log_warn("Unkown piece code '%c', converting it to space",piece);
  return 0;
}

char idx2piece(BYTE idx) {
  char *ar=PIECECODES;
  if (idx>strlen(ar)) {
    log_warn("Unkown index '%d', converting it to space",idx);
    return '_';
  }
  return ar[idx];
}

/* copy block (pw x ph) out of layer to another layer, from xfrom,yfrom to xto, yto  */
void copyblock(SILLYR *from, SILLYR *to, int xfrom, int yfrom, int xto, int yto, BYTE blend) {
  BYTE red,green,blue,alpha;
  for (int y=0;y<puzzle.ph;y++) {
    for (int x=0;x<puzzle.pw;x++) {
      sil_getPixelLayer(from,xfrom+x,yfrom+y,&red,&green,&blue,&alpha);
      if (blend) {
        sil_blendPixelLayer(to,xto+x,yto+y,red,green,blue,alpha);
      } else {
        sil_putPixelLayer(to,xto+x,yto+y,red,green,blue,alpha);
      }
    }
  }
}

/* shuffle piezes */
void shufflePuzzle(UINT seed, UINT amount) {
  BYTE dir;
  UINT xp,yp;
  UINT x,y;
  BYTE done=0;
  BYTE dorig=0;
  UINT oldmcnt=mcnt;
  pcg32_random_t pcg;

  pcg32_srandom_r(&pcg,seed,seed);

  for (int cnt=0;cnt<amount;cnt++) {
    dorig=pcg32_boundedrand_r(&pcg,4);
    xp=pcg32_boundedrand_r(&pcg,puzzle.maxx);
    yp=pcg32_boundedrand_r(&pcg,puzzle.maxy);
    x=xp;
    y=yp;
    do {
      dir=dorig;
      do {
        /* check at random position if we can shift the piece */
        /* starting at given randmon direction                */
        /* 0=up, 1=right, 2=down, 3=left */
        switch(dir) {
          case 0:
            done=shiftpiece(puzzle.pos[x][y],0,-1);
            break;
          case 1:
            done=shiftpiece(puzzle.pos[x][y],1,0);
            break;
          case 2:
            done=shiftpiece(puzzle.pos[x][y],0,1);
            break;
          case 3:
            done=shiftpiece(puzzle.pos[x][y],-1,0);
            break;
        }
        dir++;
        if (dir>3) dir=0;
      } while ((dir!=dorig)&&(!done));
      
      /* if not, check next piece */ 
      x++;
      if (x>puzzle.maxx) {
        x=0;
        y++;
        if (y>puzzle.maxy) {
          y=0;
        }
      }
    } while ((!done)&&((xp!=x)||(yp!=y)));
    mcnt=oldmcnt;
  }
  updateMove();
  sil_updateDisplay();
}



/* parse given .ini file (should be in PUZZLEDIR directory) */

void parseIni(char *filename) {
  FILE *fp;
  char file[256];
  char line[256];
  char *d=NULL,*k=NULL,*v=NULL;
  int cnt=0;
  BYTE state=PARSEKEYWORD;

  //log_info("parse ini %s",filename);

  memset(&puzzle,0,sizeof(puzzle));
  /* just to be sure */
  puzzle.shufflefirst=0;
  puzzle.posx=0;
  puzzle.posy=0;
  puzzle.maxx=0;
  puzzle.maxy=0;
  puzzle.movement=MOVEALL;

  /* open .ini file */
  memset(&file[0],0,sizeof(file));
  strcat(file,PUZZLEDIR);
  strncat(file,filename,sizeof(file)-strlen(file)-1);
  fp=fopen(file,"r");
  if (NULL==fp) {
    log_warn("Can't open %s (%s)",filename,file);
    return;
  }

  while(!feof(fp)) {
    memset(&line[0],0,sizeof(line));
    k=fgets(&line[0],sizeof(line)-1,fp);
    /* skip if can't be read */
    if (NULL==k) continue;
    /* chop trailing newline */
    for (int i=0;i<strlen(k);i++) {
      if (('\r'==k[i])||('\n'==k[i])) k[i]='\0';
    }
    /* skip spaces till keyword found */
    while(isblank(*k)) k++;
    /* skip if empty line or remark */
    if (('\0'==k[0])||('#'==k[0])) {
      state=PARSEKEYWORD;
      continue;
    }
    v=k;
    /* remove spaces in keyword and ':'*/
    /* make keyword lowercase */
    while((*v)&&(':'!=*v)) {
      if (isspace(*v)) *v='\0';
      v++;
    }
    if (*v==':') { 
      *v='\0';
      v++;
    }
    /* skip spaces in front of value*/
    while(isblank(*v)) v++;
    /* remove trailing spaces */
    for (int i=strlen(v)-1;((i>0)&&(isspace(v[i])));i--) v[i]='\0';

    // log_info("Got key '%s' and value '%s'",k,v);

    if (PARSEKEYWORD==state) {
      /* translate keywords to lowercase */
      for (int i=0;i<strlen(k);i++) k[i]=tolower(k[i]);
    }

    /* ok, at this point we a have key & possible value, lets work with that */

    /* are we parsing start map ? */
    if (PARSESTART==state) {
      if (0==strlen(k)) {
        state=PARSEKEYWORD;
        continue;
      }
      for (int i=0;i<strlen(k);i++) {
        puzzle.start[i][cnt]=piece2idx(k[i]);
        puzzle.pos[i][cnt]=piece2idx(k[i]);
      }
      cnt++;
      if (cnt>puzzle.maxy) puzzle.maxy=cnt;
      if (strlen(k)>puzzle.maxx) puzzle.maxx=strlen(k);
      continue;
    } 

    /* are we parsing finish map ? */
    if (PARSEGOAL==state) {
      if (0==strlen(k)) {
        state=PARSEKEYWORD;
        continue;
      }
      for (int i=0;i<strlen(k);i++) {
        puzzle.finish[i][cnt]=piece2idx(k[i]);
      }
      cnt++;
      if (cnt>puzzle.maxy) puzzle.maxy=cnt;
      if (strlen(k)>puzzle.maxx) puzzle.maxx=strlen(k);
      continue;
    }

    if(strcmp(k,"movement")==0) {
      for (int i=0;i<strlen(v);i++) v[i]=tolower(v[i]);
      if (strcmp(v,"shortest")==0) {
        puzzle.movement=MOVESHORTEST;        
        continue;
      }
      if (strcmp(v,"longest")==0) {
        puzzle.movement=MOVELONGEST;        
      }
      continue;
    }

    if(strcmp(k,"title")==0) {
      strcpy(puzzle.title,v);
      continue;
    }
    if(strcmp(k,"creator")==0) {
      /* insert an enter, if it is to long */
      if (strlen(v)>60) {
        for (int i=60;i>0;i--) {
          if (isblank(v[i])) {
            v[i]='\n';
            i=0;
          }
        }
      }
      strcpy(puzzle.creator,v);
      continue;
    }
    if(strcmp(k,"goal")==0) {
      /* insert an enter, if it is to long */
      if (strlen(v)>60) {
        for (int i=60;i>0;i--) {
          if (isblank(v[i])) {
            v[i]='\n';
            i=0;
          }
        }
      }
      strcpy(puzzle.goal,v);
      continue;
    }
    if(strcmp(k,"preview")==0) {
      strcpy(puzzle.preview,PUZZLEDIR);
      strncat(puzzle.preview,v,255-strlen(puzzle.preview)-1);
      continue;
    }
    if(strcmp(k,"background")==0) {
      strcpy(puzzle.background,PUZZLEDIR);
      strncat(puzzle.background,v,255-strlen(puzzle.background)-1);
      continue;
    }
    if(strcmp(k,"pieces")==0) {
      strcpy(puzzle.pieces,PUZZLEDIR);
      strncat(puzzle.pieces,v,255-strlen(puzzle.pieces)-1);
      continue;
    }
    if(strcmp(k,"info")==0) {
      strcpy(puzzle.info,PUZZLEDIR);
      strncat(puzzle.info,v,255-strlen(puzzle.info)-1);
      continue;
    }
    if(strcmp(k,"shufflefirst")==0) {
      puzzle.shufflefirst=1;
      continue;
    }
    if (strcmp(k,"pospieces")==0) {
      d=v;
      while((*d)&&(','!=*d)) d++;
      if (','==*d) {
        *d='\0';
        d++;
      }
      puzzle.posx=atoi(v);
      puzzle.posy=atoi(d);
      continue;
    }
    if (strcmp(k,"difficulty")==0) {
      puzzle.difficulty=atoi(v);
      if (puzzle.difficulty>5) puzzle.difficulty=5;
      if (puzzle.difficulty<0) puzzle.difficulty=0;
      continue;
    }
    if(strcmp(k,"start")==0) {
      state=PARSESTART;
      cnt=0;
      continue;
    }
    if(strcmp(k,"finish")==0) {
      state=PARSEGOAL;
      cnt=0;
      continue;
    }
  }

  fclose(fp);

}


/* show preview image out of x,y position of selectLyr */

void showPreview(SILLYR *layer,int idx) {

  /* parse the ini file */
  parseIni(inifiles[idx]);

  /* no preview keyword found or empty ? , no preview for you */
  if (0==strlen(puzzle.preview)) return;

  sil_clearLayer(layer);
  sil_PNGintoLayer(layer,puzzle.preview,0,0);
  sil_show(layer);


}

/* recursive insert inifile at right sorted position */

void insertIni(int place, char *p) {
  if (NULL!=inifiles[place]) {
    /* move to the right to make place */
    insertIni(place+1,inifiles[place]);
  }
  inifiles[place]=p;
}

/* load a list of all .ini files  */

void getPuzzles() {
  DIR *pzls;
  struct dirent *de;
  int cnt=0,found=0;
  char *fname=NULL;
  char *p;


  /* sorted list of .ini files not yet loaded, so get them */

  /* first, get amount of .ini files */
  pzls=opendir(PUZZLEDIR);
  if (NULL==pzls) {
    log_warn("Can't open puzzles directory");
    return;
  }

  while(de=readdir(pzls)) {
    fname=de->d_name;
    if (0==strncmp(&fname[strlen(fname)-4],".ini",4)) {
      iniamount++;
    }
  }
  closedir(pzls);

  if (0==iniamount) {
    log_warn("Can't find any .ini file in puzzles directory");
    return;
  }

  inifiles=malloc(iniamount * sizeof(*inifiles));
  if (NULL==inifiles) {
    log_warn("Can't claim memory for ini files in puzzles directory");
    return;
  }

  /* now get all ini names, sorted */
  pzls=opendir(PUZZLEDIR);
  if (NULL==pzls) {
    log_warn("Can't open puzzles directory");
    return;
  }

  for (int i=0;i<iniamount;i++) inifiles[i]=NULL;
  while((de=readdir(pzls))&&(found<iniamount)) {
    fname=de->d_name;
    if (0==strncmp(&fname[strlen(fname)-4],".ini",4)) {
      p=malloc(255);
      if (NULL==p) {
        log_warn("Out of memory");
        return;
      }
      memset(p,0,255);
      strncpy(p,fname,254);
      if (found==0) {
        inifiles[0]=p;
      } else {
        cnt=0;
        while((cnt<iniamount) && (inifiles[cnt]!=NULL) && (strncmp(p,inifiles[cnt],254)>0)) cnt++;
        insertIni(cnt,p);
      }
      found++;
    }
  }
  closedir(pzls);
}

/* update move counter */
void updateMove() {
  char tmp[100];
  UINT size;

  memset(&tmp[0],0,sizeof(tmp));
  snprintf(tmp,sizeof(tmp)-1,"%d",mcnt);
  size=sil_getTextWidth(mm32Fnt,tmp,0);
  sil_clearLayer(mcntLyr);
  sil_setForegroundColor(SILCOLOR_PALE_GOLDEN_ROD,255);
  sil_drawText(mcntLyr,mm32Fnt,tmp,65-size,0,0);
}

void resetPuzzle() {
  /* place all pieces back to original position */
  for (int i=0;i<65;i++) {
    if (NULL!=puzzle.pieceLyr[i]) {
      sil_placeLayer(puzzle.pieceLyr[i],
        boardLyr->relx+puzzle.posx+puzzle.piece[i].origx*puzzle.pw,
        boardLyr->rely+puzzle.posy+puzzle.piece[i].origy*puzzle.ph);
      puzzle.piece[i].x=puzzle.piece[i].origx;
      puzzle.piece[i].y=puzzle.piece[i].origy;
    }
  }

  /* set internal position back to start position */
  for (int x=0;x<100;x++) {
    for (int y=0;y<100;y++) {
      puzzle.pos[x][y]=puzzle.start[x][y];
    }
  }
  mcnt=0;
  updateMove();
}

void leavePuzzle() {
  /* destroy the dynamicly allocated piece layers */
  for (int i=0;i<65;i++) {
    if (NULL!=puzzle.pieceLyr[i]) sil_destroyLayer(puzzle.pieceLyr[i]);
    puzzle.pieceLyr[i]=NULL;
  }

  /* clear board */
  sil_clearLayer(boardLyr);

  /* hide everything */
  sil_hideGroup(solveGrp);

  /* hand it over to puzzleSelect */
  puzzleSelect();
}

/* Handlers */

UINT back(SILEVENT *event) {
  if (event->type!=SILDISP_MOUSE_DOWN) return 0 ;
  if (SOLVINGPUZZLE==gamestate) {
    warning(WARNBACK);
  }
  return 0;
}

UINT restart(SILEVENT *event) {
  if (event->type!=SILDISP_MOUSE_DOWN) return 0;
  if (SOLVINGPUZZLE==gamestate) {
    warning(WARNRESTART);
  }
  return 0;
}

UINT shuffle(SILEVENT *event) {
  if (event->type!=SILDISP_MOUSE_DOWN) return 0;
  if (SOLVINGPUZZLE==gamestate) {
    warning(WARNSHUFFLE);
  }
  return 0;
}

UINT another(SILEVENT *event) {
  if (event->type!=SILDISP_MOUSE_DOWN) return 0;
  if (SOLVED==gamestate) {
    sil_hideGroup(winningGrp);
    leavePuzzle();
  }
  return 0;
}

UINT again(SILEVENT *event) {
  if (event->type!=SILDISP_MOUSE_DOWN) return 0;
  if (SOLVED==gamestate) {
    gamestate=SOLVINGPUZZLE;
    sil_hideGroup(winningGrp);
    resetPuzzle();
    if (puzzle.shufflefirst) shufflePuzzle(666,500);
  }
  return 1;
}

UINT lightUp(SILEVENT *event) {
  if (event->type==SILDISP_MOUSE_LEFT) {
    sil_setAlphaLayer(event->layer,0.3);
    return 1;
  }
  if (event->type==SILDISP_MOUSE_ENTER) {
    sil_setAlphaLayer(event->layer,1.0);
    return 1;
  }
  return 0;
}

UINT info(SILEVENT *event) {
  if (event->type!=SILDISP_MOUSE_DOWN) return 0;
  if (SOLVINGPUZZLE==gamestate) {
    gamestate=INFO;
    displayInfo();
  }
  return 0;
}

UINT goplay(SILEVENT *event) {
  if (event->type!=SILDISP_MOUSE_UP) return 0;
  sil_hide(startLyr);
  puzzleSelect();
  return 0;
}

UINT keyhandler(SILEVENT *event) {
  time_t seconds;
  time(&seconds);

  switch(event->type) {
    case SILDISP_KEY_DOWN:
      switch(event->key) {
        case SILKY_ESC:
          sil_quitLoop();
          break;
        case SILKY_RIGHT:
          if ((SELECTPUZZLE==gamestate)&&(selected+1<iniamount)) {
            selected++;
            puzzleSelect(); 
          }
          break;
        case SILKY_LEFT:
          if ((SELECTPUZZLE==gamestate)&&(selected>0)) {
            selected--;
            puzzleSelect(); 
          }
          break;
        case SILKY_SPACE:
          if (SELECTPUZZLE==gamestate) {
            puzzleSolving();
            if (puzzle.shufflefirst) shufflePuzzle(666,500);
          }
          break;
      }
      break;
    case SILDISP_KEY_UP:
      if  (SILKY_PRINTSCREEN==event->key) {
        log_info("saving screen to 'printscreen.png'");
        sil_saveDisplay("printscreen.png",1000,1000,0,0);
      }
      break;
  }
  return 0;
}

UINT selectPuzzle(SILEVENT *event) {
  if (event->type==SILDISP_MOUSE_UP) {
    puzzleSolving();
    if (puzzle.shufflefirst) shufflePuzzle(666,500);
  }
  return 0;
}

UINT nextPuzzle(SILEVENT *event) {
  if (event->type==SILDISP_MOUSE_UP) {
    if (selected<iniamount) selected++;
    puzzleSelect(); 
  }
  return 0;
}

UINT previousPuzzle(SILEVENT *event) {
  if (event->type==SILDISP_MOUSE_UP) {
    if (selected>0) selected--;
    puzzleSelect(); 
  }
  return 0;
}

UINT okhandler(SILEVENT *event) {
  time_t seconds;
  time(&seconds);

  if (gamestate==WARNRESTART) {
    gamestate=SOLVINGPUZZLE;
    sil_hideGroup(warningGrp);
    resetPuzzle();
    if (puzzle.shufflefirst) shufflePuzzle(666,500);
    return 1;
  }
  if (gamestate==WARNBACK) {
    gamestate=SELECTPUZZLE;
    sil_hideGroup(warningGrp);
    leavePuzzle();
    return 0;
  }
  if (gamestate==WARNSHUFFLE) {
    gamestate=SOLVINGPUZZLE;
    sil_hideGroup(warningGrp);
    shufflePuzzle(seconds,500);
    mcnt=0;
    return 0;
  }
  return 0;
}

UINT cancelhandler(SILEVENT *event) {
  gamestate=SOLVINGPUZZLE;
  sil_hideGroup(warningGrp);
  sil_hideGroup(infoGrp);
  return 1;
}


/* Check if piece can be moved                        */
/* returns 1 when can be, 0 when wasn't allow to move */
/* dx,dy -> positive = right,down negative= left,up   */

BYTE checkshift(int code,int dx,int dy) {
  BYTE isok=1;
  int x,y,addx,addy,absdx,absdy;

  if ((0==dx)&&(0==dy)) return 0;

  absdx=SIL_ABS(dx);
  absdy=SIL_ABS(dy);
  if (absdx>absdy) {
    addx=dx/absdx;
    addy=0;
  } else {
    addy=dy/absdy;
    addx=0;
  }

  //log_info("code %d (%c) , addx=%d, addy=%d",code,idx2piece(code),addx,addy);
  //log_info("rlud: %d,%d,%d,%d",puzzle.piece[code].right,puzzle.piece[code].left,puzzle.piece[code].up,puzzle.piece[code].down);

  /* bail out if prohibited by movement indicator */
  if ((addx>0)&&(0==puzzle.piece[code].right)) return 0;
  if ((addx<0)&&(0==puzzle.piece[code].left )) return 0;
  if ((addy<0)&&(0==puzzle.piece[code].up   )) return 0;
  if ((addy>0)&&(0==puzzle.piece[code].down )) return 0;

  /* seach map for all pieces with same code */
  /* if moved, it should move to place occupied by space or part of */
  /* own structure, so same code                                   */
  for (x=0;x<puzzle.maxx;x++) {
    for (y=0;y<puzzle.maxy;y++) {
      if (puzzle.pos[x][y]==code) {
        /* check if we didn't hit a border yet */
        if ((x+addx==puzzle.maxx)||(x+addx<0)||
            (y+addy==puzzle.maxy)||(y+addy<0)) {
          isok=0;
        } else {
          if (((puzzle.pos[x+addx][y+addy])!=code)&&
              ((puzzle.pos[x+addx][y+addy])!=0)) {
            isok=0;
          }
        }
      }
    }
  }
  return isok;
}


/* returns 1 when has been moved, 0 when wasn't allowed to move */
/* dx,dy -> positive = right,down negative= left,up             */
BYTE shiftpiece (int code,int dx,int dy) {
  BYTE ret=0;
  BYTE won=1;


  ret=checkshift(code,dx,dy);
  if (0==ret) return 0;
  mcnt++;
  updateMove();
  if (dx<0) {
    /* left */
    for (int y=0;y<puzzle.maxy;y++) {
      for (int x=1;x<puzzle.maxx;x++) {
        if (puzzle.pos[x][y]==code) {
          puzzle.pos[x-1][y]=code;
          puzzle.pos[x][y]=0;
        }
      }
    }
    puzzle.piece[code].x--;
    sil_moveLayer(puzzle.pieceLyr[code],-puzzle.pw,0);
  }
  if (dx>0) {
    /* right */
    for (int y=0;y<puzzle.maxy;y++) {
      for (int x=puzzle.maxx-1;x>=0;x--) {
        if (puzzle.pos[x][y]==code) {
          puzzle.pos[x+1][y]=code;
          puzzle.pos[x][y]=0;
        }
      }
    }
    puzzle.piece[code].x++;
    sil_moveLayer(puzzle.pieceLyr[code],puzzle.pw,0);
  }
  if (dy>0) {
    /* down */
    for (int x=0;x<puzzle.maxx;x++) {
      for (int y=puzzle.maxy-1;y>=0;y--) {
        if (puzzle.pos[x][y]==code) {
          puzzle.pos[x][y+1]=code;
          puzzle.pos[x][y]=0;
        }
      }
    }
    puzzle.piece[code].y++;
    sil_moveLayer(puzzle.pieceLyr[code],0,puzzle.ph);
  }
  if (dy<0) {
    /* up */
    for (int x=0;x<puzzle.maxx;x++) {
      for (int y=1;y<puzzle.maxy;y++) {
        if (puzzle.pos[x][y]==code) {
          puzzle.pos[x][y-1]=code;
          puzzle.pos[x][y]=0;
        }
      }
    }
    puzzle.piece[code].y--;
    sil_moveLayer(puzzle.pieceLyr[code],0,-puzzle.ph);
  }

  /* check if we are in finished postion */
  for (int x=0;x<puzzle.maxx;x++) {
    for (int y=1;y<puzzle.maxy;y++) {
      if ((puzzle.pos[x][y]!=puzzle.finish[x][y])&&(puzzle.finish[x][y]!=1)) won=0;
    }
  }
  if (won) winning();


  return 1;
}

/* handler for drag events */

UINT dragpiece(SILEVENT *event) {
  PIECE *pp;
  int dx=0;
  int dy=0;
  BYTE ret=0;

  pp=(PIECE *) event->layer->user;
  //log_info("DRAG captured for %c",idx2piece(pp->code));

  /* figure out which direction, use 5 pixels as margin */
  dx=(event->x)-(event->layer->relx);
  dy=(event->y)-(event->layer->rely);
  if (SIL_ABS(dx) > SIL_ABS(dy)) {
    /* move right or left */
    if (SIL_ABS(dx)>5) ret=shiftpiece(pp->code,dx,0);
  } else {
    /* move up or down */
    if (SIL_ABS(dy)>5) ret=shiftpiece(pp->code,0,dy);
  }
  if (ret>0) {
    /* require new mouse click for next moves */
    /* otherwise movement becomes shacky      */
    sil_clearFlags(puzzle.pieceLyr[pp->code],SILFLAG_BUTTONDOWN);
    sil_updateDisplay();
  }
  return 0;
}



void initBackground() {
  bgLyr=sil_PNGtoNewLayer("game_background.png",0,0);
  sil_setKeyHandler(bgLyr,0,0,0,keyhandler);

  startLyr=sil_PNGtoNewLayer("intro.png",100,150);
  sil_setFlags(startLyr,SILFLAG_MOUSEALLPIX);
  sil_setClickHandler(startLyr,goplay) ;
  sil_updateDisplay();

}

void initPuzzleSelect() {
  selectGrp=sil_createGroup();
  ado28Fnt=sil_loadFont("architectsdaughter_thickoutline_28px.fnt");
  ad24Fnt=sil_loadFont("architectsdaughter_24px.fnt");
  ado24Fnt=sil_loadFont("architectsdaughter_thickoutline_24px.fnt");
  ss16Fnt=sil_loadFont("sourcesanspro_16px.fnt");
  mm32Fnt=sil_loadFont("mm_32px.fnt");
  boogalo18Fnt=sil_loadFont("boogalo_18px.fnt");
  boogalo22Fnt=sil_loadFont("boogalo_22px.fnt");
  boogalonumFnt=sil_loadFont("boogalonum_60px.fnt");

  selectLyr=sil_PNGtoNewLayer("selection_background.png",200,110);
  sil_addLayerGroup(selectGrp,selectLyr);

  firstLyr=sil_addLayer(260,300,100,100,SILTYPE_ARGB);
  sil_setAlphaLayer(firstLyr,0.1);
  sil_addLayerGroup(selectGrp,firstLyr);

  prevLyr=sil_addLayer(370,300,100,100,SILTYPE_ARGB);
  sil_setAlphaLayer(prevLyr,0.5);
  sil_addLayerGroup(selectGrp,prevLyr);

  nextLyr=sil_addLayer(520,300,100,100,SILTYPE_ARGB);
  sil_setAlphaLayer(nextLyr,0.5);
  sil_addLayerGroup(selectGrp,nextLyr);

  lastLyr=sil_addLayer(630,300,100,100,SILTYPE_ARGB);
  sil_setAlphaLayer(lastLyr,0.1);
  sil_addLayerGroup(selectGrp,lastLyr);

  selLyr=sil_addLayer(440,310,100,100,SILTYPE_ARGB);
  sil_setClickHandler(selLyr,selectPuzzle);
  sil_addLayerGroup(selectGrp,selLyr);
  
  btn_prevLyr=sil_PNGtoNewLayer("buttonprevious.png",230,430);
  sil_setAlphaLayer(btn_prevLyr,0.3);
  sil_setHoverHandler(btn_prevLyr,lightUp);
  sil_setClickHandler(btn_prevLyr,previousPuzzle);
  sil_addLayerGroup(selectGrp,btn_prevLyr);

  btn_nextLyr=sil_PNGtoNewLayer("buttonnext.png",630,435);
  sil_setAlphaLayer(btn_nextLyr,0.3);
  sil_setHoverHandler(btn_nextLyr,lightUp);
  sil_setClickHandler(btn_nextLyr,nextPuzzle);
  sil_addLayerGroup(selectGrp,btn_nextLyr);

  titleLyr=sil_addLayer(350,525,375,35,SILTYPE_ARGB);
  sil_addLayerGroup(selectGrp,titleLyr);

  goalLyr=sil_addLayer(270,560,375,55,SILTYPE_ARGB);
  sil_addLayerGroup(selectGrp,goalLyr);
  creatorLyr=sil_addLayer(270,620,375,55,SILTYPE_ARGB);
  sil_addLayerGroup(selectGrp,creatorLyr);

  starLyr=sil_addLayer(630,620,120,55,SILTYPE_ARGB);
  sil_addLayerGroup(selectGrp,starLyr);

  sil_hideGroup(selectGrp);
}

void initWarning() {
  warningGrp=sil_createGroup();

  warnrestartLyr=sil_PNGtoNewLayer("warningrestart.png",290,300);
  sil_setFlags(warnrestartLyr,SILFLAG_MOUSESHIELD);
  sil_addLayerGroup(warningGrp,warnrestartLyr);

  warnbackLyr=sil_PNGtoNewLayer("warningback.png",290,300);
  sil_setFlags(warnbackLyr,SILFLAG_MOUSESHIELD);
  sil_addLayerGroup(warningGrp,warnbackLyr);

  warnshuffleLyr=sil_PNGtoNewLayer("warningshuffle.png",290,300);
  sil_setFlags(warnshuffleLyr,SILFLAG_MOUSESHIELD);
  sil_addLayerGroup(warningGrp,warnshuffleLyr);

  btn_okLyr=sil_PNGtoNewLayer("surebutton.png",307,535);
  sil_setFlags(btn_okLyr,SILFLAG_MOUSEALLPIX);
  sil_setAlphaLayer(btn_okLyr,0.3);
  sil_setHoverHandler(btn_okLyr,lightUp);
  sil_setClickHandler(btn_okLyr,okhandler);
  sil_addLayerGroup(warningGrp,btn_okLyr);

  btn_cancelLyr=sil_PNGtoNewLayer("cancelbutton.png",480,535);
  sil_setFlags(btn_cancelLyr,SILFLAG_MOUSEALLPIX);
  sil_setAlphaLayer(btn_cancelLyr,0.3);
  sil_setHoverHandler(btn_cancelLyr,lightUp);
  sil_setClickHandler(btn_cancelLyr,cancelhandler);
  sil_addLayerGroup(warningGrp,btn_cancelLyr);


  sil_hideGroup(warningGrp);
}

void initWinning() {
  winningGrp=sil_createGroup();
  winningLyr=sil_PNGtoNewLayer("congrats.png",135,140);
  sil_setFlags(winningLyr,SILFLAG_MOUSESHIELD);
  sil_addLayerGroup(winningGrp,winningLyr);

  btn_anotherLyr=sil_PNGtoNewLayer("anotherbutton.png",160,585);
  sil_setFlags(btn_anotherLyr,SILFLAG_MOUSEALLPIX);
  sil_setAlphaLayer(btn_anotherLyr,0.3);
  sil_setHoverHandler(btn_anotherLyr,lightUp);
  sil_setClickHandler(btn_anotherLyr,another);
  sil_addLayerGroup(winningGrp,btn_anotherLyr);

  btn_againLyr=sil_PNGtoNewLayer("againbutton.png",505,585);
  sil_setFlags(btn_againLyr,SILFLAG_MOUSEALLPIX);
  sil_setAlphaLayer(btn_againLyr,0.3);
  sil_setHoverHandler(btn_againLyr,lightUp);
  sil_setClickHandler(btn_againLyr,again);
  sil_addLayerGroup(winningGrp,btn_againLyr);

  winmovesLyr=sil_addLayer(410,365,100,80,SILTYPE_ARGB);
  sil_addLayerGroup(winningGrp,winmovesLyr);


  sil_hideGroup(winningGrp);

}

void initInfo() {
  infoGrp=sil_createGroup();
  infobackLyr=sil_PNGtoNewLayer("infobackground.png",45,120);
  sil_setFlags(infobackLyr,SILFLAG_MOUSESHIELD);
  sil_setClickHandler(infobackLyr,cancelhandler);
  sil_addLayerGroup(infoGrp,infobackLyr);

  infoLyr=sil_addLayer(85,160,800,600,SILTYPE_ARGB);
  sil_addLayerGroup(infoGrp,infoLyr);
  
  sil_hideGroup(infoGrp);
}

void initPuzzleSolving() {
  solveGrp=sil_createGroup();
  boardLyr=sil_addLayer(150,150,700,700,SILTYPE_ARGB);
  sil_addLayerGroup(solveGrp,boardLyr);

  for (int i=0;i<65;i++) puzzle.pieceLyr[i]=NULL;

  btn_backLyr=sil_PNGtoNewLayer("back.png",30,880);
  sil_setFlags(btn_backLyr,SILFLAG_MOUSEALLPIX);
  sil_setAlphaLayer(btn_backLyr,0.3);
  sil_setHoverHandler(btn_backLyr,lightUp);
  sil_setClickHandler(btn_backLyr,back);
  sil_addLayerGroup(solveGrp,btn_backLyr);

  btn_restartLyr=sil_PNGtoNewLayer("restart.png",450,880);
  sil_setFlags(btn_restartLyr,SILFLAG_MOUSEALLPIX);
  sil_setAlphaLayer(btn_restartLyr,0.3);
  sil_setHoverHandler(btn_restartLyr,lightUp);
  sil_setClickHandler(btn_restartLyr,restart);
  sil_addLayerGroup(solveGrp,btn_restartLyr);

  btn_shuffleLyr=sil_PNGtoNewLayer("shuffle.png",900,880);
  sil_setFlags(btn_shuffleLyr,SILFLAG_MOUSEALLPIX);
  sil_setAlphaLayer(btn_shuffleLyr,0.3);
  sil_setHoverHandler(btn_shuffleLyr,lightUp);
  sil_setClickHandler(btn_shuffleLyr,shuffle);
  sil_addLayerGroup(solveGrp,btn_shuffleLyr);

  btn_infoLyr=sil_PNGtoNewLayer("info.png",670,880);
  sil_setFlags(btn_infoLyr,SILFLAG_MOUSEALLPIX);
  sil_setAlphaLayer(btn_infoLyr,0.3);
  sil_setHoverHandler(btn_infoLyr,lightUp);
  sil_setClickHandler(btn_infoLyr,info);
  sil_addLayerGroup(solveGrp,btn_infoLyr);


  movesLyr=sil_addLayer(900,145,100,80,SILTYPE_ARGB);
  sil_PNGintoLayer(movesLyr,"moves.png",0,0);
  sil_addLayerGroup(solveGrp,movesLyr);

  mcntLyr=sil_addLayer(900,200,100,120,SILTYPE_ARGB);
  sil_addLayerGroup(solveGrp,mcntLyr);


  mcnt=0;
  updateMove();

  sil_hideGroup(solveGrp);

}


/* show selection of puzzles to choose from */

void puzzleSelect() {
  int w=0;


  sil_hideGroup(solveGrp);
  sil_hideGroup(warningGrp);

  sil_show(selectLyr);
  sil_show(firstLyr);
  sil_show(prevLyr);
  sil_show(selLyr);
  sil_show(nextLyr);
  sil_show(lastLyr);
  sil_show(starLyr);
  gamestate=SELECTPUZZLE;


  /* show puzzles to select */

  if (selected>1) {
    showPreview(firstLyr,selected-2);
  }  else {
    sil_hide(firstLyr);
  }
  if (selected>0) {
    showPreview(prevLyr,selected-1);
    sil_show(btn_prevLyr);
  }  else {
    sil_hide(prevLyr);
  }

  if (selected+1<iniamount) {
    showPreview(nextLyr,selected+1);
    sil_show(btn_nextLyr);
  }  else {
    sil_hide(nextLyr);
  }
  if (selected+2<iniamount) {
    showPreview(lastLyr,selected+2);
  }  else {
    sil_hide(lastLyr);
  }

  /* last one, the one in the middle, so that after selecting it */
  /* the ini file is already loaded                              */

  showPreview(selLyr,selected);

  /* hide next/previous button if nothing */
  /* is there to point to                   */
  if (0==selected) {
    sil_setAlphaLayer(btn_prevLyr,0.3);
    sil_hide(btn_prevLyr);
  }
  if (selected==iniamount-1) {
    sil_setAlphaLayer(btn_nextLyr,0.3);
    sil_hide(btn_nextLyr);
  }


  sil_hide(titleLyr);
  sil_hide(creatorLyr);
  sil_hide(goalLyr);


  /* Title */
  if (strlen(puzzle.title)) {
    /* title layer might be used & moved previous */
    w=sil_getTextWidth(mm32Fnt,puzzle.title,0)/2;
    sil_placeLayer(titleLyr,500-w,525);
    sil_clearLayer(titleLyr);
    sil_setForegroundColor(SILCOLOR_PALE_GOLDEN_ROD,255);
    sil_drawText(titleLyr,mm32Fnt,puzzle.title,0,0,0);
    sil_show(titleLyr);
  }

  /* Description / Goal */
  if (strlen(puzzle.goal)) {
    sil_clearLayer(goalLyr);
    sil_drawText(goalLyr,boogalo22Fnt,"Goal:",25,0,0);
    sil_drawText(goalLyr,boogalo22Fnt,puzzle.goal,60,0,SILTXT_KEEPCOLOR);
    sil_show(goalLyr);
  }

  /* Creator information */
  if (strlen(puzzle.creator)) {
    sil_clearLayer(creatorLyr);
    sil_drawText(creatorLyr,boogalo18Fnt,"Created by:",0,0,0);
    sil_setForegroundColor(SILCOLOR_SILVER,255);
    sil_drawText(creatorLyr,boogalo18Fnt,puzzle.creator,60,0,0);
    sil_show(creatorLyr);
  }

  /* Difficulty information */
  sil_setForegroundColor(SILCOLOR_PALE_GOLDEN_ROD,255);
  sil_drawText(starLyr,boogalo22Fnt,"Difficulty",40,0,0);
  for (int i=5;i--;i>0) {
    if (puzzle.difficulty>i) {
      sil_PNGintoLayer(starLyr,"staryellow.png",35+i*14,30);
    } else {
      sil_PNGintoLayer(starLyr,"starwhite.png",35+i*14,30);
    }
  }


  sil_updateDisplay();
}

void puzzleSolving() {
  sil_hideGroup(warningGrp);
  sil_hideGroup(selectGrp);
  sil_showGroup(solveGrp);
  int w,code;
  int calcw,calch;
  int min_x,max_x,min_y,max_y;

  gamestate=SOLVINGPUZZLE;
   /* re-use title layer */
  w=sil_getTextWidth(mm32Fnt,puzzle.title,0)/2;
  sil_placeLayer(titleLyr,500-w,80);
  sil_show(titleLyr);

  /* place background of board */
  sil_PNGintoLayer(boardLyr,puzzle.background,0,0);
  sil_show(boardLyr);

  /* place info about game on hidden info panel */
  sil_clearLayer(infoLyr);
  if (puzzle.info[0]!=0) {
    sil_PNGintoLayer(infoLyr,puzzle.info,0,0);
  } else {
    sil_hide(btn_infoLyr);
  }
  

  /* now, create layers with the piecespic */
  tmpLyr=sil_PNGtoNewLayer(puzzle.pieces,0,0);
  //log_info("pieces %s wxh=%dx%d",puzzle.pieces,tmpLyr->fb->width,tmpLyr->fb->height);
  sil_hide(tmpLyr);

  /* calculate widthxheight of one code/rectangle */
  puzzle.pw=(tmpLyr->fb->width)/(puzzle.maxx);
  puzzle.ph=(tmpLyr->fb->height)/(puzzle.maxy);

  for (int y=0;y<puzzle.maxy;y++) {
    for (int x=0;x<puzzle.maxx;x++) {
      code=puzzle.start[x][y];
      if (code>2) {
        if (NULL==puzzle.pieceLyr[code]) {
          /* found one that isn't loaded yet                                 */
          /* first, get dimenstions of layer to create                       */
          /* found element cannot always be left upper corner of whole layer */
          /* parts of it might extend to left on lower rows                  */

          min_x=x;
          max_x=x;
          min_y=y;
          max_y=y;
          for (int iy=y;iy<puzzle.maxy;iy++) {
            for (int ix=0;ix<puzzle.maxx;ix++) {
              if (puzzle.start[ix][iy]==code) {
                if (ix<min_x) min_x=ix;
                if (ix>max_x) max_x=ix;
                if (iy<min_y) min_y=iy;
                if (iy>max_y) max_y=iy;
              }
            }
          }
          calcw=(max_x-min_x+1)*puzzle.pw;
          calch=(max_y-min_y+1)*puzzle.ph;

          /* create layer for pieces that do move */
          puzzle.pieceLyr[code]=sil_addLayer(
            boardLyr->relx+puzzle.posx+min_x*puzzle.pw,
            boardLyr->rely+puzzle.posy+min_y*puzzle.ph,
            calcw,calch,
            SILTYPE_ARGB);
          if (NULL==puzzle.pieceLyr[code]) {
            log_warn("can't create layers for pieces");
            return;
          }

          /* populate piece specific information */
          puzzle.piece[code].code=code;
          puzzle.piece[code].width=calcw;
          puzzle.piece[code].height=calch;
          puzzle.piece[code].up=1;
          puzzle.piece[code].down=1;
          puzzle.piece[code].right=1;
          puzzle.piece[code].left=1;
          puzzle.piece[code].origx=min_x;
          puzzle.piece[code].origy=min_y;
          puzzle.piece[code].x=min_x;
          puzzle.piece[code].y=min_y;

          /* set movement boundaries per piece, if there are any */
          if (MOVESHORTEST==puzzle.movement) {
            if (calcw>calch) {
              /* horizontal longest , so don't move up & down */
              puzzle.piece[code].up=0;
              puzzle.piece[code].down=0;
            } else {
              /* vertical longest */
              puzzle.piece[code].right=0;
              puzzle.piece[code].left=0;
            }
          } else {
            if (MOVELONGEST==puzzle.movement) {
              if (calcw>calch) {
                /* horizontal longest , so only move up & down */
                puzzle.piece[code].right=0;
                puzzle.piece[code].left=0;
              } else {
                /* vertical longest */
                puzzle.piece[code].up=0;
                puzzle.piece[code].down=0;
              }
            }
          }

          /* store pointer to this PIECE inside "user" part of layerstruct     */
          /* doing this, we can traceback to piececode when is given via event */
          puzzle.pieceLyr[code]->user= &puzzle.piece[code];

          /* set mousehandler */
          sil_setDragHandler(puzzle.pieceLyr[code],dragpiece);
          sil_show(puzzle.pieceLyr[code]);
        }
        /* now we have a block of a piece; copy this to layer of piece */
        copyblock(tmpLyr,puzzle.pieceLyr[code],x*puzzle.pw,y*puzzle.ph,
          (x-puzzle.piece[code].x)*puzzle.pw,(y-puzzle.piece[code].y)*puzzle.ph,0);

      } else { 
        if (2==code) {
          /* fixed piece; just copy single instance directly onto board (with blending this time) */
          copyblock(tmpLyr,boardLyr,x*puzzle.pw,y*puzzle.ph,
            x*puzzle.pw+puzzle.posx,y*puzzle.ph+puzzle.posx,1);
        } 
      }
    }
  }

  sil_destroyLayer(tmpLyr);
  mcnt=0;
  updateMove();

  sil_updateDisplay();
}

void warning(BYTE type) {
  if (WARNBACK==type) {
    gamestate=WARNBACK;
    sil_show(warnbackLyr);
  } 
  if (WARNRESTART==type) {
    gamestate=WARNRESTART;
    sil_show(warnrestartLyr);
  } 
  if (WARNSHUFFLE==type) {
    gamestate=WARNSHUFFLE;
    sil_show(warnshuffleLyr);
  }
  sil_show(btn_okLyr);
  sil_setAlphaLayer(btn_okLyr,0.3);
  sil_show(btn_cancelLyr);
  sil_setAlphaLayer(btn_cancelLyr,0.3);
  sil_topGroup(warningGrp);
  sil_updateDisplay();
}

void winning() {
  char tmp[100];
  int size;

  gamestate=SOLVED;
  sil_setAlphaLayer(btn_againLyr,0.3);
  sil_setAlphaLayer(btn_anotherLyr,0.3);
  sil_topGroup(winningGrp);
  sil_showGroup(winningGrp);
  memset(&tmp[0],0,sizeof(tmp));
  snprintf(tmp,sizeof(tmp)-1,"%d",mcnt);

  size=sil_getTextWidth(boogalonumFnt,tmp,0)/2;
  sil_clearLayer(winmovesLyr);
  sil_setForegroundColor(SILCOLOR_PALE_GOLDEN_ROD,255);
  sil_drawText(winmovesLyr,boogalonumFnt,tmp,50-size,0,0);

  sil_updateDisplay();
}

void displayInfo() {
  gamestate=INFO;
  sil_topGroup(infoGrp);
  sil_showGroup(infoGrp);


  sil_updateDisplay();
}

#ifdef SIL_W32
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
#else
int main() {
  void *hInstance=NULL;
#endif

  SILEVENT *se=NULL;


  sil_initSIL(1000,1000,"Shifting Madness",hInstance);
  sil_setLog(NULL,LOG_INFO|LOG_DEBUG|LOG_VERBOSE);

  initBackground();
  getPuzzles();
  initPuzzleSelect();
  initPuzzleSolving();
  initWarning();
  initWinning();
  initInfo();


 // puzzleSelect();

  sil_mainLoop();

  sil_destroyGroup(warningGrp);
  sil_destroyGroup(selectGrp);
  sil_destroyGroup(solveGrp);
  sil_destroyFont(ad24Fnt);
  sil_destroyFont(ado28Fnt);
  sil_destroySIL();


  return 0;
}
