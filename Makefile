.PHONY : all

.SUFFIXES : .exe .a .lib .o .res .c .h .rc .d

CC      = gcc
CFLAGS  = -Wall
LDFLAGS =
LIBS    = -lssh2 -lcrypto -lssl -lz

ifdef RELEASE
CFLAGS  += -O3
STRIP    = lxlite /B- /L- /CS
else
CFLAGS  += -O0 -g -DDEBUG
LDFLAGS += -g -Zomf
STRIP    = @echo
endif

RC = rc
RCFLAGS =

RM = rm -f

SRCS = kscp.c addrbookdlg.c windirdlg.c
DEPS = $(SRCS:.c=.d)
OBJS = $(SRCS:.c=.o)

ADD_D = sed -e 's/^\(.*\)\.o\(.*\)/\1.o \1.d\2/g'

.c.d :
	@echo [DEP] $@
	@$(CC) $(CFLAGS) -MM -MT $(@:.d=.o) $< | $(ADD_D) > $@

.c.o :
	@echo [CC] $@
	@$(CC) $(CFLAGS) -c -o $@ $<

.rc.res :
	@echo [RC] $@
	@$(RC) $(RCFLAGS) -r $< $@

all : kscp.exe

kscp.res : kscprc.rc kscprc.h

kscp.exe : $(OBJS) kscprc.res kscp.def
	@echo [LD] $@
	@$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)
	@$(STRIP) $@

clean :
	$(RM) *.bak
	$(RM) *.rwp
	$(RM) *.o
	$(RM) *.res
	$(RM) *.exe
	$(RM) *.d

ifeq ($(filter clean, $(MAKECMDGOALS)),)
-include $(DEPS)
endif
