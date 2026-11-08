PREFIX ?= /usr
BINDIR ?= $(PREFIX)/bin
LIBDIR ?= $(PREFIX)/lib/tinyhat-bootsplash
DATADIR ?= $(PREFIX)/share/tinyhat-bootsplash
APPDIR ?= $(PREFIX)/share/applications
STATEDIR ?= /var/lib/tinyhat-bootsplash
SYSTEMDDIR ?= $(PREFIX)/lib/systemd/system
INITCPIODIR ?= $(PREFIX)/lib/initcpio
CFLAGS ?= -O2 -pipe
PKG_CONFIG ?= pkg-config
PYTHON ?= python3
FONT ?= /usr/share/kbd/consolefonts/default8x16.psfu.gz
COLORS ?=
SPLASH_LDFLAGS ?=
ANIMATIONS = tinyhat orbits

all: tinyhat-splash tinyhat-boot-animations animations

build/font.h: scripts/psf2h.py
	mkdir -p build
	$(PYTHON) scripts/psf2h.py $(FONT) >$@.tmp
	mv $@.tmp $@

tinyhat-splash: splash/tinyhat-splash.c runtime/draw.c runtime/anim.h animations/tinyhat/anim.c build/font.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(COLORS) -Iruntime -Ibuild -o $@ splash/tinyhat-splash.c runtime/draw.c animations/tinyhat/anim.c $(LDFLAGS) $(SPLASH_LDFLAGS) -lm

tinyhat-boot-animations: gui/tinyhat-boot-animations.c runtime/anim.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(COLORS) -Iruntime -DDATADIR='"$(DATADIR)"' -DLIBDIR='"$(LIBDIR)"' -DSTATEDIR='"$(STATEDIR)"' $$($(PKG_CONFIG) --cflags gtk4) -o $@ gui/tinyhat-boot-animations.c $(LDFLAGS) $$($(PKG_CONFIG) --libs gtk4)

animations:
	for a in $(ANIMATIONS); do $(MAKE) -C animations/$$a RUNTIME="$(CURDIR)/runtime" CFLAGS="$(CFLAGS)" || exit 1; done

install: all
	install -Dm755 tinyhat-splash "$(DESTDIR)$(BINDIR)/tinyhat-splash"
	install -Dm755 tinyhat-boot-animations "$(DESTDIR)$(BINDIR)/tinyhat-boot-animations"
	install -Dm755 scripts/tinyhat-bootsplash-activate "$(DESTDIR)$(BINDIR)/tinyhat-bootsplash-activate"
	install -Dm644 system/tinyhat-boot-animations.desktop "$(DESTDIR)$(APPDIR)/tinyhat-boot-animations.desktop"
	install -Dm644 -t "$(DESTDIR)$(DATADIR)/runtime" runtime/anim.h runtime/draw.c runtime/main.c
	install -Dm644 -t "$(DESTDIR)$(DATADIR)/templates/basic" templates/basic/anim.c templates/basic/Makefile templates/basic/compile_flags.txt
	for a in $(ANIMATIONS); do \
		install -Dm644 -t "$(DESTDIR)$(DATADIR)/animations/$$a" animations/$$a/anim.c animations/$$a/Makefile animations/$$a/compile_flags.txt || exit 1; \
		install -Dm755 animations/$$a/anim "$(DESTDIR)$(LIBDIR)/animations/$$a" || exit 1; \
	done
	install -dm755 "$(DESTDIR)$(LIBDIR)/activate.d"
	install -Dm644 LICENSE "$(DESTDIR)$(PREFIX)/share/licenses/tinyhat-bootsplash/LICENSE"

install-arch:
	install -Dm644 arch/initcpio-install "$(DESTDIR)$(INITCPIODIR)/install/tinyhat-bootsplash"
	install -Dm644 arch/initcpio-hook "$(DESTDIR)$(INITCPIODIR)/hooks/tinyhat-bootsplash"
	install -Dm644 arch/tinyhat-bootsplash-quit.service "$(DESTDIR)$(SYSTEMDDIR)/tinyhat-bootsplash-quit.service"
	install -Dm644 arch/tinyhat-bootsplash-shutdown.service "$(DESTDIR)$(SYSTEMDDIR)/tinyhat-bootsplash-shutdown.service"
	install -Dm755 arch/50-mkinitcpio "$(DESTDIR)$(LIBDIR)/activate.d/50-mkinitcpio"

clean:
	$(RM) tinyhat-splash tinyhat-boot-animations build/font.h
	for a in $(ANIMATIONS); do $(MAKE) -C animations/$$a clean; done

.PHONY: all animations install install-arch clean
