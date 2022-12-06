#!/bin/sh
../../bin/oscar64 mbtext.c -n
../../bin/oscar64 mbhires.c -n
../../bin/oscar64 mbmulti.c -n
../../bin/oscar64 mbmulti3d.c -n
../../bin/oscar64 mbfixed.c -n -O3
../../bin/oscar64 mbzoom.c -n -O3

