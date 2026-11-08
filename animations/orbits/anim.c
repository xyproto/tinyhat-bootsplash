#include <math.h>
#include "anim.h"

static const unsigned char inks[] = { INK_HIGHLIGHT, INK_CYAN, INK_LIGHT, INK_TEAL };

static void dot(double x, double y, unsigned char ink, int big)
{
	int px = (int)lround(x), py = (int)lround(y);
	plot(px, py, ink);
	if (big) {
		plot(px + 1, py, ink);
		plot(px, py + 1, ink);
		plot(px + 1, py + 1, ink);
	}
}

static void orbits(double t, double radius_scale, double spin)
{
	for (int o = 0; o < 4; o++) {
		double r = (8 + o * 6) * radius_scale, speed = (1.6 - o * 0.3) * spin;
		for (int i = 0; i < 24; i++) {
			double a = 2 * M_PI * i / 24;
			plot(LOW / 2 + (int)lround(r * cos(a)), LOW / 2 + (int)lround(r * sin(a) * 0.5), INK_GRAY);
		}
		double a = t * speed + o * 1.7;
		dot(LOW / 2 + r * cos(a), LOW / 2 + r * sin(a) * 0.5, inks[o], 1);
	}
	dot(LOW / 2 - 1, LOW / 2 - 1, INK_HAT, 1);
}

void boot_frame(long tick)
{
	clear(INK_BG);
	orbits(tick / TICKS_PER_SECOND, 1, 1);
}

void shutdown_frame(long tick)
{
	double t = tick / (2 * TICKS_PER_SECOND);
	clear(INK_BG);
	if (t < 1)
		orbits(tick / TICKS_PER_SECOND, 1 - t, 1 + 6 * t);
}
