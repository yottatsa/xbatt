#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef DEBUG
#define PROCAPM "procapm"
#else
#define PROCAPM "/proc/apm"
#endif
#define FMTCHG "%i%%+"
#define FMTDIS "%i%%"

void die(char *s) {
  fprintf(stderr, "%s\n", s);
  exit(1);
}

char msg[] = "----";
FILE *procapm_fd;
Display *d;
int s;
Window w;
GC gc;

void draw() {
  // "1.16ac 1. 2 0x03 0x01 0x03 0x9 79% -1 ?"
  int chg, rem;
  if (procapm_fd == NULL)
    goto redraw;

  procapm_fd = freopen(NULL, "r", procapm_fd);
  if (procapm_fd == NULL)
    goto redraw;

  if (fscanf(procapm_fd, "%*s %*d. %*i %*i %*i %*i %*i %i%% %i %*s", &chg,
             &rem) != 2)
    die("Failed on parsing " PROCAPM ".");
  if (rem == -1)
    snprintf(msg, sizeof(msg), FMTCHG, chg);
  else
    snprintf(msg, sizeof(msg), FMTDIS, chg);

redraw:
  XClearWindow(d, w);
  XDrawString(d, w, gc, 1, 15, msg, strlen(msg));
  XFlush(d);
}

int main(void) {
  char *env;
  XEvent e;
  Colormap cmap;
  XColor fgcolor, bgcolor;

  int x11_fd;
  fd_set in_fds;
  struct timeval tv;

  procapm_fd = fopen(PROCAPM, "r");

  d = XOpenDisplay(NULL);
  if (d == NULL)
    die("Cannot open display.");

  s = DefaultScreen(d);
  cmap = XDefaultColormap(d, s);

  fgcolor.pixel = BlackPixel(d, s);
  env = getenv("FGCOLOR");
  if (env != NULL) {
    XParseColor(d, cmap, env, &fgcolor);
    XAllocColor(d, cmap, &fgcolor);
  }

  bgcolor.pixel = WhitePixel(d, s);
  env = getenv("BGCOLOR");
  if (env != NULL) {
    XParseColor(d, cmap, env, &bgcolor);
    XAllocColor(d, cmap, &bgcolor);
  }

#ifdef DEBUG
  printf("colors: fg=%li bg=%li\n", fgcolor.pixel, bgcolor.pixel);
#endif

  w = XCreateSimpleWindow(d, RootWindow(d, s), 0, 0, 100, 20, 0, fgcolor.pixel,
                          bgcolor.pixel);

  XSelectInput(d, w, ExposureMask | KeyPressMask);
  XMapWindow(d, w);

  gc = DefaultGC(d, s);
  XSetForeground(d, gc, fgcolor.pixel);

  XFlush(d);
  x11_fd = ConnectionNumber(d);

  while (1) {
    FD_ZERO(&in_fds);
    FD_SET(x11_fd, &in_fds);
    tv.tv_usec = 0;
    tv.tv_sec = 1;
    int num_ready_fds = select(x11_fd + 1, &in_fds, NULL, NULL, &tv);
    if (num_ready_fds > 0)
      while (XPending(d)) {
        XNextEvent(d, &e);
        if (e.type == Expose)
          draw();
#ifdef DEBUG
        if (e.type == KeyPress)
          goto close;
#endif
      }
    else if (num_ready_fds == 0)
      draw();
  }

#ifdef DEBUG
close:
#endif

  XFreeColormap(d, cmap);
  XCloseDisplay(d);
  XDestroyWindow(d, w);
  if (procapm_fd != NULL)
    fclose(procapm_fd);
  return 0;
}
