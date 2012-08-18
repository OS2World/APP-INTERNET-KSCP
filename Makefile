.PHONY : all clean

.SUFFIXES : .exe .a .lib .o .res .c .h .rc .d

CC      = gcc
CFLAGS  = -Wall
LD      = gcc
LDFLAGS =
LIBS    = -lssh2 -lcrypto -lssl -lz

ifdef RELEASE
CFLAGS  += -O3
STRIP    = lxlite /B- /L- /CS
else
CFLAGS  += -O0 -g -DDEBUG
LDFLAGS += -g -Zomf
STRIP    = echo
endif

RC      = rc
RCFLAGS = -n

RM = rm -f

PROGRAM    = kscp
PROGRAM_RC = $(PROGRAM)rc

SRCS = kscp.c addrbookdlg.c windirdlg.c
DEPS = $(SRCS:.c=.d)
OBJS = $(SRCS:.c=.o)

# default verbose is quiet, that is V=0
VERBOSE_  = @
VERBOSE_0 = @

VERBOSE = $(VERBOSE_$(V))

%.d : %.c
	$(if $(VERBOSE), @echo [DEP] $@)
	$(VERBOSE)$(CC) $(CFLAGS) -MM -MP -MT "$(@:.d=.o) $@" -MF $@ $<

%.o : %.c
	$(if $(VERBOSE), @echo [CC] $@)
	$(VERBOSE)$(CC) $(CFLAGS) -c -o $@ $<

%.res : %.rc
	$(if $(VERBOSE), @echo [RC] $@)
	$(VERBOSE)$(RC) $(RCFLAGS) -r $< $@

all : $(PROGRAM).exe

$(PROGRAM_RC)_DEPS  = $(PROGRAM_RC).rc
$(PROGRAM_RC)_DEPS += $(PROGRAM_RC).h

$(PROGRAM_RC).res : $($(PROGRAM_RC)_DEPS)

$(PROGRAM)_DEPS  = $(OBJS)
$(PROGRAM)_DEPS += $(PROGRAM_RC).res
$(PROGRAM)_DEPS += $(PROGRAM).def

$(PROGRAM).exe : $($(PROGRAM)_DEPS)
	$(if $(VERBOSE), @echo [LD] $@)
	$(VERBOSE)$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)
	$(if $(VERBOSE), @echo [STRIP] $@)
	$(VERBOSE)$(STRIP) $@

clean :
	$(RM) *.bak
	$(RM) *.rwp
	$(RM) $(DEPS)
	$(RM) $(OBJS)
	$(RM) $(PROGRAM_RC).res
	$(RM) $(PROGRAM).exe

ifeq ($(filter clean, $(MAKECMDGOALS)),)
-include $(DEPS)
endif
