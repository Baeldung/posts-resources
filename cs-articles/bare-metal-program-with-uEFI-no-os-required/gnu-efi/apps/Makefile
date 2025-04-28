#
#  Copyright (C) 1999-2001 Hewlett-Packard Co.
#	Contributed by David Mosberger <davidm@hpl.hp.com>
#	Contributed by Stephane Eranian <eranian@hpl.hp.com>
#
#    All rights reserved.
#
#    Redistribution and use in source and binary forms, with or without
#    modification, are permitted provided that the following conditions
#    are met:
#
#    * Redistributions of source code must retain the above copyright
#      notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
#      copyright notice, this list of conditions and the following
#      disclaimer in the documentation and/or other materials
#      provided with the distribution.
#    * Neither the name of Hewlett-Packard Co. nor the names of its
#      contributors may be used to endorse or promote products derived
#      from this software without specific prior written permission.
#
#    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
#    CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
#    INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
#    MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
#    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
#    BE LIABLE FOR ANYDIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
#    OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
#    PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
#    PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
#    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
#    TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
#    THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
#    SUCH DAMAGE.
#

SRCDIR = .

VPATH = $(SRCDIR)
TOPDIR = $(SRCDIR)/..

include $(SRCDIR)/../Make.defaults

LINUX_HEADERS	= /usr/src/sys/build
APPSDIR		= $(LIBDIR)/gnuefi/apps
CPPFLAGS	+= -D__KERNEL__ -I$(LINUX_HEADERS)/include

ifneq ($(HAVE_EFI_OBJCOPY),)
ifeq ($(SYSTEM_HAS_EFI_OBJCOPY),0)
CRT0_LOCAL = -local
LDS_LOCAL = _local
endif
endif

ifeq ($(IS_MINGW32),)
# The CRT0 libs are only needed for non MinGW platforms
CRT0_LIBS	= -lgnuefi
endif

ifneq ($(CRT0_LIBS),)
CRTOBJS		= $(TOPDIR)/$(ARCH)/gnuefi/crt0-efi-$(ARCH)$(CRT0_LOCAL).o

LDSCRIPT	= $(TOPDIR)/gnuefi/elf_$(ARCH)_efi$(LDS_LOCAL).lds
ifeq ($(USING_FREEBSD),1)
LDSCRIPT	= $(TOPDIR)/gnuefi/elf_$(ARCH)_fbsd_efi.lds
endif
LDFLAGS		+= -L$(TOPDIR)/$(ARCH)/lib -L$(TOPDIR)/$(ARCH)/gnuefi $(CRTOBJS)
LOADLIBES	+= -T $(LDSCRIPT)
endif

LDFLAGS		+= -shared -Bsymbolic -L$(TOPDIR)/$(ARCH)/lib

LOADLIBES	+= -lefi $(CRT0_LIBS)
LOADLIBES	+= $(LIBGCC)

TARGET_APPS  = baeldung.efi baeldung-gui.efi

SUBSYSTEM	:= 0xa
$(TARGET_BSDRIVERS): SUBSYSTEM = 0xb
$(TARGET_RTDRIVERS): SUBSYSTEM = 0xc

ifeq ($(SYSTEM_HAS_EFI_OBJCOPY),1)

FORMAT		:= -O efi-app-$(ARCH)
$(TARGET_BSDRIVERS): FORMAT=-O efi-bsdrv-$(ARCH)
$(TARGET_RTDRIVERS): FORMAT=-O efi-rtdrv-$(ARCH)

ifneq ($(IS_MINGW32),)
LDFLAGS     += -s -Wl,-dll -Wl,--subsystem,$(SUBSYSTEM) 
ifeq ($(ARCH),ia32)
LDFLAGS     += -e _efi_main
else
LDFLAGS     += -e efi_main
endif
endif

else

FORMAT		:= -O binary
LDFLAGS		+= --defsym=EFI_SUBSYSTEM=$(SUBSYSTEM)

endif

TARGETS = $(TARGET_APPS)

all:	$(TARGETS)

ctors_test.so : ctors_fns.o ctors_test.o

clean:
	@rm -vf $(TARGETS) *~ *.o *.so

install:
	mkdir -p $(INSTALLROOT)$(APPSDIR)
	$(INSTALL) -m 644 $(TARGETS) $(INSTALLROOT)$(APPSDIR)

.PHONY: install

include $(SRCDIR)/../Make.rules
