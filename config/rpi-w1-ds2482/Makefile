# Package name
PKGNAME := rpi-w1-ds2482

# Let makepkg guess the package version from the latest GIT tag
VERSION = $(shell /usr/share/makepkg/gitversion.sh)

# The package is architecture-independent,
DEBARCH := all
RPMARCH := noarch

# List of .deb packages required by the Makefile to run
CHECK_PACKAGES += makepkg

all:
	@true

version:
	@echo $(VERSION)

# Include makepkg rules
include /usr/share/makepkg/pkg.mk

# Copy the files to be packaged in the staging directory $(DESTDIR)
# By default, DESTDIR = out/root
.PHONY: install_clean install_bin install_init

install_clean:
	rm -rf $(DESTDIR)

install_bin:
	mkdir -p $(DESTDIR)/sbin
	install -m 755 ds2482-probe $(DESTDIR)/sbin/

install_init:
	mkdir -p $(DESTDIR)/lib/systemd/system
	install -m 644 ds2482-probe.service $(DESTDIR)/lib/systemd/system

install: all install_clean install_bin install_init
	find $(DESTDIR) -name '.*' -or -name '*~' -or -name '__*' | xargs rm -rf
