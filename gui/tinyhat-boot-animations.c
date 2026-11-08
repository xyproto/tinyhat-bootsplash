#include <errno.h>
#include <gtk/gtk.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "anim.h"

#ifndef DATADIR
#define DATADIR "/usr/share/tinyhat-bootsplash"
#endif
#ifndef LIBDIR
#define LIBDIR "/usr/lib/tinyhat-bootsplash"
#endif
#ifndef STATEDIR
#define STATEDIR "/var/lib/tinyhat-bootsplash"
#endif

#define TILE_W 300
#define TILE_H 200
#define FRAME_MS 80
#define BOOT_TICKS 50
#define SHUTDOWN_TICKS 25
#define TIMEOUT_MS 300
#define DEFAULT_ANIMATION "tinyhat"

typedef struct {
	GPid pid;
	int in, out;
} Runner;

typedef struct {
	char *name, *dir, *exe;
	gboolean system, is_default;
	Runner boot, shutdown;
	unsigned char frame[LOW * LOW];
	long t;
	gboolean ok;
	GtkWidget *area, *title;
} Anim;

static const uint32_t palette[INKS] = INK_COLORS;
static GPtrArray *anims;
static GtkWidget *flow, *statusbar, *window;
static char *active_name;
static const char *preview_dir;

static const char *data_dir(void)
{
	static char *dir;
	const char *d = g_getenv("TINYHAT_BOOTSPLASH_DATA");
	if (d && *d)
		return d;
	if (!dir) {
		char *exe = g_file_read_link("/proc/self/exe", NULL), *here = exe ? g_path_get_dirname(exe) : NULL;
		char *anims = here ? g_build_filename(here, "animations", NULL) : NULL,
		     *runtime = here ? g_build_filename(here, "runtime", "anim.h", NULL) : NULL;
		dir = anims && g_file_test(anims, G_FILE_TEST_IS_DIR) && g_file_test(runtime, G_FILE_TEST_EXISTS) ? g_strdup(here)
														: g_strdup(DATADIR);
		g_free(exe);
		g_free(here);
		g_free(anims);
		g_free(runtime);
	}
	return dir;
}

static const char *state_dir(void)
{
	const char *d = g_getenv("TINYHAT_BOOTSPLASH_STATE");
	return d && *d ? d : STATEDIR;
}

static char *user_dir(void)
{
	return g_build_filename(g_get_user_data_dir(), "tinyhat-bootsplash", "animations", NULL);
}

static void status(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	char *s = g_strdup_vprintf(fmt, ap);
	va_end(ap);
	gtk_label_set_text(GTK_LABEL(statusbar), s);
	g_free(s);
}

static void runner_stop(Runner *r)
{
	if (r->pid > 0) {
		kill(r->pid, SIGKILL);
		waitpid(r->pid, NULL, 0);
		g_spawn_close_pid(r->pid);
	}
	if (r->in >= 0)
		close(r->in);
	if (r->out >= 0)
		close(r->out);
	r->pid = 0;
	r->in = r->out = -1;
}

static gboolean runner_start(Runner *r, const char *exe, const char *mode)
{
	char *argv[] = { (char *)exe, (char *)mode, NULL };
	r->pid = 0;
	r->in = r->out = -1;
	if (!g_file_test(exe, G_FILE_TEST_IS_EXECUTABLE))
		return FALSE;
	if (!g_spawn_async_with_pipes(NULL, argv, NULL, G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_STDERR_TO_DEV_NULL, NULL, NULL,
				      &r->pid, &r->in, &r->out, NULL, NULL)) {
		r->pid = 0;
		return FALSE;
	}
	return TRUE;
}

static gboolean runner_frame(Runner *r, int32_t tick, unsigned char *frame)
{
	size_t got = 0;
	if (r->pid <= 0 || write(r->in, &tick, sizeof(tick)) != sizeof(tick))
		return FALSE;
	while (got < LOW * LOW) {
		struct pollfd p = { r->out, POLLIN, 0 };
		if (poll(&p, 1, TIMEOUT_MS) <= 0)
			return FALSE;
		ssize_t n = read(r->out, frame + got, LOW * LOW - got);
		if (n <= 0)
			return FALSE;
		got += (size_t)n;
	}
	return TRUE;
}

static void anim_stop(Anim *a)
{
	runner_stop(&a->boot);
	runner_stop(&a->shutdown);
}

static void anim_start(Anim *a)
{
	anim_stop(a);
	a->t = 0;
	a->ok = runner_start(&a->boot, a->exe, "boot") && runner_start(&a->shutdown, a->exe, "shutdown");
	if (!a->ok)
		anim_stop(a);
}

static void anim_free(gpointer p)
{
	Anim *a = p;
	anim_stop(a);
	g_free(a->name);
	g_free(a->dir);
	g_free(a->exe);
	g_free(a);
}

static void draw(GtkDrawingArea *area, cairo_t *cr, int w, int h, gpointer data)
{
	Anim *a = data;
	(void)area;
	uint32_t bg = palette[INK_BG];
	cairo_set_source_rgb(cr, ((bg >> 16) & 255) / 255.0, ((bg >> 8) & 255) / 255.0, (bg & 255) / 255.0);
	cairo_paint(cr);
	if (!a->ok) {
		cairo_set_source_rgb(cr, 0.75, 0.79, 0.96);
		cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
		cairo_set_font_size(cr, 14);
		cairo_move_to(cr, 16, h / 2.0);
		cairo_show_text(cr, a->system ? "Not built" : "Not built yet, press Build");
		return;
	}
	cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_RGB24, LOW, LOW);
	unsigned char *px = cairo_image_surface_get_data(s);
	int stride = cairo_image_surface_get_stride(s);
	cairo_surface_flush(s);
	for (int y = 0; y < LOW; y++)
		for (int x = 0; x < LOW; x++) {
			unsigned char ink = a->frame[y * LOW + x];
			((uint32_t *)(px + y * stride))[x] = palette[ink < INKS ? ink : INK_BG];
		}
	cairo_surface_mark_dirty(s);
	int scale = MIN(w, h) / LOW;
	if (scale < 1)
		scale = 1;
	cairo_save(cr);
	cairo_translate(cr, (w - LOW * scale) / 2.0, (h - LOW * scale) / 2.0);
	cairo_scale(cr, scale, scale);
	cairo_set_source_surface(cr, s, 0, 0);
	cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_NEAREST);
	cairo_paint(cr);
	cairo_restore(cr);
	cairo_surface_destroy(s);
	cairo_set_source_rgba(cr, 0.75, 0.79, 0.96, 0.6);
	cairo_select_font_face(cr, "monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cr, 11);
	cairo_move_to(cr, 8, h - 8);
	cairo_show_text(cr, (a->t % (BOOT_TICKS + SHUTDOWN_TICKS)) < BOOT_TICKS ? "boot" : "shutdown");
}

static gboolean tick_all(gpointer data)
{
	(void)data;
	for (guint i = 0; i < anims->len; i++) {
		Anim *a = anims->pdata[i];
		if (!a->ok)
			continue;
		long t = a->t++ % (BOOT_TICKS + SHUTDOWN_TICKS);
		gboolean fine = t < BOOT_TICKS ? runner_frame(&a->boot, (int32_t)t, a->frame)
					       : runner_frame(&a->shutdown, (int32_t)(t - BOOT_TICKS), a->frame);
		if (!fine) {
			a->ok = FALSE;
			anim_stop(a);
			status("%s stopped drawing frames", a->name);
		}
		gtk_widget_queue_draw(a->area);
	}
	return G_SOURCE_CONTINUE;
}

static char *unique_dir(const char *base)
{
	char *parent = user_dir(), *dir = g_build_filename(parent, base, NULL);
	for (int i = 2; g_file_test(dir, G_FILE_TEST_EXISTS); i++) {
		char *name = g_strdup_printf("%s-%d", base, i);
		g_free(dir);
		dir = g_build_filename(parent, name, NULL);
		g_free(name);
	}
	g_free(parent);
	return dir;
}

static gboolean copy_sources(const char *from, const char *to, GError **err)
{
	GDir *d = g_dir_open(from, 0, err);
	const char *f;
	if (!d || g_mkdir_with_parents(to, 0755))
		return FALSE;
	while ((f = g_dir_read_name(d))) {
		if (strcmp(f, "Makefile") && strcmp(f, "compile_flags.txt") && !g_str_has_suffix(f, ".c") &&
		    !g_str_has_suffix(f, ".h"))
			continue;
		char *src = g_build_filename(from, f, NULL), *dst = g_build_filename(to, f, NULL), *text = NULL;
		gsize len = 0;
		gboolean ok = g_file_get_contents(src, &text, &len, err) && g_file_set_contents(dst, text, (gssize)len, err);
		g_free(src);
		g_free(dst);
		g_free(text);
		if (!ok) {
			g_dir_close(d);
			return FALSE;
		}
	}
	g_dir_close(d);
	return TRUE;
}

static const char *terminals[][4] = {
	{ "foot", "", NULL, NULL },	       { "xdg-terminal-exec", "", NULL, NULL }, { "kitty", "", NULL, NULL },
	{ "alacritty", "", "-e", NULL },   { "wezterm", "", "start", "--" },	      { "konsole", "", "-e", NULL },
	{ "gnome-terminal", "", "--", NULL }, { "xfce4-terminal", "", "-x", NULL }, { "xterm", "", "-e", NULL },
};

static const char *desktop_terminal(void)
{
	const char *desktop = g_getenv("XDG_CURRENT_DESKTOP");
	if (!desktop)
		return NULL;
	if (strstr(desktop, "XFCE"))
		return "xfce4-terminal";
	if (strstr(desktop, "KDE"))
		return "konsole";
	if (strstr(desktop, "GNOME"))
		return "gnome-terminal";
	if (strstr(desktop, "dwl") || strstr(desktop, "sway"))
		return "foot";
	return NULL;
}

static gboolean terminal_usable(const char *name)
{
	return strcmp(name, "foot") || g_getenv("WAYLAND_DISPLAY");
}

static void open_editor(const char *dir)
{
	const char *term = g_getenv("TERMINAL"), *preferred = desktop_terminal();
	char *found = NULL;
	GPtrArray *argv = g_ptr_array_new();
	GError *err = NULL;
	if (term && *term && (found = g_find_program_in_path(term))) {
		g_ptr_array_add(argv, found);
		g_ptr_array_add(argv, "-e");
	}
	for (int pass = 0; pass < 2 && !found; pass++)
		for (size_t i = 0; i < G_N_ELEMENTS(terminals) && !found; i++) {
			if ((pass == 0 && (!preferred || strcmp(preferred, terminals[i][0]))) || !terminal_usable(terminals[i][0]))
				continue;
			if ((found = g_find_program_in_path(terminals[i][0]))) {
				g_ptr_array_add(argv, found);
				for (int j = 2; j < 4 && terminals[i][j]; j++)
					g_ptr_array_add(argv, (char *)terminals[i][j]);
			}
		}
	if (!found) {
		status("No terminal program was found for opening Orbiton");
		g_ptr_array_free(argv, TRUE);
		return;
	}
	g_ptr_array_add(argv, "o");
	g_ptr_array_add(argv, "anim.c");
	g_ptr_array_add(argv, NULL);
	if (!g_spawn_async(dir, (char **)argv->pdata, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &err)) {
		status("Could not open Orbiton: %s", err->message);
		g_error_free(err);
	} else {
		char *name = g_path_get_basename(dir);
		status("Opened %s in Orbiton. Press Build here when it is saved", name);
		g_free(name);
	}
	g_free(found);
	g_ptr_array_free(argv, TRUE);
}

static void reload(void);

static char *duplicate(Anim *a)
{
	GError *err = NULL;
	char *base = g_strdup_printf("%s-copy", a->name), *dir = unique_dir(base);
	g_free(base);
	if (!copy_sources(a->dir, dir, &err)) {
		status("Could not copy %s: %s", a->name, err ? err->message : "unknown error");
		g_clear_error(&err);
		g_free(dir);
		return NULL;
	}
	return dir;
}

typedef struct {
	char *name, *dir;
	gboolean then_reload;
} Job;

static void job_free(Job *j)
{
	g_free(j->name);
	g_free(j->dir);
	g_free(j);
}

static void build_done(GObject *src, GAsyncResult *res, gpointer data)
{
	Job *j = data;
	char *out = NULL;
	GError *err = NULL;
	GSubprocess *p = G_SUBPROCESS(src);
	if (!g_subprocess_communicate_utf8_finish(p, res, &out, NULL, &err)) {
		status("Building %s failed: %s", j->name, err->message);
		g_error_free(err);
	} else if (!g_subprocess_get_successful(p)) {
		char **lines = g_strsplit(out ? out : "", "\n", -1);
		const char *msg = "";
		for (int i = 0; lines[i]; i++)
			if (strstr(lines[i], "error"))
				msg = lines[i];
		status("Building %s failed: %s", j->name, msg);
		g_strfreev(lines);
	} else {
		status("Built %s", j->name);
	}
	g_free(out);
	reload();
	job_free(j);
}

static void build(const char *name, const char *dir)
{
	GError *err = NULL;
	GSubprocess *p = g_subprocess_new(G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_MERGE, &err, "make",
					  "-C", dir, NULL);
	if (!p) {
		status("Could not run make: %s", err->message);
		g_error_free(err);
		return;
	}
	Job *j = g_new0(Job, 1);
	j->name = g_strdup(name);
	j->dir = g_strdup(dir);
	status("Building %s...", name);
	g_subprocess_communicate_utf8_async(p, NULL, NULL, build_done, j);
	g_object_unref(p);
}

static void activate_done(GObject *src, GAsyncResult *res, gpointer data)
{
	Job *j = data;
	char *out = NULL;
	GError *err = NULL;
	GSubprocess *p = G_SUBPROCESS(src);
	if (!g_subprocess_communicate_utf8_finish(p, res, &out, NULL, &err)) {
		status("%s", err->message);
		g_error_free(err);
	} else if (!g_subprocess_get_successful(p)) {
		status("%s", out && *out ? g_strstrip(out) : "Changing the boot animation failed");
	} else if (j->name) {
		status("%s is now used for booting and shutting down", j->name);
	} else {
		status("The default boot animation is used again");
	}
	g_free(out);
	reload();
	job_free(j);
}

static void set_active(Anim *a)
{
	GError *err = NULL;
	char *helper = g_build_filename(data_dir(), "scripts", "tinyhat-bootsplash-activate", NULL);
	GSubprocess *p;
	if (!g_file_test(helper, G_FILE_TEST_IS_EXECUTABLE)) {
		g_free(helper);
		helper = g_find_program_in_path("tinyhat-bootsplash-activate");
	}
	if (!helper) {
		status("tinyhat-bootsplash-activate was not found");
		return;
	}
	if (a)
		p = g_subprocess_new(G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_MERGE, &err, helper, a->exe,
				     a->name, NULL);
	else
		p = g_subprocess_new(G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_MERGE, &err, helper, "--off",
				     NULL);
	g_free(helper);
	if (!p) {
		status("%s", err->message);
		g_error_free(err);
		return;
	}
	Job *j = g_new0(Job, 1);
	j->name = a ? g_strdup(a->name) : NULL;
	g_subprocess_communicate_utf8_async(p, NULL, NULL, activate_done, j);
	g_object_unref(p);
}

static gboolean in_use(Anim *a)
{
	if (a->is_default)
		return !active_name || !strcmp(active_name, a->name);
	return active_name && !strcmp(active_name, a->name);
}

static Anim *button_anim(GtkWidget *b)
{
	return g_object_get_data(G_OBJECT(b), "anim");
}

static void on_edit(GtkButton *b, gpointer d)
{
	(void)d;
	Anim *a = button_anim(GTK_WIDGET(b));
	if (a->system) {
		char *dir = duplicate(a);
		if (!dir)
			return;
		char *copy = g_path_get_basename(dir);
		status("Built-in animations can not be changed, so the copy %s was opened instead", copy);
		g_free(copy);
		open_editor(dir);
		char *name = g_path_get_basename(dir);
		build(name, dir);
		g_free(name);
		g_free(dir);
	} else {
		open_editor(a->dir);
	}
}

static void on_build(GtkButton *b, gpointer d)
{
	(void)d;
	Anim *a = button_anim(GTK_WIDGET(b));
	build(a->name, a->dir);
}

static void on_duplicate(GtkButton *b, gpointer d)
{
	(void)d;
	char *dir = duplicate(button_anim(GTK_WIDGET(b)));
	if (dir) {
		char *name = g_path_get_basename(dir);
		build(name, dir);
		g_free(name);
		g_free(dir);
	}
}

static void on_activate(GtkButton *b, gpointer d)
{
	(void)d;
	Anim *a = button_anim(GTK_WIDGET(b));
	if (a->is_default || in_use(a))
		set_active(NULL);
	else if (!a->ok)
		status("Build %s before using it", a->name);
	else
		set_active(a);
}

static void on_new(GtkButton *b, gpointer d)
{
	(void)b;
	(void)d;
	GError *err = NULL;
	char *tpl = g_build_filename(data_dir(), "templates", "basic", NULL), *dir = unique_dir("my-animation");
	if (!copy_sources(tpl, dir, &err)) {
		status("Could not create a new animation: %s", err ? err->message : "unknown error");
		g_clear_error(&err);
	} else {
		char *name = g_path_get_basename(dir);
		open_editor(dir);
		build(name, dir);
		g_free(name);
	}
	g_free(tpl);
	g_free(dir);
}

static GtkWidget *tile_button(const char *label, Anim *a, GCallback cb)
{
	GtkWidget *b = gtk_button_new_with_label(label);
	g_object_set_data(G_OBJECT(b), "anim", a);
	g_signal_connect(b, "clicked", cb, NULL);
	return b;
}

static void add_tile(Anim *a)
{
	gboolean active = in_use(a);
	GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4), *row;
	char *name = g_markup_escape_text(a->name, -1);
	char *markup = g_strdup_printf("<b>%s</b>%s%s", name, a->is_default ? "  (default)" : a->system ? "  (built-in)" : "",
				       active ? "  <span foreground='#f7768e'>● in use</span>" : "");
	g_free(name);
	a->area = gtk_drawing_area_new();
	gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(a->area), TILE_W);
	gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(a->area), TILE_H);
	gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(a->area), draw, a, NULL);
	gtk_widget_set_size_request(a->area, TILE_W, TILE_H);
	gtk_widget_set_hexpand(a->area, FALSE);
	gtk_widget_set_halign(a->area, GTK_ALIGN_START);
	gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
	gtk_widget_set_valign(box, GTK_ALIGN_START);
	gtk_box_append(GTK_BOX(box), a->area);
	a->title = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(a->title), markup);
	gtk_label_set_xalign(GTK_LABEL(a->title), 0);
	gtk_box_append(GTK_BOX(box), a->title);
	g_free(markup);
	if (!preview_dir) {
		row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
		gtk_box_append(GTK_BOX(row), tile_button("Edit", a, G_CALLBACK(on_edit)));
		if (!a->system)
			gtk_box_append(GTK_BOX(row), tile_button("Build", a, G_CALLBACK(on_build)));
		gtk_box_append(GTK_BOX(row), tile_button("Duplicate", a, G_CALLBACK(on_duplicate)));
		GtkWidget *use = tile_button(active ? (a->is_default ? "In use" : "Stop using") : "Use", a, G_CALLBACK(on_activate));
		gtk_widget_set_sensitive(use, !(active && a->is_default));
		gtk_box_append(GTK_BOX(row), use);
		gtk_box_append(GTK_BOX(box), row);
	}
	gtk_flow_box_append(GTK_FLOW_BOX(flow), box);
	anim_start(a);
}

static void add_anim(const char *name, const char *dir, const char *exe, gboolean system, gboolean is_default)
{
	Anim *a = g_new0(Anim, 1);
	a->is_default = is_default;
	a->name = g_strdup(name);
	a->dir = g_strdup(dir);
	a->exe = g_strdup(exe);
	a->system = system;
	a->boot.in = a->boot.out = a->shutdown.in = a->shutdown.out = -1;
	g_ptr_array_add(anims, a);
	add_tile(a);
}

static void scan(const char *parent, gboolean system)
{
	GDir *d = g_dir_open(parent, 0, NULL);
	const char *name;
	GPtrArray *names = g_ptr_array_new_with_free_func(g_free);
	if (!d)
		return;
	while ((name = g_dir_read_name(d)))
		g_ptr_array_add(names, g_strdup(name));
	g_dir_close(d);
	g_ptr_array_sort(names, (GCompareFunc)g_strcmp0);
	for (guint i = 1; system && i < names->len; i++)
		if (!strcmp(g_ptr_array_index(names, i), DEFAULT_ANIMATION)) {
			gpointer first = g_ptr_array_steal_index(names, i);
			g_ptr_array_insert(names, 0, first);
		}
	for (guint i = 0; i < names->len; i++) {
		name = g_ptr_array_index(names, i);
		char *dir = g_build_filename(parent, name, NULL), *src = g_build_filename(dir, "anim.c", NULL);
		if (g_file_test(src, G_FILE_TEST_EXISTS)) {
			char *exe = g_build_filename(dir, "anim", NULL);
			if (system && !g_file_test(exe, G_FILE_TEST_IS_EXECUTABLE)) {
				g_free(exe);
				exe = g_build_filename(LIBDIR, "animations", name, NULL);
			}
			add_anim(name, dir, exe, system && access(dir, W_OK) != 0, system && !strcmp(name, DEFAULT_ANIMATION));
			g_free(exe);
		}
		g_free(dir);
		g_free(src);
	}
	g_ptr_array_free(names, TRUE);
}

static void reload(void)
{
	char *path = g_build_filename(state_dir(), "active-name", NULL), *text = NULL;
	g_clear_pointer(&active_name, g_free);
	if (g_file_get_contents(path, &text, NULL, NULL))
		active_name = g_strdup(g_strstrip(text));
	g_free(text);
	g_free(path);
	g_ptr_array_set_size(anims, 0);
	GtkWidget *child;
	while ((child = gtk_widget_get_first_child(flow)))
		gtk_flow_box_remove(GTK_FLOW_BOX(flow), child);
	if (preview_dir) {
		char *dir = g_canonicalize_filename(preview_dir, NULL), *name = g_path_get_basename(dir),
		     *exe = g_build_filename(dir, "anim", NULL);
		add_anim(name, dir, exe, FALSE, FALSE);
		g_free(dir);
		g_free(name);
		g_free(exe);
		return;
	}
	char *sys = g_build_filename(data_dir(), "animations", NULL), *usr = user_dir();
	scan(sys, TRUE);
	scan(usr, FALSE);
	g_free(sys);
	g_free(usr);
}

static gboolean on_key(GtkEventControllerKey *c, guint key, guint code, GdkModifierType mods, gpointer d)
{
	(void)c;
	(void)code;
	(void)mods;
	(void)d;
	if (key != GDK_KEY_Escape || !preview_dir)
		return FALSE;
	gtk_window_destroy(GTK_WINDOW(window));
	return TRUE;
}

static void on_reload(GtkButton *b, gpointer d)
{
	(void)b;
	(void)d;
	reload();
	status("Reloaded");
}

static void activate(GtkApplication *app, gpointer d)
{
	(void)d;
	GtkWidget *box, *scroll, *header, *b;
	GtkEventController *keys;
	window = gtk_application_window_new(app);
	gtk_window_set_title(GTK_WINDOW(window), preview_dir ? "Boot animation preview" : "Boot Animations");
	gtk_window_set_default_size(GTK_WINDOW(window), preview_dir ? 340 : 1000, preview_dir ? 280 : 720);
	keys = gtk_event_controller_key_new();
	g_signal_connect(keys, "key-pressed", G_CALLBACK(on_key), NULL);
	gtk_widget_add_controller(window, keys);
	if (!preview_dir) {
		header = gtk_header_bar_new();
		b = gtk_button_new_with_label("New");
		gtk_widget_set_tooltip_text(b, "Create a new boot and shutdown animation from an example, and open it in Orbiton");
		g_signal_connect(b, "clicked", G_CALLBACK(on_new), NULL);
		gtk_header_bar_pack_start(GTK_HEADER_BAR(header), b);
		b = gtk_button_new_with_label("Reload");
		g_signal_connect(b, "clicked", G_CALLBACK(on_reload), NULL);
		gtk_header_bar_pack_end(GTK_HEADER_BAR(header), b);
		gtk_window_set_titlebar(GTK_WINDOW(window), header);
	}
	box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	scroll = gtk_scrolled_window_new();
	gtk_widget_set_vexpand(scroll, TRUE);
	flow = gtk_flow_box_new();
	gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(flow), GTK_SELECTION_NONE);
	gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(flow), TRUE);
	gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(flow), 8);
	gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(flow), 12);
	gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(flow), 12);
	gtk_widget_set_margin_start(flow, 12);
	gtk_widget_set_margin_end(flow, 12);
	gtk_widget_set_margin_top(flow, 12);
	gtk_widget_set_margin_bottom(flow, 12);
	gtk_widget_set_valign(flow, GTK_ALIGN_START);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), flow);
	gtk_box_append(GTK_BOX(box), scroll);
	statusbar = gtk_label_new("");
	gtk_label_set_xalign(GTK_LABEL(statusbar), 0);
	gtk_label_set_ellipsize(GTK_LABEL(statusbar), PANGO_ELLIPSIZE_END);
	gtk_widget_set_margin_start(statusbar, 12);
	gtk_widget_set_margin_end(statusbar, 12);
	gtk_widget_set_margin_bottom(statusbar, 6);
	gtk_box_append(GTK_BOX(box), statusbar);
	gtk_window_set_child(GTK_WINDOW(window), box);
	reload();
	if (!preview_dir)
		status("Each preview shows 4 seconds of booting, then 2 seconds of shutting down");
	g_timeout_add(FRAME_MS, tick_all, NULL);
	gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char **argv)
{
	signal(SIGPIPE, SIG_IGN);
	if (argc > 2 && !strcmp(argv[1], "--preview")) {
		preview_dir = argv[2];
		argc = 1;
	} else if (argc > 1) {
		fprintf(stderr, "usage: tinyhat-boot-animations [--preview ANIMATION_DIRECTORY]\n");
		return 2;
	}
	anims = g_ptr_array_new_with_free_func(anim_free);
	GtkApplication *app = gtk_application_new(preview_dir ? NULL : "org.tinyhat.BootAnimations",
						  preview_dir ? G_APPLICATION_NON_UNIQUE : G_APPLICATION_DEFAULT_FLAGS);
	g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
	int r = g_application_run(G_APPLICATION(app), argc, argv);
	g_ptr_array_free(anims, TRUE);
	g_object_unref(app);
	return r;
}
