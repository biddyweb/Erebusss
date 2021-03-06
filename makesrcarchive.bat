rd /S /Q c:\temp\erebussrc\

mkdir c:\temp\erebussrc\

set src="."
set dst="c:\temp\erebussrc"

copy %src%\makesrcarchive.bat %dst%
copy %src%\makesymbianfolder.bat %dst%
copy %src%\makeandroidfolder.bat %dst%
copy %src%\erebus_source.txt %dst%

copy %src%\erebus.pro %dst%
copy %src%\deployment.pri %dst%
copy %src%\erebus.ico %dst%
copy %src%\resource.rc %dst%
copy %src%\erebus.svg %dst%
copy %src%\erebus64.png %dst%
copy %src%\erebus.desktop %dst%

copy %src%\animatedobject.cpp %dst%
copy %src%\game.cpp %dst%
copy %src%\gamestate.cpp %dst%
copy %src%\infodialog.cpp %dst%
copy %src%\logiface.cpp %dst%
copy %src%\main.cpp %dst%
copy %src%\maingraphicsview.cpp %dst%
copy %src%\mainwindow.cpp %dst%
copy %src%\optionsgamestate.cpp %dst%
copy %src%\particlesystem.cpp %dst%
copy %src%\playinggamestate.cpp %dst%
copy %src%\qt_screen.cpp %dst%
copy %src%\qt_utils.cpp %dst%
copy %src%\scrollinglistwidget.cpp %dst%
copy %src%\sound.cpp %dst%
copy %src%\test.cpp %dst%
copy %src%\webvieweventfilter.cpp %dst%

copy %src%\animatedobject.h %dst%
copy %src%\common.h %dst%
copy %src%\game.h %dst%
copy %src%\gamestate.h %dst%
copy %src%\infodialog.h %dst%
copy %src%\logiface.h %dst%
copy %src%\maingraphicsview.h %dst%
copy %src%\mainwindow.h %dst%
copy %src%\optionsgamestate.h %dst%
copy %src%\particlesystem.h %dst%
copy %src%\playinggamestate.h %dst%
copy %src%\qt_screen.h %dst%
copy %src%\qt_utils.h %dst%
copy %src%\scrollinglistwidget.h %dst%
copy %src%\sound.h %dst%
copy %src%\test.h %dst%
copy %src%\webvieweventfilter.h %dst%

mkdir %dst%\rpg\

copy %src%\rpg\character.cpp %dst%\rpg
copy %src%\rpg\item.cpp %dst%\rpg
copy %src%\rpg\location.cpp %dst%\rpg
copy %src%\rpg\locationgenerator.cpp %dst%\rpg
copy %src%\rpg\profile.cpp %dst%\rpg
copy %src%\rpg\rpgengine.cpp %dst%\rpg
copy %src%\rpg\utils.cpp %dst%\rpg

copy %src%\rpg\character.h %dst%\rpg
copy %src%\rpg\item.h %dst%\rpg
copy %src%\rpg\location.h %dst%\rpg
copy %src%\rpg\locationgenerator.h %dst%\rpg
copy %src%\rpg\profile.h %dst%\rpg
copy %src%\rpg\rpgengine.h %dst%\rpg
copy %src%\rpg\utils.h %dst%\rpg

mkdir %dst%\androidaudio\

copy %src%\androidaudio\androidaudio.cpp %dst%\androidaudio
copy %src%\androidaudio\androidaudio.h %dst%\androidaudio
copy %src%\androidaudio\androidsound.cpp %dst%\androidaudio
copy %src%\androidaudio\androidsound.h %dst%\androidaudio
copy %src%\androidaudio\androidsoundeffect.cpp %dst%\androidaudio
copy %src%\androidaudio\androidsoundeffect.h %dst%\androidaudio

REM also copy data folders, as Linux users won't have downloaded the Windows binary archive!

mkdir %dst%\docs\
copy %src%\docs\ %dst%\docs\

mkdir %dst%\data\
copy %src%\data\ %dst%\data\

mkdir %dst%\gfx
xcopy %src%\gfx %dst%\gfx /E /Y

mkdir %dst%\gfx_lores
xcopy %src%\gfx_lores %dst%\gfx_lores /E /Y

mkdir %dst%\sound
xcopy %src%\sound %dst%\sound /E /Y

mkdir %dst%\music
xcopy %src%\music %dst%\music /E /Y

mkdir %dst%\android
xcopy %src%\android %dst%\android /E /Y

mkdir %dst%\ts\
copy %src%\ts\ %dst%\ts\

mkdir %dst%\_test_savegames\
copy %src%\_test_savegames\ %dst%\_test_savegames\

REM exit
