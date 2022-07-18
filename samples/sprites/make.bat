call ..\..\bin\oscar64 joycontrol.c
call ..\..\bin\oscar64 multiplexer.c -n
call ..\..\bin\oscar64 creditroll.c -n
call ..\..\bin\oscar64 -n sprmux32.c -O2 -dVSPRITES_MAX=32 -dNUM_IRQS=28
call ..\..\bin\oscar64 -n sprmux64.c
