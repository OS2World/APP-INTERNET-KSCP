.PHONY : all clean kpmlib

.SUFFIXES : .exe .a .lib .o .res .cpp .h .rc .d

CXX       = g++
CXXFLAGS  = -Wall -IKPMLib -DOS2EMX_PLAIN_CHAR=1
LD        = g++
LDFLAGS   =
LIBS      = -lssh2 -lcrypto -lssl -lz

ifdef RELEASE
CXXFLAGS  += -O3
LDFLAGS   += -s
STRIP      = lxlite /B- /L- /CS
else
CXXFLAGS  += -O0 -g -DDEBUG
LDFLAGS   += -g -Zomf
STRIP      = echo
endif

RC      = rc
RCFLAGS = -n

RM = rm -f

PROGRAM    = kscp
PROGRAM_RC = $(PROGRAM)rc

SRCS = kscp.cpp \
       addrbookdlg.cpp \
       KSCPClient.cpp \
       KRemoteWorkThread.cpp \
       KLocalWorkThread.cpp \
       ServerInfoVector.cpp \
       KAddrBookDlg.cpp

DEPS = $(SRCS:.cpp=.d)
OBJS = $(SRCS:.cpp=.o)

# default verbose is quiet, that is V=0
QUIET_  = @
QUIET_0 = @

QUIET = $(QUIET_$(V))

%.d : %.cpp
	$(if $(QUIET), @echo [DEP] $@)
	$(QUIET)$(CXX) $(CXXFLAGS) -MM -MP -MT "$(@:.d=.o) $@" -MF $@ $<

%.o : %.cpp
	$(if $(QUIET), @echo [CXX] $@)
	$(QUIET)$(CXX) $(CXXFLAGS) -c -o $@ $<

%.res : %.rc
	$(if $(QUIET), @echo [RC] $@)
	$(QUIET)$(RC) $(RCFLAGS) -r $< $@

all : kpmlib $(PROGRAM).exe

$(PROGRAM_RC)_DEPS  = $(PROGRAM_RC).rc
$(PROGRAM_RC)_DEPS += $(PROGRAM_RC).h

$(PROGRAM_RC).res : $($(PROGRAM_RC)_DEPS)

$(PROGRAM)_DEPS  = $(OBJS)
$(PROGRAM)_DEPS += $(PROGRAM_RC).res
$(PROGRAM)_DEPS += $(PROGRAM).def
$(PROGRAM)_DEPS += KPMLib/KPMLib.a

$(PROGRAM).exe : $($(PROGRAM)_DEPS)
	$(if $(QUIET), @echo [LD] $@)
	$(QUIET)$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)
	$(if $(QUIET), @echo [STRIP] $@)
	$(QUIET)$(STRIP) $@

kpmlib :
	$(MAKE) -C KPMLib lib RELEASE=$(RELEASE)

clean :
	$(RM) *.bak
	$(RM) *.rwp
	$(RM) $(DEPS)
	$(RM) $(OBJS)
	$(RM) $(PROGRAM_RC).res
	$(RM) $(PROGRAM).exe
	$(MAKE) -C KPMLib clean

ifeq ($(filter clean, $(MAKECMDGOALS)),)
-include $(DEPS)
endif
