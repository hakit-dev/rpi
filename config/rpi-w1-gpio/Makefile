# Package name
PKGNAME := rpi-w1-gpio

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
.PHONY: install_clean install_share

install_clean:
	rm -rf $(DESTDIR)
	mkdir -p $(DESTDIR)

install: all install_clean
	find $(DESTDIR) -name '.*' -or -name '*~' -or -name '__*' | xargs rm -rf
