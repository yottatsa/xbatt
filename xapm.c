#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#ifdef DEBUG
#define PROCAPM "procapm"
#else
#define PROCAPM "/proc/apm"
#endif
#define FMTCHG "%i%%+"
#define FMTDIS "%i%%"
char msg[] = "-----";

void die(char *s) {
  fprintf(stderr, "%s\n", s);
  syslog(LOG_ERR, "%s", s);
  exit(1);
}

FILE *procapm_fd;
Display *d;
int s;
Window w;
GC gc;
char visible = 1;

void draw() {
  if (visible == 0)
    return;
  // "1.16ac 1. 2 0x03 0x01 0x03 0x9 79% -1 ?"
  int chg, rem;

  procapm_fd = fopen(PROCAPM, "r");
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

  if (procapm_fd != NULL)
    fclose(procapm_fd);
}

int main(int argc, char *argv[]) {
  openlog("xapm", LOG_PID, LOG_USER);
  char *env;
  XEvent e;
  Colormap cmap;
  XColor fgcolor, bgcolor;

  int x11_fd;
  fd_set in_fds;
  struct timeval tv;

  long pv = -1;
  if (argc == 2)
    pv = strtol((char *)argv[1], NULL, 0);

  d = XOpenDisplay(NULL);
  if (d == NULL)
    die("Cannot open display.");
  Atom wm_delete = XInternAtom(d, "WM_DELETE_WINDOW", True);

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

  syslog(LOG_INFO, "colors: fg=%li bg=%li", fgcolor.pixel, bgcolor.pixel);

  Window parent = RootWindow(d, s);

  if (argc == 2)
    parent = strtol((char *)argv[1], NULL, 0);
  syslog(LOG_INFO, "parent window: %li", pv);

  /*
  XWindowAttributes attr;
  XGetWindowAttributes(d, parent, &attr);
  XVisualInfo vis;
  XMatchVisualInfo(d, s, attr.depth, TrueColor, &vis);
  */

  XSetWindowAttributes xswattr;
  xswattr.colormap = cmap;
  xswattr.border_pixel = fgcolor.pixel;
  xswattr.background_pixel = bgcolor.pixel;
  xswattr.event_mask =
      ExposureMask | StructureNotifyMask | VisibilityChangeMask;
  xswattr.bit_gravity = NorthWestGravity;
  w = XCreateWindow(
      d, parent, 0, 0, 100, 20, 0, CopyFromParent, InputOutput, CopyFromParent,
      CWBackPixel | CWBorderPixel | CWBitGravity | CWEventMask | CWColormap,
      &xswattr);

  /*
  w = XCreateSimpleWindow(d, parent, 0, 0, 100, 20, 0, fgcolor.pixel,
                          bgcolor.pixel);
  XSelectInput(d, w, ExposureMask | StructureNotifyMask | VisibilityChangeMask);
  */
  XClassHint class = {"xapm", "Xapm"};
  XWMHints wm = {.flags = InputHint, .input = 1};
  XSetWMProperties(d, w, NULL, NULL, NULL, 0, NULL, &wm, &class);
  XStoreName(d, w, "xapm");

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
        if (e.type == UnmapNotify) {
          syslog(LOG_INFO, "unmap");
          visible = 0;
        }
        if (e.type == MapNotify) {
          syslog(LOG_INFO, "map, %li", e.xmap.window);
          visible = 1;
        }
        if (e.type == VisibilityNotify) {
          syslog(LOG_INFO, "visibility");
          visible = (e.xvisibility.state != VisibilityFullyObscured) ? 1 : 0;
        }
        if (e.type == ClientMessage) {
          if (e.xclient.data.l[0] == wm_delete) {
            goto exit;
          }
        }
      }
    else if (num_ready_fds == 0)
      draw();
  }

exit:
  XFreeColormap(d, cmap);
  XCloseDisplay(d);
  XDestroyWindow(d, w);
  return 0;
}
