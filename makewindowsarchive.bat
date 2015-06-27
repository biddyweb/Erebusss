rd /S /Q c:\temp\erebus\

mkdir c:\temp\erebus\

set src="."
REM set dllsrc="..\_windows_release_dlls_4_7_3_msvc2008\"
REM set dllsrc="..\_windows_release_dlls_4_8_5_msvc2008\"
set dllsrc="..\_windows_release_dlls_5_3_msvc2012_opengl_32bit\"
set dst="c:\temp\erebus"

REM copy ..\erebus-build-desktop\release\erebus.exe %dst%\erebus.exe
copy ..\build-erebus-desktop-Release\release\erebus.exe %dst%\erebus.exe

REM Qt 4:
REM copy %dllsrc%\QtCore4.dll %dst%
REM copy %dllsrc%\QtGui4.dll %dst%
REM No longer using QtWebKit
REM copy %dllsrc%\QtWebKit4.dll %dst%
REM copy %dllsrc%\QtXml4.dll %dst%
REM N.B., we still need phonon4.dll even when using SFML, as it's still needed for QtWebKit
REM Though no longer using QtWebKit...
REM copy %dllsrc%\phonon4.dll %dst%
REM mkdir %dst%\plugins\
REM mkdir %dst%\plugins\imageformats\
REM copy %dllsrc%\plugins\imageformats\qjpeg4.dll %dst%\plugins\imageformats\

REM Qt 5:
copy %dllsrc%\icudt52.dll %dst%
copy %dllsrc%\icuin52.dll %dst%
copy %dllsrc%\icuuc52.dll %dst%
copy %dllsrc%\Qt5Core.dll %dst%
copy %dllsrc%\Qt5Gui.dll %dst%
copy %dllsrc%\Qt5Widgets.dll %dst%
mkdir %dst%\plugins\
mkdir %dst%\plugins\imageformats\
copy %dllsrc%\plugins\imageformats\qjpeg.dll %dst%\plugins\imageformats\
REM platforms shouldn't be in plugins folder, see:
REM http://stackoverflow.com/questions/21268558/application-failed-to-start-because-it-could-not-find-or-load-the-qt-platform-pl
REM https://forum.qt.io/topic/22825/solved-failed-to-load-platform-plugin/15
mkdir %dst%\platforms\
copy %dllsrc%\platforms\qwindows.dll %dst%\platforms\

REM For SFML (libsndfile-1 no longer needed with SFML 2.3):
REM copy libsndfile-1.dll %dst%
copy openal32.dll %dst%
copy sfml-audio-2.dll %dst%
copy sfml-system-2.dll %dst%

REM For Phonon:
REM mkdir %dst%\plugins\phonon_backend\
REM copy %dllsrc%\plugins\phonon_backend\phonon_ds94.dll %dst%\plugins\phonon_backend\

mkdir %dst%\docs\
copy %src%\docs\ %dst%\docs\

mkdir %dst%\data\
copy %src%\data\ %dst%\data\

mkdir %dst%\gfx
xcopy %src%\gfx %dst%\gfx /E /Y

mkdir %dst%\sound
xcopy %src%\sound %dst%\sound /E /Y

mkdir %dst%\music
xcopy %src%\music %dst%\music /E /Y

mkdir %dst%\ts\
copy %src%\ts\ %dst%\ts\

REM exit
