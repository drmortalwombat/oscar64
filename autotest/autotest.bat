rem @echo off

@call :test loopunrolltest.cpp
@if %errorlevel% neq 0 goto :error

@call :test rolrortest.cpp
@if %errorlevel% neq 0 goto :error

@call :test maskcheck.c
@if %errorlevel% neq 0 goto :error

@call :test bitfields.cpp
@if %errorlevel% neq 0 goto :error

@call :testn autorefreturn.cpp
@if %errorlevel% neq 0 goto :error

@call :testh opp_string.cpp
@if %errorlevel% neq 0 goto :error

@call :testh opp_array.cpp
@if %errorlevel% neq 0 goto :error

@call :testh opp_vector.cpp
@if %errorlevel% neq 0 goto :error

@call :testh opp_static_vector.cpp
@if %errorlevel% neq 0 goto :error

@call :testh opp_vector_string.cpp
@if %errorlevel% neq 0 goto :error

@call :testh opp_string_init.cpp
@if %errorlevel% neq 0 goto :error

@call :testh opp_optional.cpp
@if %errorlevel% neq 0 goto :error

@call :testh opp_numeric.cpp
@if %errorlevel% neq 0 goto :error

@call :testh opp_streamtest.cpp
@if %errorlevel% neq 0 goto :error

@call :testh opp_pairtest.cpp
@if %errorlevel% neq 0 goto :error

@call :testh opp_parts.cpp
@if %errorlevel% neq 0 goto :error

@call :testh opp_list.cpp
@if %errorlevel% neq 0 goto :error

@call :testn opp_functional.cpp
@if %errorlevel% neq 0 goto :error

@call :testh operatoroverload.cpp
@if %errorlevel% neq 0 goto :error

@call :testh virtualdestruct.cpp
@if %errorlevel% neq 0 goto :error

@call :testh vcalltest.cpp
@if %errorlevel% neq 0 goto :error

@call :testh vcalltree.cpp
@if %errorlevel% neq 0 goto :error

@call :testh constructortest.cpp
@if %errorlevel% neq 0 goto :error

@call :testn copyconstructor.cpp
@if %errorlevel% neq 0 goto :error

@call :testh copyassign.cpp
@if %errorlevel% neq 0 goto :error

@call :testh arrayconstruct.cpp
@if %errorlevel% neq 0 goto :error

@call :testh stdlibtest.c
@if %errorlevel% neq 0 goto :error

@call :test mathtest.c
@if %errorlevel% neq 0 goto :error

@call :test testint16.c
@if %errorlevel% neq 0 goto :error

@call :test testint32.c
@if %errorlevel% neq 0 goto :error

@call :test testint16mul.c
@if %errorlevel% neq 0 goto :error

@call :test testsigned16mul.c
@if %errorlevel% neq 0 goto :error

@call :test testsigned16div.c
@if %errorlevel% neq 0 goto :error

@call :test recursiontest.c
@if %errorlevel% neq 0 goto :error

@call :test copyinitmove.c
@if %errorlevel% neq 0 goto :error

@call :test fastcalltest.c
@if %errorlevel% neq 0 goto :error

@call :test strlen.c
@if %errorlevel% neq 0 goto :error

@call :test strcmptest.c
@if %errorlevel% neq 0 goto :error

@call :test strcmptest2.c
@if %errorlevel% neq 0 goto :error

@call :test memmovetest.c
@if %errorlevel% neq 0 goto :error

@call :test arraytest.c
@if %errorlevel% neq 0 goto :error

@call :test arraytestfloat.c
@if %errorlevel% neq 0 goto :error

@call :test optiontest.c
@if %errorlevel% neq 0 goto :error

@call :test floatcmptest.c
@if %errorlevel% neq 0 goto :error

@call :test floatmultest.c
@if %errorlevel% neq 0 goto :error

@call :test floatinttest.c
@if %errorlevel% neq 0 goto :error

@call :test staticconsttest.c
@if %errorlevel% neq 0 goto :error

@call :test arrayinittest.c
@if %errorlevel% neq 0 goto :error

@call :test arrayindexintrangecheck.c
@if %errorlevel% neq 0 goto :error

@call :test array2stringinittest.c
@if %errorlevel% neq 0 goto :error

@call :test testint16cmp.c
@if %errorlevel% neq 0 goto :error

@call :test testint8cmp.c
@if %errorlevel% neq 0 goto :error

@call :test testint32cmp.c
@if %errorlevel% neq 0 goto :error

@call :test mixsigncmptest.c
@if %errorlevel% neq 0 goto :error

@call :test testinterval.c
@if %errorlevel% neq 0 goto :error

@call :test cmprangeshortcuttest.c
@if %errorlevel% neq 0 goto :error

@call :test floatstringtest.c
@if %errorlevel% neq 0 goto :error

@call :test sprintftest.c
@if %errorlevel% neq 0 goto :error

@call :test qsorttest.c
@if %errorlevel% neq 0 goto :error

@call :testn plasma.c
@if %errorlevel% neq 0 goto :error

@call :test loopdomtest.c
@if %errorlevel% neq 0 goto :error

@call :test loopboundtest.c
@if %errorlevel% neq 0 goto :error

@call :test byteindextest.c
@if %errorlevel% neq 0 goto :error

@call :test asmtest.c
@if %errorlevel% neq 0 goto :error

@call :testb bitshifttest.c
@if %errorlevel% neq 0 goto :error

@call :test arrparam.c
@if %errorlevel% neq 0 goto :error

@call :test bsstest.c
@if %errorlevel% neq 0 goto :error

@call :test copyintvec.c
@if %errorlevel% neq 0 goto :error

@call :test divmodtest.c
@if %errorlevel% neq 0 goto :error

@call :test divmod32test.c
@if %errorlevel% neq 0 goto :error

@call :test fixmathtest.c
@if %errorlevel% neq 0 goto :error

@call :testn andmultest.cpp
@if %errorlevel% neq 0 goto :error

@call :test enumswitch.c
@if %errorlevel% neq 0 goto :error

@call :test incvector.c
@if %errorlevel% neq 0 goto :error

@call :test structoffsettest2.c
@if %errorlevel% neq 0 goto :error

@call :test structsplittest.c
@if %errorlevel% neq 0 goto :error

@call :test funcvartest.c
@if %errorlevel% neq 0 goto :error

@call :test funcarraycall.c
@if %errorlevel% neq 0 goto :error

@call :test structassigntest.c
@if %errorlevel% neq 0 goto :error

@call :test structmembertest.c
@if %errorlevel% neq 0 goto :error

@call :test structarraycopy.c
@if %errorlevel% neq 0 goto :error

@call :test randsumtest.c
@if %errorlevel% neq 0 goto :error

@call :test longcodetest.c
@if %errorlevel% neq 0 goto :error

@call :test scrolltest.c
@if %errorlevel% neq 0 goto :error

@call :test charwintest.c
@if %errorlevel% neq 0 goto :error

@call :test linetest.c
@if %errorlevel% neq 0 goto :error

@call :test ptrinittest.c
@if %errorlevel% neq 0 goto :error

@call :test ptrarraycmptest.c
@if %errorlevel% neq 0 goto :error

@call :test cplxstructtest.c
@if %errorlevel% neq 0 goto :error

@call :testn stripedarraytest.c
@if %errorlevel% neq 0 goto :error

@call :testn mmultest.c
@if %errorlevel% neq 0 goto :error

@call :test tileexpand.cpp
@if %errorlevel% neq 0 goto :error

@exit /b 0

:error
echo Failed with error #%errorlevel%.
exit /b %errorlevel%

:testh
..\bin\oscar64 -ea -g -bc %~1
@if %errorlevel% neq 0 goto :error

..\bin\oscar64 -ea -g -n %~1
@if %errorlevel% neq 0 goto :error

..\bin\oscar64 -ea -g -O2 -bc %~1
@if %errorlevel% neq 0 goto :error

..\bin\oscar64 -ea -g -O2 -n %~1
@if %errorlevel% neq 0 goto :error

..\bin\oscar64 -ea -g -O2 -n -dHEAPCHECK %~1
@if %errorlevel% neq 0 goto :error

..\bin\oscar64 -ea -g -O2 -xz -Oz -n %~1
@if %errorlevel% neq 0 goto :error

..\bin\oscar64 -ea -g -O2 -Oo -n %~1
@if %errorlevel% neq 0 goto :error

..\bin\oscar64 -ea -g -O2 -Ox -n %~1
@if %errorlevel% neq 0 goto :error

..\bin\oscar64 -ea -g -O0 -bc %~1
@if %errorlevel% neq 0 goto :error

..\bin\oscar64 -ea -g -O0 -n %~1
@if %errorlevel% neq 0 goto :error

..\bin\oscar64 -ea -g -Os -bc %~1
@if %errorlevel% neq 0 goto :error

..\bin\oscar64 -ea -g -Os -n %~1
@if %errorlevel% neq 0 goto :error

..\bin\oscar64 -ea -g -O3 -bc %~1
@if %errorlevel% neq 0 goto :error

..\bin\oscar64 -ea -g -O3 -n %~1
@if %errorlevel% neq 0 goto :error

..\bin\oscar64 -ea -g -O3 -n -dHEAPCHECK %~1
@if %errorlevel% neq 0 goto :error

@exit /b 0

:test
..\bin\oscar64 -ea -g -bc %~1
@if %errorlevel% neq 0 goto :error

..\bin\oscar64 -ea -g -n %~1
@if %errorlevel% neq 0 goto :error

..\bin\oscar64 -ea -g -O2 -bc %~1
@if %errorlevel% neq 0 goto :error

..\bin\oscar64 -ea -g -O2 -n %~1
@if %errorlevel% neq 0 goto :error

..\bin\oscar64 -ea -g -O0 -bc %~1
@if %errorlevel% neq 0 goto :error

..\bin\oscar64 -ea -g -O0 -n %~1
@if %errorlevel% neq 0 goto :error

..\bin\oscar64 -ea -g -Os -bc %~1
@if %errorlevel% neq 0 goto :error

..\bin\oscar64 -ea -g -Os -n %~1
@if %errorlevel% neq 0 goto :error

..\bin\oscar64 -ea -g -O3 -bc %~1
@if %errorlevel% neq 0 goto :error

..\bin\oscar64 -ea -g -O3 -n %~1
@if %errorlevel% neq 0 goto :error

..\bin\oscar64 -ea -g -O2 -xz -Oz -n %~1
@if %errorlevel% neq 0 goto :error

..\bin\oscar64 -ea -g -O2 -Oo -n %~1
@if %errorlevel% neq 0 goto :error

..\bin\oscar64 -ea -g -O2 -Ox -n %~1
@if %errorlevel% neq 0 goto :error

@exit /b 0

:testb
..\bin\oscar64 -ea -g -bc %~1
@if %errorlevel% neq 0 goto :error

..\bin\oscar64 -ea -g -bc -O2 %~1
@if %errorlevel% neq 0 goto :error

..\bin\oscar64 -ea -g -bc -O0 %~1
@if %errorlevel% neq 0 goto :error

..\bin\oscar64 -ea -g -bc -Os %~1
@if %errorlevel% neq 0 goto :error

..\bin\oscar64 -ea -g -bc -O3 %~1
@if %errorlevel% neq 0 goto :error

@exit /b 0

:testn
..\bin\oscar64 -ea -g -n %~1
@if %errorlevel% neq 0 goto :error

..\bin\oscar64 -ea -g -O2 -n %~1
@if %errorlevel% neq 0 goto :error

..\bin\oscar64 -ea -g -O0 -n %~1
@if %errorlevel% neq 0 goto :error

..\bin\oscar64 -ea -g -Os -n %~1
@if %errorlevel% neq 0 goto :error

..\bin\oscar64 -ea -g -O3 -n %~1
@if %errorlevel% neq 0 goto :error

..\bin\oscar64 -ea -g -O2 -xz -Oz -n %~1
@if %errorlevel% neq 0 goto :error

..\bin\oscar64 -ea -g -O2 -Oo -n %~1
@if %errorlevel% neq 0 goto :error

..\bin\oscar64 -ea -g -O2 -Ox -n %~1
@if %errorlevel% neq 0 goto :error

@exit /b 0
