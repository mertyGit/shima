#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <dirent.h>
#ifdef SIL_W32
  #include <windows.h>
#endif
#include "log.h"
#include "sil.h"
#include "shima.h"


static SILLYR *bgLyr,*selectLyr,*prevLyr,*nextLyr,*selLyr,*lastLyr,*firstLyr,*btn_prevLyr,*btn_nextLyr;
static SILLYR *creatorLyr,*descLyr,*titleLyr,*boardLyr,*tmpLyr;
static SILFONT *boogalo18Fnt,*boogalo22Fnt,*mm32Fnt,*ss16Fnt,*ad24Fnt,*ado24Fnt,*ado28Fnt;
static int selected=0;
static char **inifiles=NULL;
static int iniamount=0;
static int gamestate=0;

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

/* copy block out of layer to another layer, from xfrom,yfrom to xto, yto and */
/* width & height of single block                                   */
void copyblock(SILLYR *from, SILLYR *to, int xfrom, int yfrom, int xto, int yto, int w, int h) {
  BYTE red,green,blue,alpha;
  for (int y=0;y<h;y++) {
    for (int x=0;x<w;x++) {
      sil_getPixelLayer(from,xfrom+x,yfrom+y,&red,&green,&blue,&alpha);
      sil_putPixelLayer(to,xto+x,yto+y,red,green,blue,alpha);
    }
  }
}

/* same as copyblock, but now with blendpixel, preserving alpha of background */
void copyblock2(SILLYR *from, SILLYR *to, int xfrom, int yfrom, int xto, int yto, int w, int h) {
  BYTE red,green,blue,alpha;
  for (int y=0;y<h;y++) {
    for (int x=0;x<w;x++) {
      sil_getPixelLayer(from,xfrom+x,yfrom+y,&red,&green,&blue,&alpha);
      sil_blendPixelLayer(to,xto+x,yto+y,red,green,blue,alpha);
    }
  }
}



/* parse given .ini file (should be in PUZZLEDIR directory) */

void parseIni(char *filename) {
  FILE *fp;
  char file[256];
  char line[256];
  char *d=NULL,*k=NULL,*v=NULL;
  int cnt=0;
  BYTE state=PARSEKEYWORD;

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

    //log_info("Got key '%s' and value '%s'",k,v);

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

    /* are we parsing goal map ? */
    if (PARSEGOAL==state) {
      if (0==strlen(k)) {
        state=PARSEKEYWORD;
        continue;
      }
      for (int i=0;i<strlen(k);i++) {
        puzzle.goal[i][cnt]=piece2idx(k[i]);
      }
      cnt++;
      if (cnt>puzzle.maxy) puzzle.maxy=cnt;
      if (strlen(k)>puzzle.maxx) puzzle.maxx=strlen(k);
      continue;
    }

    if(strcmp(k,"movement")==0) {
      for (int i=0;i<strlen(v);i++) v[i]=tolower(v[i]);
      if ((strcmp,v,"shortest")==0) {
        puzzle.movement=MOVESHORTEST;
        continue;
      }
      if ((strcmp,v,"longest")==0) {
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
    if(strcmp(k,"description")==0) {
      /* insert an enter, if it is to long */
      if (strlen(v)>60) {
        for (int i=60;i>0;i--) {
          if (isblank(v[i])) {
            v[i]='\n';
            i=0;
          }
        }
      }
      strcpy(puzzle.description,v);
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
    if(strcmp(k,"start")==0) {
      state=PARSESTART;
      cnt=0;
      continue;
    }
    if(strcmp(k,"goal")==0) {
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

  sil_PNGintoLayer(layer,puzzle.preview,0,0);
  sil_show(layer);

}

void insertIni(int place, char *p) {
  if (NULL!=inifiles[place]) {
    /* move to the right to make place */
    insertIni(place+1,inifiles[place]);
  }
  inifiles[place]=p;
}

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

/* show selection of puzzles to choose from */

void puzzleSelection() {
  sil_show(selectLyr);
  sil_show(firstLyr);
  sil_show(prevLyr);
  sil_show(selLyr);
  sil_show(nextLyr);
  sil_show(lastLyr);
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
  sil_hide(descLyr);


  /* Title */
  if (strlen(puzzle.title)) {
    /* title layer might be used & moved previous */
    sil_placeLayer(titleLyr,350,520);
    sil_paintLayer(titleLyr,SILCOLOR_BLACK,0);
    sil_setForegroundColor(SILCOLOR_PALE_GOLDEN_ROD,255);
    sil_drawText(titleLyr,mm32Fnt,puzzle.title,0,0,0);
    sil_show(titleLyr);
  }

  /* Description / Goal */
  if (strlen(puzzle.description)) {
    sil_paintLayer(descLyr,SILCOLOR_BLACK,0);
    sil_drawText(descLyr,boogalo22Fnt,"Goal:",25,0,0);
    sil_drawText(descLyr,boogalo22Fnt,puzzle.description,60,0,SILTXT_KEEPCOLOR);
    sil_show(descLyr);
  }

  /* Creator information */
  if (strlen(puzzle.creator)) {
    sil_paintLayer(creatorLyr,SILCOLOR_BLACK,0);
    sil_drawText(creatorLyr,boogalo18Fnt,"Created by:",0,0,0);
    sil_setForegroundColor(SILCOLOR_SILVER,255);
    sil_drawText(creatorLyr,boogalo18Fnt,puzzle.creator,60,0,0);
    sil_show(creatorLyr);
  }


  sil_updateDisplay();
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

UINT keyhandler(SILEVENT *event) {
  switch(event->type) {
    case SILDISP_KEY_DOWN:
      switch(event->key) {
        case SILKY_ESC:
          sil_quitLoop();
          break;
        case SILKY_RIGHT:
          if ((SELECTPUZZLE==gamestate)&&(selected+1<iniamount)) {
            selected++;
            puzzleSelection(); 
          }
          break;
        case SILKY_LEFT:
          if ((SELECTPUZZLE==gamestate)&&(selected>0)) {
            selected--;
            puzzleSelection(); 
          }
          break;
        case SILKY_SPACE:
          if (SELECTPUZZLE==gamestate) {
            puzzleSolving();
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

UINT nextPuzzle(SILEVENT *event) {
  if (event->type==SILDISP_MOUSE_UP) {
    if (selected<iniamount) selected++;
    puzzleSelection(); 
  }
  return 0;
}

UINT previousPuzzle(SILEVENT *event) {
  if (event->type==SILDISP_MOUSE_UP) {
    if (selected>0) selected--;
    puzzleSelection(); 
  }
  return 0;
}

/* Check if piece can be moved                        */
/* returns 1 when can be, 0 when wasn't allow to move */
/* dx,dy -> positive = right,down negative= left,up   */

BYTE checkshift(int code,int dx,int dy) {
  BYTE isok=1;
  int x,y;
  int w,h;


  x=puzzle.piece[code].x;
  y=puzzle.piece[code].y;
  w=puzzle.piece[code].width/puzzle.pw;
  h=puzzle.piece[code].height/puzzle.ph;
  /* check if it isn't allowed to move anyway */
  if (dx>0) {
    /* right */
    if (x+w>=puzzle.maxx) return 0;
    if (0==puzzle.piece[code].right) return 0;
    x+=w;
    for (int i=0;i<h;i++) {
      if ((y+i<puzzle.maxy)&&(x<puzzle.maxx)) {
        if (puzzle.pos[x][y+i]!=0) {
          isok=0;
        }
      } else {
        isok=0;
      }
      y++;
    }
    return isok;
  } 
  if (dx<0) {
    /* left */
    if (x-1<0) return 0;
    if (0==puzzle.piece[code].left) return 0;
    x--;
    for (int i=0;i<h;i++) {
      if ((y+i<puzzle.maxy)&&(x>=0)) {
        if (puzzle.pos[x][y+i]!=0) isok=0;
      } else {
        isok=0;
      }
      y++;
    }
    return isok;

  }
  if (dy>0) {
    /* down */
    if (y+h>=puzzle.maxy) return 0;
    if (0==puzzle.piece[code].down) return 0;
    y+=h;
    for (int i=0;i<w;i++) {
      if ((x+i<puzzle.maxx)&&(y<puzzle.maxy)) {
        if (puzzle.pos[x+i][y]!=0) {
          isok=0;
        }
      } else {
        isok=0;
      } 
      x++;
    }
    return isok;
  }
  if (dy<0) {
    /* up */
    if (y-1<0) return 0;
    if (0==puzzle.piece[code].up) return 0;
    y--;
    for (int i=0;i<w;i++) {
      if ((x+i<puzzle.maxx)&&(y>=0)) {
        if (puzzle.pos[x+i][y]!=0) {
          isok=0;
        }
      } else {
        isok=0;
      } 
      x++;
    }
    return isok;
  }

  return 0;
}


/* returns 1 when has been moved, 0 when wasn't allowed to move */
/* dx,dy -> positive = right,down negative= left,up             */
BYTE shiftpiece (int code,int dx,int dy) {
  BYTE ret=0;
  ret=checkshift(code,dx,dy);
  if (dx<0) {
    /* left */
  }
  if (dx>0) {
    /* right */
  }
  if (dy<0) {
    /* up */
  }
  if (dy>0) {
    /* down */
  }


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
    if (SIL_ABS(dx)>5) {
      ret=shiftpiece(pp->code,dx,0);
      if (ret>0) log_info("%c: ALLOWED MOVE",idx2piece(pp->code));
    }
  } else {
    /* move up or down */
    if (SIL_ABS(dy)>5) {
      ret=shiftpiece(pp->code,0,dy);
      if (ret>0) log_info("%c: ALLOWED MOVE",idx2piece(pp->code));
    }
  }
  return 0;
}




void initBackground() {
  bgLyr=sil_PNGtoNewLayer("game_background.png",0,0);
  sil_setKeyHandler(bgLyr,0,0,0,keyhandler);
}

void initPuzzleSelect() {
  ado28Fnt=sil_loadFont("architectsdaughter_thickoutline_28px.fnt");
  ad24Fnt=sil_loadFont("architectsdaughter_24px.fnt");
  ado24Fnt=sil_loadFont("architectsdaughter_thickoutline_24px.fnt");
  ss16Fnt=sil_loadFont("sourcesanspro_16px.fnt");
  mm32Fnt=sil_loadFont("mm_32px.fnt");
  boogalo18Fnt=sil_loadFont("boogalo_18px.fnt");
  boogalo22Fnt=sil_loadFont("boogalo_22px.fnt");

  selectLyr=sil_PNGtoNewLayer("selection_background.png",200,100);
  sil_hide(selectLyr);

  firstLyr=sil_addLayer(100,100,260,300,SILTYPE_ARGB);
  sil_setAlphaLayer(firstLyr,0.1);
  sil_hide(firstLyr);

  prevLyr=sil_addLayer(100,100,370,300,SILTYPE_ARGB);
  sil_setAlphaLayer(prevLyr,0.5);
  sil_hide(prevLyr);

  nextLyr=sil_addLayer(100,100,520,300,SILTYPE_ARGB);
  sil_setAlphaLayer(nextLyr,0.5);
  sil_hide(nextLyr);

  lastLyr=sil_addLayer(100,100,630,300,SILTYPE_ARGB);
  sil_setAlphaLayer(lastLyr,0.1);
  sil_hide(lastLyr);

  selLyr=sil_addLayer(100,100,440,310,SILTYPE_ARGB);
  sil_hide(selLyr);
  
  btn_prevLyr=sil_PNGtoNewLayer("buttonprevious.png",230,430);
  sil_setAlphaLayer(btn_prevLyr,0.3);
  sil_setHoverHandler(btn_prevLyr,lightUp);
  sil_setClickHandler(btn_prevLyr,previousPuzzle);
  sil_hide(btn_prevLyr);

  btn_nextLyr=sil_PNGtoNewLayer("buttonnext.png",630,435);
  sil_setAlphaLayer(btn_nextLyr,0.3);
  sil_setHoverHandler(btn_nextLyr,lightUp);
  sil_setClickHandler(btn_nextLyr,nextPuzzle);
  sil_hide(btn_nextLyr);

  titleLyr=sil_addLayer(375,35,350,520,SILTYPE_ARGB);
  sil_hide(titleLyr);
  descLyr=sil_addLayer(375,55,270,560,SILTYPE_ARGB);
  sil_hide(descLyr);
  creatorLyr=sil_addLayer(375,55,270,620,SILTYPE_ARGB);
  sil_hide(creatorLyr);
}

void hidePuzzleSelect() {
  sil_hide(selectLyr);
  sil_hide(firstLyr);
  sil_hide(prevLyr);
  sil_hide(nextLyr);
  sil_hide(lastLyr);
  sil_hide(selLyr);
  sil_hide(btn_prevLyr);
  sil_hide(btn_nextLyr);
  sil_hide(creatorLyr);
  sil_hide(descLyr);
  sil_hide(titleLyr);
}

void initPuzzleSolving() {
  boardLyr=sil_addLayer(700,700,150,200,SILTYPE_ARGB);
  sil_hide(boardLyr);
  for (int i=0;i<65;i++) puzzle.pieceLyr[i]=NULL;
}

void puzzleSolving() {
  hidePuzzleSelect();
  int code;
  int calcw,calch;
  int min_x,max_x,min_y,max_y;

   /* re-use title layer */
  sil_placeLayer(titleLyr,350,120);
  sil_show(titleLyr);

  /* place background of board */
  sil_PNGintoLayer(boardLyr,puzzle.background,0,0);
  sil_show(boardLyr);

  /* now, create layers with the piecespic */
  tmpLyr=sil_PNGtoNewLayer(puzzle.pieces,0,0);
  log_info("pieces %s wxh=%dx%d",puzzle.pieces,tmpLyr->fb->width,tmpLyr->fb->height);
  sil_hide(tmpLyr);

  /* calculate widthxheight of one code/rectangle */
  puzzle.pw=(tmpLyr->fb->width)/(puzzle.maxx);
  puzzle.ph=(tmpLyr->fb->height)/(puzzle.maxy);

  for (int y=0;y<puzzle.maxy;y++) {
    for (int x=0;x<puzzle.maxx;x++) {
      code=puzzle.start[x][y];
      if ((code>2)&&(NULL==puzzle.pieceLyr[code])) {
        /* found one that isn't loaded yet */
        /* first, get dimenstions of layer to create */
        calcw=0;
        calch=0;
        for (int i=x;i<puzzle.maxx;i++) {
          if (puzzle.start[i][y]==code) {
            calcw+=puzzle.pw;
          } else {
            i=puzzle.maxx;
          }
        }
        for (int i=y;i<puzzle.maxy;i++) {
          if (puzzle.start[x][i]==code) {
            calch+=puzzle.ph;
          } else {
            i=puzzle.maxy;
          }
        }

        /* create layer for pieces that do move */
        puzzle.pieceLyr[code]=sil_addLayer(calcw,calch,boardLyr->relx+puzzle.posx+x*puzzle.pw,
          boardLyr->rely+puzzle.posy+y*puzzle.ph,SILTYPE_ARGB);
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
        puzzle.piece[code].origx=x;
        puzzle.piece[code].origy=y;
        puzzle.piece[code].x=x;
        puzzle.piece[code].y=y;

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


        /* and copy pixelinformation in it from pieces */
        for (int ix=x;ix<puzzle.maxx;ix++) {
          for (int iy=y;iy<puzzle.maxy;iy++) {
            if (puzzle.start[ix][iy]==code) {
              copyblock(tmpLyr,puzzle.pieceLyr[code],ix*puzzle.pw,iy*puzzle.ph,(ix-x)*puzzle.pw,
                (iy-y)*puzzle.ph,puzzle.pw,puzzle.ph);
            }
          }
        }
        sil_show(puzzle.pieceLyr[code]);

      } else {
        if (code>2) {
        }
        if (2==code) {
          /* fixed piece; just copy single instance directly onto board */
          copyblock2(tmpLyr,boardLyr,x*puzzle.pw,y*puzzle.ph,x*puzzle.pw+puzzle.posx,y*puzzle.ph+puzzle.posx,puzzle.pw,puzzle.ph);
        }
      }
    }
  }

  sil_destroyLayer(tmpLyr);

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


  puzzleSelection();

  sil_mainLoop();

  sil_destroyFont(ad24Fnt);
  sil_destroyFont(ado28Fnt);
  sil_destroySIL();


  return 0;
}
