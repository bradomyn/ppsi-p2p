# Danilo Sabato and Alessandro Rubini, 2013 -- public domain
# Modeled on arch-wrps (as we build ppsi.o only, binary is linked upstream)

CFLAGS += -ffreestanding -Os

# The bathos source tree is expected to be in ../bathos, and default is arm7
# CROSS_COMPILE is set by the user.
BATHOS_ROOT ?= ../bathos
BATHOS_ARCH ?= lpc2104

CFLAGS += -I$(BATHOS_ROOT)/include -I$(BATHOS_ROOT)/arch-$(BATHOS_ARCH) -Itools

# Let's use the pp_printf we already have in bathos
CONFIG_NO_PRINTF = y

# All files are under A (short for ARCH): I'm lazy
A := arch-$(ARCH)

LIBARCH := $A/libarch.a

LIBS += -L$A -larch


OBJ-libarch := \
	$A/bathos-task.o \
	$A/memcmp.o \
	lib/dump-funcs.o \
	lib/div64.o

# We only support our own time operations
TIME := bathos
include time-bathos/Makefile

$(LIBARCH): $(OBJ-libarch)
	$(AR) r $@ $^

$(TARGET).o: $(LIBARCH)

# The final link is performed in the bathos directory, by passing
#
#    BATHOS_EXTRAOBJ=$(CURDIR)/ppsi.o
