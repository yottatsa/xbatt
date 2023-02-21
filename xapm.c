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

char msg[] = "----";
FILE *procapm_fd;

void draw(Display *d, int s, Window *w) {
  // "1.16ac 1. 2 0x03 0x01 0x03 0x9 79% -1 ?"
  int chg, rem;
  if (procapm_fd != NULL) {
    fseek(procapm_fd, 0L, SEEK_SET);
    fscanf(procapm_fd, "%*s %*d. %*i %*i %*i %*i %*i %i%% %i %*s", &chg, &rem);
    if (rem == -1)
      snprintf(msg, sizeof(msg), FMTCHG, chg);
    else
      snprintf(msg, sizeof(msg), FMTDIS, chg);
  }
  XClearWindow(d, *w);
  XDrawString(d, *w, DefaultGC(d, s), 1, 15, msg, strlen(msg));
  XFlush(d);
}

void die(char *s) {
  fprintf(stderr, "%s\n", s);
  exit(1);
}

int main(void) {
  Display *d;
  Window w;
  XEvent e;
  int s;

  int x11_fd;
  fd_set in_fds;
  struct timeval tv;

  procapm_fd = fopen(PROCAPM, "r");

  d = XOpenDisplay(NULL);
  if (d == NULL)
    die("Cannot open display.");

  s = DefaultScreen(d);
  w = XCreateSimpleWindow(d, RootWindow(d, s), 0, 0, 100, 20, 0,
                          BlackPixel(d, s), WhitePixel(d, s));
  XSelectInput(d, w, ExposureMask | KeyPressMask);
  XMapWindow(d, w);
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
          draw(d, s, &w);
#ifdef DEBUG
        if (e.type == KeyPress)
          goto close;
#endif
      }
    else if (num_ready_fds == 0)
      draw(d, s, &w);
  }

close:
  XCloseDisplay(d);
  if (procapm_fd != NULL)
    fclose(procapm_fd);
  return 0;
}
