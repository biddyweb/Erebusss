rd /S /Q c:\temp\erebus\

mkdir c:\temp\erebus\

set src="."
set docsrc="_docs\"
set dllsrc="..\_windows_release_dlls_4_7_3_msvc2008\"
set dst="c:\temp\erebus"

copy ..\erebus-build-desktop\release\erebus.exe %dst%\erebus.exe
copy %docsrc%\erebus.html %dst%\readme.html
copy %src%\gpl.txt %dst%

copy %dllsrc%\QtCore4.dll %dst%
copy %dllsrc%\QtGui4.dll %dst%
copy %dllsrc%\QtNetwork4.dll %dst%
copy %dllsrc%\QtWebKit4.dll %dst%
copy %dllsrc%\QtXml4.dll %dst%
copy %dllsrc%\phonon4.dll %dst%

mkdir %dst%\music\
copy %src%\music\ %dst%\music\

REM exit
