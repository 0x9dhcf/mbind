# minimal binding daemon
# Copyright (c) 2019 Pierre Evenou
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

.POSIX:

MAJOR = 0
MINOR = 2
PATCH = 0

# paths
PREFIX ?= /usr/local
PKG_CONFIG = pkg-config

DEPS = xcb\
       xcb-util\
       xcb-keysyms\
       xcb-xkb\
       xcb-xtest\
       xkbcommon\
       xkbcommon-x11\

INCS = `$(PKG_CONFIG) --cflags $(DEPS)`
LIBS = `$(PKG_CONFIG) --libs $(DEPS)`

# flags
NDEBUG ?= 0
ifeq ($(NDEBUG), 1)
    CFLAGS += -O2
    CPPFLAGS += -DNDEBUG
else
    CFLAGS += -g -Wno-unused-parameter
endif
CPPFLAGS += -DVERSION=\"$(MAJOR).$(MINOR).$(PATCH)\"
CFLAGS += $(INCS) $(CPPFLAGS)
LDFLAGS += $(LIBS)

SRC = $(wildcard *.c)
HDR = $(wildcard *.h)
OBJ = $(SRC:.c=.o)

all: mbind

.c.o:
	$(CC) $(CFLAGS) -c $<

$(OBJ): $(HDR)

mbind: $(OBJ)
	$(CC) -o $@ $(OBJ) $(LDFLAGS)

clean:
	rm -f mbind $(OBJ)

install: mbind 
	mkdir -p $(PREFIX)/bin
	cp -f mbind $(PREFIX)/bin

uninstall:
	rm -f $(PREFIX)/bin/mbind

.PHONY: all clean install uninstall
