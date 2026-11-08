#include <stdlib.h>
#include <string.h>
#include "anim.h"

unsigned char low[LOW * LOW];

void clear(unsigned char ink)
{
	memset(low, ink, sizeof(low));
}

void plot(int x, int y, unsigned char ink)
{
	if (x >= 0 && y >= 0 && x < LOW && y < LOW)
		low[y * LOW + x] = ink;
}

void line(int x0, int y0, int x1, int y1, unsigned char ink)
{
	int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1, dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1, e = dx + dy;
	for (;;) {
		plot(x0, y0, ink);
		if (x0 == x1 && y0 == y1)
			break;
		int e2 = 2 * e;
		if (e2 >= dy) {
			e += dy;
			x0 += sx;
		}
		if (e2 <= dx) {
			e += dx;
			y0 += sy;
		}
	}
}
