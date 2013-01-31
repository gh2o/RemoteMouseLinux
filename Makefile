remotemouse: remotemouse.c
	gcc -Os -ggdb $^ -o $@ -lX11 -lXtst -lm -Wall -ffast-math
