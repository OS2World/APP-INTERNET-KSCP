.PHONY : all

.SUFFIXES : .exe .a .lib .o .res .c .h .rc

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

.c.o :
	$(CC) $(CFLAGS) -c -o $@ $<

.rc.res :
	$(RC) $(RCFLAGS) -r $< $@

all : kscp.exe

kscp.o : kscp.c kscp.h windirdlg.h addrbookdlg.h kscprc.h

windirdlg.o : windirdlg.c windirdlg.h

addrbookdlg.o : addrbookdlg.c kscp.h windirdlg.h addrbookdlg.h kscprc.h

kscp.res : kscprc.rc kscprc.h

kscp.exe : kscp.o windirdlg.o addrbookdlg.o kscprc.res kscp.def
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)
	$(STRIP) $@

clean :
	$(RM) *.bak
	$(RM) *.rwp
	$(RM) *.o
	$(RM) *.res
	$(RM) *.exe
