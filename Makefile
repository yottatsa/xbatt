CFLAGS := -O0 -g -DDEBUG=1
LDLIBS := -lX11

all: xapm procapm
xapm: xapm.o

prod: clean
	$(MAKE) CFLAGS="$(CFLAGS_P)" xapm
clean:
	rm -f xapm xapm.o
.PHONY: all prod clean
