rd /S /Q c:\temp\erebussrc\

mkdir c:\temp\erebussrc\

set src="."
set dst="c:\temp\erebussrc"

copy %src%\makesrcarchive.bat %dst%
copy %src%\makesymbianfolder.bat %dst%
copy %src%\makeandroidfolder.bat %dst%
REM copy %src%\erebus_source.txt %dst%
mkdir %dst%\_docs\
copy %src%\_docs\erebus.html %dst%\_docs\

copy %src%\erebus.pro %dst%
copy %src%\deployment.pri %dst%
copy %src%\erebus.qrc %dst%
copy %src%\erebus.ico %dst%
copy %src%\resource.rc %dst%
copy %src%\erebus.svg %dst%

copy %src%\gpl.txt %dst%

copy %src%\game.cpp %dst%
copy %src%\gamestate.cpp %dst%
copy %src%\main.cpp %dst%
copy %src%\mainwindow.cpp %dst%
copy %src%\optionsgamestate.cpp %dst%
copy %src%\playinggamestate.cpp %dst%
copy %src%\qt_screen.cpp %dst%
copy %src%\qt_utils.cpp %dst%

copy %src%\game.h %dst%
copy %src%\gamestate.h %dst%
copy %src%\mainwindow.h %dst%
copy %src%\optionsgamestate.h %dst%
copy %src%\playinggamestate.h %dst%
copy %src%\qt_screen.h %dst%
copy %src%\qt_utils.h %dst%

mkdir %dst%\rpg\

copy %src%\rpg\character.cpp %dst%\rpg
copy %src%\rpg\item.cpp %dst%\rpg
copy %src%\rpg\location.cpp %dst%\rpg
copy %src%\rpg\utils.cpp %dst%\rpg

copy %src%\rpg\character.h %dst%\rpg
copy %src%\rpg\item.h %dst%\rpg
copy %src%\rpg\location.h %dst%\rpg
copy %src%\rpg\utils.h %dst%\rpg

REM also copy data folders, as Linux users won't have downloaded the Windows binary archive!

mkdir %dst%\data\
copy %src%\data\ %dst%\data\

mkdir %dst%\gfx
REM copy %src%\gfx\ %dst%\gfx\
xcopy %src%\gfx %dst%\gfx /E /Y

mkdir %dst%\sound
xcopy %src%\sound %dst%\sound /E /Y

mkdir %dst%\music
xcopy %src%\music %dst%\music /E /Y

mkdir %dst%\android
xcopy %src%\android %dst%\android /E /Y

REM exit
