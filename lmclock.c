#include <math.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/shape.h>

#define DEFAULT_RADIUS 70

struct stuff {
     Display *disp;
     Pixmap bound, clip;
     Pixmap hbound, hclip;
     GC gc, cgc, copygc;
     XFontStruct *font;
     int x, y, orbit, radius, width;
     Window window;
};

void vector(int xo, int yo, double angle, double radius, int *x, int *y)
{
    *x = (int) (xo + radius * cos(angle) + 0.5);
    *y = (int) (yo - radius * sin(angle) + 0.5);
}


void drawface(struct stuff *st)
{
    int h, radius, xo, yo, mg;
    char buf[5];
    XCharStruct cs;

    xo = st->x / 2;
    yo = st->y / 2;
    mg = (st->font->ascent + st->font->descent) / 2;
    h = XTextWidth(st->font, "3", 1) / 2;
    mg = h > mg ? h : mg;
    h = XTextWidth(st->font, "9", 1) / 2;
    mg = h > mg ? h : mg;
    radius = (st->x < st->y ? st->x : st->y) / 2 - mg - 1;
    st->radius = radius;

    XFillRectangle(st->disp, st->bound, st->cgc, 0, 0, st->x, st->y);
    XFillRectangle(st->disp, st->clip, st->cgc, 0, 0, st->x, st->y);
    for (h = 0; h < 12; ++h) {
	double angle;
	int x, y, i, j;

	sprintf(buf, "%d", (14 - h) % 12 + 1);
	angle = h * M_PI * 2.0 / 12.0;
	vector(xo, yo, angle, (double) radius, &x, &y);
        x -= XTextWidth(st->font, buf, strlen(buf)) / 2;
	y += (st->font->ascent - st->font->descent) / 2;

	for (i = -(st->orbit); i <= st->orbit; ++i) {
	    for (j = -(st->orbit); j <= st->orbit; ++j) {
		XDrawString(st->disp, st->bound, st->gc, x + i, y + j,
		    buf, strlen(buf));
	    }
	}
	XDrawString(st->disp, st->clip, st->gc, x, y, buf, strlen(buf));
    }
}


void drawline(struct stuff *st, int x1, int y1, int x2, int y2, int width)
{
    XSetLineAttributes(st->disp, st->gc, width + st->orbit * 2,
	LineSolid, CapRound, JoinMiter);
    XDrawLine(st->disp, st->hbound, st->gc, x1, y1, x2, y2);

    XSetLineAttributes(st->disp, st->gc, width,
	LineSolid, CapRound, JoinMiter);
    XDrawLine(st->disp, st->hclip, st->gc, x1, y1, x2, y2);
}


void drawhands(struct stuff *st)
{
    double hangle, mangle;
    time_t t;
    struct tm *lt;
    int m, dm, qdm, xo, yo, x, y;

    dm = 60 * 12;
    qdm = 60 * 3;

    /* get time, figure hand positions */
    time(&t);
    lt = localtime(&t);

    m = ((lt->tm_hour % 12) * 60 + lt->tm_min);
    m = (dm - m + qdm - 1) % dm;
    hangle = (double) m / dm * 2 * M_PI;

    m = lt->tm_min;
    m = (60 - m + 15 - 1) % 60;
    mangle = (double) m / 60 * 2 * M_PI;
    
    /* copy over face pixmaps */
    XCopyArea(st->disp, st->bound, st->hbound, st->copygc,
	0, 0, st->x, st->y, 0, 0);
    XCopyArea(st->disp, st->clip, st->hclip, st->copygc,
	0, 0, st->x, st->y, 0, 0);

    /* draw hands */
    xo = st->x / 2;
    yo = st->y / 2;
    vector(xo, yo, mangle, st->radius * .75, &x, &y);
    drawline(st, xo, yo, x, y, st->width);

    vector(xo, yo, hangle, st->radius * .5, &x, &y);
    drawline(st, xo, yo, x, y, st->width * 2);

    /* re-shape window */
    XShapeCombineMask(st->disp, st->window, ShapeBounding, 0, 0, st->hbound,
	ShapeSet);
    XShapeCombineMask(st->disp, st->window, ShapeClip, 0, 0, st->hclip,
	ShapeSet);
    XSync(st->disp, True);
}


void resizepm(struct stuff *st, Pixmap *p)
{
    Display *d = st->disp;

    if (*p != None)
	XFreePixmap(d, *p);

    *p = XCreatePixmap(d, RootWindow(d, DefaultScreen(d)), st->x, st->y, 1);
}


void resize(struct stuff *st, int x, int y)
{
    if (st->x == x && st->y == y)
	return;

    st->x = x;
    st->y = y;

    resizepm(st, &(st->bound));
    resizepm(st, &(st->clip));
    resizepm(st, &(st->hbound));
    resizepm(st, &(st->hclip));

    drawface(st);
    drawhands(st);
}

static char *basename(char *s)
{
    char *t;

    if (t = strrchr(s, '/'))
	return ++t;

    return s;
}

main(int argc, char **argv)
{
    Display *disp;
    XFontStruct *font;
    int screen, orbit;
    Window rw, w;
    Pixmap sp;
    GC gc, cgc, copygc;
    struct stuff st;
    XClassHint xch = { "lmclock", "Lmclock" };
    int i, x, y, err;
    int radius;
    char *geom, *dstr;
    XSizeHints xsh = {
	PMinSize | PMaxSize | PResizeInc | PAspect | PBaseSize | PWinGravity,
	0, 0, 0, 0,
	0, 0, 0, 0, 0, 0,
	{1, 1}, {1, 1},
	0, 0,
	NorthWestGravity
    };

    radius = DEFAULT_RADIUS;
    xsh.min_width = xsh.min_height = xsh.max_width = xsh.max_height =
	xsh.base_width = xsh.base_height = 2 * radius;
    geom = 0;
    dstr = 0;
    orbit = 2;
    err = 0;
    for (i = 1; i < argc && '-' == argv[i][0]; ++i) {
	if ('-' != argv[i][0])
	    break;

	if (!strcmp(argv[i], "-") || !strcmp(argv[i], "--")) {
	    ++i;
	    break;
	}

	if (!strncmp(argv[i], "-display", 2)) {
	    ++i;
	    if (i >= argc) {
		++err;
		break;
	    }

	    dstr = argv[i];
	} else if (!strncmp(argv[i], "-geometry", 2)) {
	    ++i;
	    if (i >= argc) {
		++err;
		break;
	    }

	    geom = argv[i];
	}
    }

    if (err || i < argc) {
	fprintf(stderr, "usage: %s [-display <display>] [-geometry <geometry>]\n",
	    argv[0] ? basename(argv[0]) : "lmclock");
	exit(2);
    }

    disp = XOpenDisplay(dstr);
    if (!disp) {
	fprintf(stderr, "Cannot open display: %s\n", dstr ? dstr : "");
	exit(1);
    }
    screen = DefaultScreen(disp);
    rw = RootWindow(disp, screen);

    x = y = 0;
    if (geom) {
	char pgeom[256];
        int w, h, g;

	sprintf(pgeom, "%dx%d", 2 * radius, 2 * radius);
	XWMGeometry(disp, screen, geom, pgeom, 0, &xsh, &x, &y,
	    &w, &h, &g);
	xsh.flags = USPosition;
    }

    /* open font */
    font = XLoadQueryFont(disp, "-adobe-courier-*-r-*-*-18-*");

    w =  XCreateWindow(disp, rw, x, y, radius * 2, radius * 2, 0,
	CopyFromParent, InputOutput, CopyFromParent,
	0L, (XSetWindowAttributes *) 0);
    XSetWindowBackground(disp, w, BlackPixel(disp, screen));
    XSetWindowBorder(disp, w, WhitePixel(disp, screen));
    XmbSetWMProperties(disp, w, "lmclock",
       "lmclock", argv, argc, &xsh, NULL, &xch);
    XMapWindow(disp, w);

    /* create GCs */
    sp = XCreatePixmap(disp, w, 1, 1, 1);

    gc = XCreateGC(disp, sp, 0L, (XGCValues *) 0);
    XSetFont(disp, gc, font->fid);
    XSetLineAttributes(disp, gc, 5, LineSolid, CapRound, JoinMiter);
    XSetFunction(disp, gc, GXset);

    cgc = XCreateGC(disp, sp, 0L, (XGCValues *) 0);
    XSetFunction(disp, cgc, GXclear);

    copygc = XCreateGC(disp, sp, 0L, (XGCValues *) 0);
    XSetFunction(disp, copygc, GXcopy);

    XFreePixmap(disp, sp);

    st.disp = disp;
    st.bound = st.hbound = st.clip = st.hclip = None;
    st.gc = gc;
    st.cgc = cgc;
    st.copygc = copygc;
    st.font = font;
    st.x = st.y = 0;
    st.orbit = st.width = orbit;
    st.window = w;

    resize(&st, radius * 2, radius * 2);
    for (;;) {
	sleep(30);
	drawhands(&st);
    }
}
