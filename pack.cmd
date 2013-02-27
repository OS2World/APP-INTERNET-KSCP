@echo off
gmake clean
gmake RELEASE=1
git export > src.zip
zip kscp kscp.exe COPYING README README.eng src.zip
del src.zip
call name.cmd
