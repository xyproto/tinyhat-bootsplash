#include <math.h>
#include "anim.h"

#define SEGMENTS 16

typedef struct {
	int x, y;
	double z;
} Point;

static double tilt = 0.38, size = 25, lift = 0;

static Point project(double angle, double x, double y, double z)
{
	double x1 = x * cos(angle) + z * sin(angle), z1 = -x * sin(angle) + z * cos(angle);
	double y1 = y - 0.45;
	double y2 = y1 * cos(tilt) - z1 * sin(tilt), z2 = y1 * sin(tilt) + z1 * cos(tilt);
	double f = 3.4 / (z2 + 3.4);
	Point p = { (int)lround(LOW / 2 + x1 * f * size), (int)lround(LOW / 2 - lift - y2 * f * size), z2 };
	return p;
}

static void edge(int pass, Point a, Point b, unsigned char front, unsigned char rear)
{
	int is_front = a.z + b.z < 0;
	if (is_front == pass)
		line(a.x, a.y, b.x, b.y, is_front ? front : rear);
}

static void ring(int pass, double angle, double y, double r, unsigned char front, unsigned char rear)
{
	for (int i = 0; i < SEGMENTS; i++) {
		double a0 = 2 * M_PI * i / SEGMENTS, a1 = 2 * M_PI * (i + 1) / SEGMENTS;
		edge(pass, project(angle, r * cos(a0), y, r * sin(a0)), project(angle, r * cos(a1), y, r * sin(a1)), front,
		     rear);
	}
}

static Point at(double angle, double r, double y, double a, double t)
{
	return project(angle, r * cos(a) - t * sin(a), y, r * sin(a) + t * cos(a));
}

static Point quill(double angle, double t, double spread)
{
	double w = spread * sin(M_PI * t);
	return at(angle, 0.58 + 0.45 * t - 0.15 * t * t + (w != 0 ? 0.04 : 0), 0.2 + 0.95 * t - (w != 0 ? 0.08 : 0),
		  -0.45 - 0.35 * t, w);
}

static void draw_hat(double angle)
{
	for (int pass = 0; pass < 2; pass++) {
		for (int side = -1; side <= 1; side += 2) {
			Point knot = at(angle, 0.56, 0.19, 0, 0);
			Point top = at(angle, 0.6, 0.27, 0, side * 0.15), bottom = at(angle, 0.6, 0.11, 0, side * 0.15);
			edge(pass, knot, top, INK_LIGHT, INK_GRAY);
			edge(pass, top, bottom, INK_LIGHT, INK_GRAY);
			edge(pass, bottom, knot, INK_LIGHT, INK_GRAY);
		}
		ring(pass, angle, 0, 1.0, INK_HIGHLIGHT, INK_HAT);
		ring(pass, angle, 0, 0.55, INK_HIGHLIGHT, INK_HAT);
		ring(pass, angle, 0.12, 0.555, INK_LIGHT, INK_GRAY);
		ring(pass, angle, 0.26, 0.56, INK_LIGHT, INK_GRAY);
		ring(pass, angle, 0.95, 0.6, INK_HIGHLIGHT, INK_HAT);
		for (int i = 0; i < SEGMENTS; i += 2) {
			double a = 2 * M_PI * i / SEGMENTS;
			edge(pass, project(angle, 0.55 * cos(a), 0, 0.55 * sin(a)),
			     project(angle, 0.6 * cos(a), 0.95, 0.6 * sin(a)), INK_HIGHLIGHT, INK_HAT);
			edge(pass, project(angle, 0.55 * cos(a), 0, 0.55 * sin(a)), project(angle, cos(a), 0, sin(a)),
			     INK_HIGHLIGHT, INK_HAT);
		}
		for (int k = 0; k < 6; k++)
			edge(pass, quill(angle, k / 6.0, 0), quill(angle, (k + 1) / 6.0, 0), INK_CYAN, INK_TEAL);
		for (int k = 1; k < 6; k++)
			for (int side = -1; side <= 1; side += 2)
				edge(pass, quill(angle, k / 6.0, 0), quill(angle, k / 6.0, side * 0.17), INK_CYAN, INK_TEAL);
	}
}

void boot_frame(long tick)
{
	double angle = 2 * M_PI * (double)(tick % 75) / 75;
	clear(INK_BG);
	tilt = 0.36 + 0.1 * sin(2 * angle);
	size = 25;
	lift = 0;
	draw_hat(angle);
}

void shutdown_frame(long tick)
{
	double t = tick / (2 * TICKS_PER_SECOND);
	clear(INK_BG);
	if (t < 1) {
		double angle = 2 * M_PI * (t * 2.5 + t * t * 3);
		tilt = 0.36 + 0.3 * t;
		size = 25 * (1 - t) * (1 - t);
		lift = 40 * t * t;
		if (size >= 1)
			draw_hat(angle);
	}
	for (int i = 0; i < 6; i++) {
		double life = t * 1.6 - i * 0.12;
		if (life <= 0 || life >= 1)
			continue;
		int x = LOW / 2 + (int)lround(sin(i * 2.4) * 14 * life), y = LOW / 2 - (int)lround(life * 30) + i * 3;
		unsigned char ink = life < 0.6 ? INK_HIGHLIGHT : INK_HAT;
		plot(x, y, ink);
		if (life < 0.4) {
			plot(x - 1, y, ink);
			plot(x + 1, y, ink);
			plot(x, y - 1, ink);
			plot(x, y + 1, ink);
		}
	}
}
