# MIT License
#
# netsp - A simple bandwidth monitor
#
# Copyright (c) 2022 Arthur Lapz (rLapz) <rlapz@gnuweeb.org>
#
# See LICENSE file for license details

TARGET  = netsp
VERSION = 0.0.1-dev

PREFIX  = /usr
CC      = cc
CFLAGS  = -std=c99 -Wall -Wextra -pedantic -O3

SRC     = netsp.c
BIN_DIR = $(DESTDIR)$(PREFIX)/bin
# -----------------------------------------------------------------------#

all: options $(TARGET)

config.h:
	cp config.def.h $(@)

$(TARGET): config.h netsp.c
	@printf "\n%s\n" "Creating executable file..."
	$(CC) $(CFLAGS) $(^) -o $(@)
# -----------------------------------------------------------------------#

options:
	@echo $(TARGET) build options:
	@echo "CFLAGS" = $(CFLAGS)
	@echo "CC"     = $(CC)

clean:
	@echo Cleaning...
	rm -f $(TARGET) config.h

install: all
	@echo Installing executable file to $(BIN_DIR)
	mkdir -p  "$(BIN_DIR)"
	cp -f     "$(TARGET)" "$(BIN_DIR)"
	chmod 755 "$(BIN_DIR)/$(TARGET)"

uninstall:
	@echo Removing executable file from $(BIN_DIR)
	rm -f "$(BIN_DIR)/$(TARGET)"

.PHONY: all options clean install uninstall
