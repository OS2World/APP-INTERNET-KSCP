.PHONY : all clean

.SUFFIXES : .exe .a .lib .o .res .c .h .rc .d

CC      = gcc
CFLAGS  = -Wall
LD      = gcc
LDFLAGS =
LIBS    = -lssh2 -lcrypto -lssl -lz

ifdef RELEASE
CFLAGS  += -O3
LDFLAGS += -s
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
QUIET_  = @
QUIET_0 = @

QUIET = $(QUIET_$(V))

%.d : %.c
	$(if $(QUIET), @echo [DEP] $@)
	$(QUIET)$(CC) $(CFLAGS) -MM -MP -MT "$(@:.d=.o) $@" -MF $@ $<

%.o : %.c
	$(if $(QUIET), @echo [CC] $@)
	$(QUIET)$(CC) $(CFLAGS) -c -o $@ $<

%.res : %.rc
	$(if $(QUIET), @echo [RC] $@)
	$(QUIET)$(RC) $(RCFLAGS) -r $< $@

all : $(PROGRAM).exe

$(PROGRAM_RC)_DEPS  = $(PROGRAM_RC).rc
$(PROGRAM_RC)_DEPS += $(PROGRAM_RC).h

$(PROGRAM_RC).res : $($(PROGRAM_RC)_DEPS)

$(PROGRAM)_DEPS  = $(OBJS)
$(PROGRAM)_DEPS += $(PROGRAM_RC).res
$(PROGRAM)_DEPS += $(PROGRAM).def

$(PROGRAM).exe : $($(PROGRAM)_DEPS)
	$(if $(QUIET), @echo [LD] $@)
	$(QUIET)$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)
	$(if $(QUIET), @echo [STRIP] $@)
	$(QUIET)$(STRIP) $@

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
