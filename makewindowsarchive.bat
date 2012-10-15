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
copy %dllsrc%\phonon4.dll %dst%

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

REM exit
