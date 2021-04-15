E = .exe
DISP = winGDIdisplay
#CFLAGS = -mconsole -lgdi32 -DSIL_W32
CFLAGS = -mwindows -lgdi32 -DSIL_W32
CC = x86_64-w64-mingw32-gcc
ICO = ico.rco
DEBUG = -g
DEPS = sil.h log.h lodepng.h
OBJ = log.o sil.o framebuffer.o layer.o drawing.o filter.o lodepng.o font.o group.o pcg_basic.o
CFLAGS +=-I ./

$(PROG)$E: $(PROG).o $(OBJ) $(DISP).o 
	$(CC) $(ICO) -o ../bin/$@ $^ $(CFLAGS)

$(PROG).o: $(PROG).c $(DEPS)
	$(CC) $(DEBUG) -c $< -o $@ $(CFLAGS)

%.o: %.c $(DEPS)
	$(CC) $(DEBUG) -c $< -o $@ $(CFLAGS)

gdi: clean

%: 
	$(MAKE) PROG=shima

clean: 
	rm -rf ../bin/*.exe ../bin/*.o *.o 
