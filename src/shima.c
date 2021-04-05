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
static SILLYR *creatorLyr,*descLyr,*titleLyr;
static SILFONT *mm32Fnt,*ss16Fnt,*ad24Fnt,*ado28Fnt;
static int selected=0;
static char **inifiles=NULL;
static int iniamount=0;
static int gamestate=0;

/* cleanup of string, strips spaces and /r and /n */
char *sclean(char *l) {
  while((isblank(*l))&&(*l!='\0')) l++;
  /* chop newlines and all */
  for (int i=0;i<strlen(l);i++) {
    if ((l[i]=='\r')||(l[i]=='\n')) l[i]='\0';
  }
  /* insert an enter, if it is to long */
  if (strlen(l)>50) {
    for (int i=50;i>0;i--) {
      if (isblank(l[i])) {
        l[i]='\n';
        i=0;
      }
    }
  }
  return l;
}

PUZZLE puzzle;

/* parse .ini file */

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

    /* open .ini file */
  memset(&file[0],0,sizeof(file));
  strcat(file,"puzzles/");
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
        puzzle.start[i][cnt]=k[i];
        puzzle.pos[i][cnt]=k[i];
      }
      cnt++;
      if (cnt>puzzle.maxy) puzzle.maxy=cnt;
      if (strlen(k)>puzzle.maxx) puzzle.maxx=strlen(k);
      continue;
    } 
    if (PARSEGOAL==state) {
      if (0==strlen(k)) {
        state=PARSEKEYWORD;
        continue;
      }
      for (int i=0;i<strlen(k);i++) {
        puzzle.goal[i][cnt]=k[i];
      }
      cnt++;
      if (cnt>puzzle.maxy) puzzle.maxy=cnt;
      if (strlen(k)>puzzle.maxx) puzzle.maxx=strlen(k);
      continue;
    }


    if(strcmp(k,"title")==0) {
      strcpy(puzzle.title,v);
      continue;
    }
    if(strcmp(k,"creator")==0) {
      strcpy(puzzle.creator,v);
      continue;
    }
    if(strcmp(k,"description")==0) {
      strcpy(puzzle.description,v);
      continue;
    }
    if(strcmp(k,"preview")==0) {
      strcpy(puzzle.description,v);
      continue;
    }
    if(strcmp(k,"background")==0) {
      strcpy(puzzle.background,v);
      continue;
    }
    if(strcmp(k,"pieces")==0) {
      strcpy(puzzle.background,v);
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


/* show info of selected puzzle on selection screen */

void showInfo(char *filename) {
  FILE *fp;
  char file[256];
  char line[256];
  char *l=NULL;
  const char *titleTxt="title:";
  const char *creatorTxt="creator:";
  const char *descTxt="description:";

  /* open .ini file */
  memset(&file[0],0,sizeof(file));
  strcat(file,"puzzles/");
  strncat(file,filename,sizeof(file)-strlen(file)-1);
  fp=fopen(file,"r");
  if (NULL==fp) {
    log_warn("Can't open %s (%s)",filename,file);
    return;
  }

  sil_hide(titleLyr);
  sil_hide(creatorLyr);
  sil_hide(descLyr);

  /* get keywords out of ini file */

  while(!feof(fp)) {
    memset(&line[0],0,sizeof(line));
    l=fgets(&line[0],sizeof(line)-1,fp);
    if (NULL!=l) {
      while((isblank(*l))&&(*l!='\0')) l++;
      if(strncmp(l,titleTxt,strlen(titleTxt))==0) {
        l+=strlen(titleTxt);
        l=sclean(l);
        sil_paintLayer(titleLyr,SILCOLOR_BLACK,0);
        sil_setForegroundColor(SILCOLOR_PALE_GOLDEN_ROD,255);
        sil_drawText(titleLyr,mm32Fnt,l,0,0,0);
        sil_show(titleLyr);
      } else {
        if(strncmp(l,creatorTxt,strlen(creatorTxt))==0) {
          l+=strlen(creatorTxt);
          l=sclean(l);
          sil_paintLayer(creatorLyr,SILCOLOR_BLACK,0);
          sil_drawText(creatorLyr,ss16Fnt,l,0,0,SILTXT_KEEPCOLOR);
          sil_show(creatorLyr);
        } else {
          if(strncmp(l,descTxt,strlen(descTxt))==0) {
            l+=strlen(descTxt);
            l=sclean(l);
            sil_paintLayer(descLyr,SILCOLOR_BLACK,0);
            sil_drawText(descLyr,ss16Fnt,l,0,0,SILTXT_KEEPCOLOR);
            sil_show(descLyr);
          }
        }
      } 
    }
  }
  fclose(fp);
}


/* show preview image out of x,y position of selectLyr */

void showPreview(SILLYR *layer,char *filename) {
  FILE *fp;
  char file[256];
  char line[256];
  char *l=NULL;
  const char *preview="preview:";

  /* open .ini file */
  memset(&file[0],0,sizeof(file));
  strcat(file,"puzzles/");
  strncat(file,filename,sizeof(file)-strlen(file)-1);
  fp=fopen(file,"r");
  if (NULL==fp) {
    log_warn("Can't open %s (%s)",filename,file);
    return;
  }

  /* get preview image out of ini file */

  memset(&file[0],0,sizeof(file));
  while(!feof(fp)&&(0==file[0])) {
    memset(&line[0],0,sizeof(line));
    l=fgets(&line[0],sizeof(line)-1,fp);
    if (NULL!=l) {
      while((isblank(*l))&&(*l!='\0')) l++;
      if(strncmp(l,preview,strlen(preview))==0) {
        l+=strlen(preview);
        while((isblank(*l))&&(*l!='\0')) l++;
        strcat(file,"puzzles/");
        /* chop newlines and all */
        for (int i=0;i<strlen(l);i++) {
          if ((l[i]=='\r')||(l[i]=='\n')) l[i]='\0';
        }
        strncat(file,l,sizeof(file)-strlen(file)-1);
      }
    }
  }
  fclose(fp);

  /* no preview keyword found or empty ? , no preview for you */
  if (0==file[0]) return;

  /* display found file */
  sil_PNGintoLayer(layer,file,0,0);
  sil_show(layer);

  fclose(fp);
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

  if (NULL==inifiles) {

    /* sorted list of .ini files not yet loaded, so get them */

    /* first, get amount of .ini files */
    pzls=opendir("puzzles");
    if (NULL==pzls) {
      log_warn("Can't open puzzles directory");
      return;
    }

    inifiles=malloc(iniamount * sizeof(*inifiles));
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

    /* now get all ini names, sorted */
    pzls=opendir("puzzles");
    if (NULL==pzls) {
      log_warn("Can't open puzzles directory");
      return;
    }

    inifiles=malloc(iniamount * sizeof(*inifiles));
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


  if (selected>1) {
    showPreview(firstLyr,inifiles[selected-2]);
  }  else {
    sil_hide(firstLyr);
  }
  if (selected>0) {
    showPreview(prevLyr,inifiles[selected-1]);
    sil_show(btn_prevLyr);
  }  else {
    sil_hide(prevLyr);
  }
  showPreview(selLyr,inifiles[selected]);
  showInfo(inifiles[selected]);
  if (selected+1<iniamount) {
    showPreview(nextLyr,inifiles[selected+1]);
    sil_show(btn_nextLyr);
  }  else {
    sil_hide(nextLyr);
  }
  if (selected+2<iniamount) {
    showPreview(lastLyr,inifiles[selected+2]);
  }  else {
    sil_hide(lastLyr);
  }
  if (0==selected) {
    sil_setAlphaLayer(btn_prevLyr,0.3);
    sil_hide(btn_prevLyr);
  }
  if (selected==iniamount-1) {
    sil_setAlphaLayer(btn_nextLyr,0.3);
    sil_hide(btn_nextLyr);
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
        sil_saveDisplay("printscreen.png",1000,800,0,0);
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
void initBackground() {
  bgLyr=sil_PNGtoNewLayer("game_background.png",0,0);
  sil_setKeyHandler(bgLyr,0,0,0,keyhandler);
}

void initPuzzleSelect() {
  ado28Fnt=sil_loadFont("architectsdaughter_thickoutline_28px.fnt");
  ad24Fnt=sil_loadFont("architectsdaughter_24px.fnt");
  ss16Fnt=sil_loadFont("sourcesanspro_16px.fnt");
  mm32Fnt=sil_loadFont("mm_32px.fnt");

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

  creatorLyr=sil_addLayer(375,35,350,640,SILTYPE_ARGB);
  sil_hide(creatorLyr);
  descLyr=sil_addLayer(375,35,350,600,SILTYPE_ARGB);
  sil_hide(descLyr);
  titleLyr=sil_addLayer(375,35,350,520,SILTYPE_ARGB);
  sil_hide(titleLyr);
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

void puzzleSolving() {
  hidePuzzleSelect();
  parseIni(inifiles[selected]);
  sil_updateDisplay();
}


#ifdef SIL_W32
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
#else
int main() {
  void *hInstance=NULL;
#endif

  SILEVENT *se=NULL;


  sil_initSIL(1000,800,"Shifting Madness",hInstance);
  sil_setLog(NULL,LOG_INFO|LOG_DEBUG|LOG_VERBOSE);

  initBackground();
  getPuzzles();
  initPuzzleSelect();


  puzzleSelection();

  sil_mainLoop();

  sil_destroyFont(ad24Fnt);
  sil_destroyFont(ado28Fnt);
  sil_destroySIL();


  return 0;
}
