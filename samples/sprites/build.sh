#!/bin/sh
../../bin/oscar64 joycontrol.c
../../bin/oscar64 multiplexer.c -n
../../bin/oscar64 creditroll.c -n
../../bin/oscar64 -n sprmux32.c -O2 -dVSPRITES_MAX=32 -dNUM_IRQS=28
../../bin/oscar64 -n sprmux64.c
