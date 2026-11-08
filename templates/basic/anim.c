#include <math.h>
#include "anim.h"

void boot_frame(long tick)
{
	double t = tick / TICKS_PER_SECOND;
	int x = LOW / 2 + (int)lround(20 * cos(t * 2)), y = LOW / 2 + (int)lround(12 * sin(t * 3));
	clear(INK_BG);
	line(LOW / 2, LOW / 2, x, y, INK_TEAL);
	plot(x, y, INK_HIGHLIGHT);
}

void shutdown_frame(long tick)
{
	double t = tick / (2 * TICKS_PER_SECOND);
	int r = (int)lround(20 * (1 - t));
	clear(INK_BG);
	if (r > 0) {
		line(LOW / 2 - r, LOW / 2, LOW / 2 + r, LOW / 2, INK_HIGHLIGHT);
		line(LOW / 2, LOW / 2 - r, LOW / 2, LOW / 2 + r, INK_HIGHLIGHT);
	}
}
