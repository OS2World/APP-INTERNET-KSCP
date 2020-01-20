@echo off
setlocal

SET KSCP_SRC=src.zip
SET SRC_LIST=srclist

SET KPMLIB_DIR=KPMLib

gmake clean
gmake RELEASE=1

git ls-files > %SRC_LIST

%cd %KPMLIB_DIR%
git ls-files | awk '{ print "%KPMLIB_DIR%/"$1 }' >> ..\%SRC_LIST%
cd ..

type %SRC_LIST% | zip %KSCP_SRC% -@

zip kscp kscp.exe COPYING README README.eng %KSCP_SRC%
del %KSCP_SRC% %SRC_LIST%

call name.cmd

endlocal
