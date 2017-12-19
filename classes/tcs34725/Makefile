NAME := tcs34725
PKGNAME := hakit-rpi-$(NAME)

ARCH ?= $(shell arch)
OUTDIR = device/$(ARCH)

include ../../../hakit/defs.mk

SRCS = main.c i2cdev.c
OBJS = $(SRCS:%.c=$(OUTDIR)/%.o)
BIN = $(OUTDIR)/$(NAME).so

INSTALL_DIR = $(DESTDIR)/usr/lib/hakit/classes/$(NAME)/device

all:: $(BIN) $(TEST_BIN)

$(BIN): $(OBJS)

install:: all
	$(MKDIR) $(INSTALL_DIR)
	$(CP) $(BIN) $(INSTALL_DIR)/
