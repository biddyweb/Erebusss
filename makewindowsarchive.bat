rd /S /Q c:\temp\erebus\

mkdir c:\temp\erebus\

set src="."
set dllsrc="..\_windows_release_dlls_4_7_3_msvc2008\"
set dst="c:\temp\erebus"

copy ..\erebus-build-desktop\release\erebus.exe %dst%\erebus.exe

copy %dllsrc%\QtCore4.dll %dst%
copy %dllsrc%\QtGui4.dll %dst%
copy %dllsrc%\QtNetwork4.dll %dst%
copy %dllsrc%\QtWebKit4.dll %dst%
copy %dllsrc%\QtXml4.dll %dst%
REM N.B., we still need phonon4.dll even when using SFML, as it's still needed for QtWebKit
copy %dllsrc%\phonon4.dll %dst%
REM For SFML:
copy libsndfile-1.dll %dst%
copy openal32.dll %dst%
copy sfml-audio-2.dll %dst%
copy sfml-system-2.dll %dst%

mkdir %dst%\plugins\
mkdir %dst%\plugins\imageformats\
copy %dllsrc%\plugins\imageformats\qjpeg4.dll %dst%\plugins\imageformats\
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
