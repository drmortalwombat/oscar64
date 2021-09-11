@echo off

..\release\oscar64 -i=../include -rt=../include/crt.c -e stdlibtest.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -i=../include -rt=../include/crt.c -e -n stdlibtest.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -i=../include -rt=../include/crt.c -e testint16.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -i=../include -rt=../include/crt.c -e -n testint16.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -i=../include -rt=../include/crt.c -e recursiontest.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -i=../include -rt=../include/crt.c -e -n recursiontest.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -i=../include -rt=../include/crt.c -e strcmptest.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -i=../include -rt=../include/crt.c -e -n strcmptest.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -i=../include -rt=../include/crt.c -e arraytest.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -i=../include -rt=../include/crt.c -e -n arraytest.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -i=../include -rt=../include/crt.c -e arraytestfloat.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -i=../include -rt=../include/crt.c -e -n arraytestfloat.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -i=../include -rt=../include/crt.c -e optiontest.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -i=../include -rt=../include/crt.c -e -n optiontest.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -i=../include -rt=../include/crt.c -e floatcmptest.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -i=../include -rt=../include/crt.c -e -n floatcmptest.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -i=../include -rt=../include/crt.c -e floatmultest.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -i=../include -rt=../include/crt.c -e -n floatmultest.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -i=../include -rt=../include/crt.c -e staticconsttest.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -i=../include -rt=../include/crt.c -e -n staticconsttest.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -i=../include -rt=../include/crt.c -e arrayinittest.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -i=../include -rt=../include/crt.c -e -n arrayinittest.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -i=../include -rt=../include/crt.c -e array2stringinittest.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -i=../include -rt=../include/crt.c -e -n array2stringinittest.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -i=../include -rt=../include/crt.c -e testint16cmp.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -i=../include -rt=../include/crt.c -e -n testint16cmp.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -i=../include -rt=../include/crt.c -e floatstringtest.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -i=../include -rt=../include/crt.c -e -n floatstringtest.c
if %errorlevel% neq 0 goto :error

exit /b 0
:error
echo Failed with error #%errorlevel%.
exit /b %errorlevel%
