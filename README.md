# SHIMA
Shifting Madness , program to create and play difficult sliding puzzles. Used as showcase for SIL , a small multi-platform simpel GUI library.

<img src="https://github.com/mertyGit/shima/releases/download/beta1.0/shima_1beta_screenshot1.png" width="300">&nbsp;<img src="https://github.com/mertyGit/shima/releases/download/beta1.0/shima_1beta_screenshot2.png" width="300">&nbsp;<img src="https://github.com/mertyGit/shima/releases/download/beta1.0/shima_1beta_screenshot4.png" width="300">&nbsp;<BR>

# How to download

Check releases page. Latest release is a ["beta" release for windows only](https://github.com/mertyGit/shima/releases/download/beta1.0/Shima_Windows_v1_Beta.zip), more platforms will follow. 
Installing is easy, just unpack the .zip file to directory you like, and start the shima.exe ....thats it..

# How to create your own puzzle

Create an ".ini" file and place it in the "puzzles" directory right under the directory shima executable is in. 
The filename has to have the .ini extension, but can have every naming scheme you like, however, shima will read and show puzzles based on alphabetical order.
The fileformat is just plain ASCII. It consists out of keyword + ':' + value. Keyword are case-insensitive
Newlines are considered 'end of keyword/value pair' except for 'start' and 'finish' they will end with an empty line. lines starting with '#' are remarks and ignored. All png files should be included in the puzzles directory itselves.

The following keywords can be used:
* **title** => title to display on selection screen and when playing
* **goal** => text for "goal" info on selection screen
* **creator** => creator information on selection screen
* **preview** => 100px x 100px png image (within puzzle directory !) of preview picture, as will be seen in selection screen. 
* **background** => 600px x 600px png image of the "board" or "non movable background" of the puzzle. The "shiftable pieces" will be placed on top of it.
* **pieces** => png image of a combination of the pieces in the "start" position. SHIMA will "cut" them up and makes it moveable pieces using the "start" information** (can be any size, but of couse smaller then 600px x 600px)
* **Info** => 
* **pospieces** => left upper corner offset (x, y) of background picture the pieces will be placed on (like inside the border drawn in the background pic)
* **shufflefirst** => -optional- before starting, shuffle the start position randomly (but always the same). Usefull for "simple" sliding puzzles.
* **difficulty** => amount of "stars" to show on puzzle selection screen
* **movement** => -optional- restricts movement of the piece, not only by space available, but also direction. At the moment only "shortest" (move only to sides that has the shortest length of the block) or "longest" (opposite). This is used in the "Rush Hour" puzzle, to make cars only move forward or backwards, but not sideways.
* **Start** => On new lines under it, "draw" the puzzle using A-Z,a-z or 0-9 as identification of all squares that can be considered as part of a piece. Special character '+' can be used for "fixed" pieces (pieces/obstacles that can't be moved at all) , underscore can be used for space between pieces
* **Start** => On new lines under it, "draw" the finished puzzle / objective. If not all positions of pieces are important, fill it up with dot ('.') as a kind of "wildcard"

Example of an INI file ("0_02easy4x4_picture"):
```
title:Easy Slider with Picture
goal:Fix the picture
creator: Remco Schellekens, photo by Hans-Peter Gauster
preview:easy_4x4_picture_preview.png
background:easy_board.png
info: easy_4x4_info.png
pieces:easy_4x4_picture_pieces.png
pospieces: 50,50
shufflefirst:yes
difficulty:1


# A-Za-z0-9 -> pieces _=space +=fixed piece .="doesn't matter"
start:
1234
5678
9ABC
DEF_

finish:
1234
5678
9ABC
DEF_
```

Accompanied by:
**easy_4x4_picture_preview.png** <BR>
<IMG SRC="https://github.com/mertyGit/shima/blob/main/bin/puzzles/easy_4x4_picture_preview.png" WIDTH="100"><BR>

**easy_board.png** (shared with "easy_4x4" puzzle) <BR>
<IMG SRC="https://github.com/mertyGit/shima/blob/main/bin/puzzles/easy_board.png" WIDTH="200"><BR>

**easy_4x4_picture_pieces.png** <BR>
<IMG SRC="https://github.com/mertyGit/shima/blob/main/bin/puzzles/easy_4x4_picture_pieces.png" WIDTH="200"><BR>
    
**easy_4x4_info.png** (also shared with "easy_4x4" puzzle) <BR>
<IMG SRC="https://github.com/mertyGit/shima/blob/main/bin/puzzles/easy_4x4_info.png" WIDTH="200"><BR>
