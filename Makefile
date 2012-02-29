.PHONY : all clean

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
RCFLAGS = -n

RM = rm -f

PROGRAM    = kscp
PROGRAM_RC = $(PROGRAM)rc

SRCS = kscp.c addrbookdlg.c windirdlg.c
DEPS = $(SRCS:.c=.d)
OBJS = $(SRCS:.c=.o)

%.d : %.c
	@echo [DEP] $@
	@$(CC) $(CFLAGS) -MM -MP -MT "$(@:.d=.o) $@" -MF $@ $<

%.o : %.c
	@echo [CC] $@
	@$(CC) $(CFLAGS) -c -o $@ $<

%.res : %.rc
	@echo [RC] $@
	@$(RC) $(RCFLAGS) -r $< $@

all : $(PROGRAM).exe

$(PROGRAM_RC)_DEPS  = $(PROGRAM_RC).rc
$(PROGRAM_RC)_DEPS += $(PROGRAM_RC).h

$(PROGRAM_RC).res : $($(PROGRAM_RC)_DEPS)

$(PROGRAM)_DEPS  = $(OBJS)
$(PROGRAM)_DEPS += $(PROGRAM_RC).res
$(PROGRAM)_DEPS += $(PROGRAM).def

$(PROGRAM).exe : $($(PROGRAM)_DEPS)
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
