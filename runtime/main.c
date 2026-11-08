#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include "anim.h"

int main(int argc, char **argv)
{
	int shutdown_mode = argc > 1 && !strcmp(argv[1], "shutdown");
	int out = dup(1);
	int32_t tick;
	if (out < 0 || dup2(2, 1) < 0)
		return 1;
	for (;;) {
		size_t n = 0;
		while (n < sizeof(tick)) {
			ssize_t r = read(0, (char *)&tick + n, sizeof(tick) - n);
			if (r <= 0)
				return 0;
			n += (size_t)r;
		}
		if (shutdown_mode)
			shutdown_frame(tick);
		else
			boot_frame(tick);
		for (n = 0; n < sizeof(low);) {
			ssize_t w = write(out, low + n, sizeof(low) - n);
			if (w <= 0)
				return 1;
			n += (size_t)w;
		}
	}
}
