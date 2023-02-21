CFLAGS := -O0 -g -DDEBUG=1
CFLAGS_P := -O2 -Wall
LDLIBS := -lX11

all: xapm procapm
xapm: xapm.o

prod: clean
	$(MAKE) CFLAGS="$(CFLAGS_P)" xapm
	rm -f xapm.o
	strip xapm

clean:
	rm -f xapm xapm.o
.PHONY: all prod clean
