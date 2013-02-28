@echo off
setlocal

SET PACK_DIR=pack
SET KSCP_SRC=src.zip
SET KPMLIB_SRC=kpmlibsrc.zip
SET KPMLIB_DIR=KPMLib

gmake clean
gmake RELEASE=1

git export > %KSCP_SRC%

cd KPMLib
git export > ..\%KPMLIB_SRC%
cd ..

md %PACK_DIR%
cd %PACK_DIR%
unzip ..\%KPMLIB_SRC% -d %KPMLIB_DIR%
del ..\%KPMLIB_SRC%
zip -rpSm ..\%KSCP_SRC% %KPMLIB_DIR%
cd ..
rd %PACK_DIR%

zip kscp kscp.exe COPYING README README.eng %KSCP_SRC%
del %KSCP_SRC%

call name.cmd

endlocal
