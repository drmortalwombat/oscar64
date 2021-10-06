@echo off

..\release\oscar64 -e stdlibtest.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -e -n stdlibtest.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -e testint16.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -e -n testint16.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -e testint32.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -e -n testint32.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -e recursiontest.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -e -n recursiontest.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -e strcmptest.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -e -n strcmptest.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -e strcmptest2.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -e -n strcmptest2.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -e arraytest.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -e -n arraytest.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -e arraytestfloat.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -e -n arraytestfloat.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -e optiontest.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -e -n optiontest.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -e floatcmptest.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -e -n floatcmptest.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -e floatmultest.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -e -n floatmultest.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -e staticconsttest.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -e -n staticconsttest.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -e arrayinittest.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -e -n arrayinittest.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -e array2stringinittest.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -e -n array2stringinittest.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -e testint16cmp.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -e -n testint16cmp.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -e testint32cmp.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -e -n testint32cmp.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -e floatstringtest.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -e -n floatstringtest.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -e qsorttest.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -e -n qsorttest.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -e loopdomtest.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -e -n loopdomtest.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -e byteindextest.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -e -n byteindextest.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -e asmtest.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -e -n asmtest.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -e bitshifttest.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -e -n bitshifttest.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -e arrparam.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -e -n arrparam.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -e bsstest.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -e -n bsstest.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -e copyintvec.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -e -n copyintvec.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -e divmodtest.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -e -n divmodtest.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -e enumswitch.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -e -n enumswitch.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -e incvector.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -e -n incvector.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -e structoffsettest2.c
if %errorlevel% neq 0 goto :error

..\release\oscar64 -e -n structoffsettest2.c
if %errorlevel% neq 0 goto :error



exit /b 0
:error
echo Failed with error #%errorlevel%.
exit /b %errorlevel%
