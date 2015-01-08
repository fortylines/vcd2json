# Copyright (c) 2015, Sebastien Mirolo
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

-include $(buildTop)/share/dws/prefix.mk

srcDir        ?= .
objDir        ?= .
installTop    ?= $(VIRTUAL_ENV)
binDir        ?= $(installTop)/bin
includeDir    ?= $(installTop)/include
libDir        ?= $(installTop)/lib

PYTHON        := $(binDir)/python

version       := $(shell $(PYTHON) $(srcDir)/src/setup.py --version)

bins          := vcd2json
dynlibs       := libvcd$(dylSuffix)

CPPFLAGS      += -I$(srcDir)/include -D__VCD2JSON_VERSION__=\"$(version)\"
CFLAGS        += -std=c99 -g -fPIC
LDLIBS        := -lvcd

ifeq (,$(findstring -L$(objDir), $(LDFLAGS)))
LDFLAGS       := -L$(objDir) $(LDFLAGS)
endif

ifneq ($(filter Darwin,$(shell uname)),)
dylSuffix       := .dylib
SHAREDLIBFLAGS  := -dynamiclib
else
dylSuffix       := .so
SHAREDLIBFLAGS  = -pthread -shared -Wl,-soname,$@
endif

vpath %.c $(srcDir)/src


all:: vcd2json libvcd$(dylSuffix) _vcd.so

# NOTE: The python lib needs to be installed after libvcd.so since
# we use LD_LIBRARY_PATH to find that library in setup.py.
# XXX Does not work when running rpmbuild and lib is BUILDROOT/.../usr/lib64
install:: vcd2json libvcd$(dylSuffix)
	install -d $(DESTDIR)$(binDir)
	install -s -p -m 755 vcd2json $(DESTDIR)$(binDir)
	install -d $(DESTDIR)$(includeDir)
	sed -e 's/#define __VCD2JSON_VERSION__ "(.*)"/#define __VCD2JSON_VERSION__ "$(version)"/' $(srcDir)/include/libvcd.h > $(DESTDIR)$(includeDir)/libvcd.h
	install -d $(DESTDIR)$(libDir)
	install -p -m 755 libvcd$(dylSuffix) $(DESTDIR)$(libDir)
	cd $(srcDir)/src && $(PYTHON) setup.py build -b $(CURDIR)/build \
            install --prefix=$(DESTDIR)$(installTop)

_vcd.so: wrapper.c
	cd $(srcDir)/src && $(PYTHON) setup.py build -b $(CURDIR)/build

vcd2json: vcd2json.c libvcd$(dylSuffix)
	$(LINK.c) $(filter-out %.h %.hh %.hpp %.ipp %.tcc %.def %$(dylSuffix),$^) $(LOADLIBES) $(LDLIBS) -o $@

libvcd$(dylSuffix): parser.o buf.o
	$(LINK.o) $(SHAREDLIBFLAGS) $(filter-out %.h %.hh %.hpp %.ipp %.tcc %.def,$^) -o $@

clean::
	rm -rf vcd2json libvcd$(dylSuffix) *.o *.d *~  *.dSYM $(CURDIR)/build


-include $(buildTop)/share/dws/suffix.mk
