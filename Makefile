#
# Copyright (c) 2012-2013 by KO Myung-Hun <komh@chollian.net>
# All rights reserved.
#
# Redistribution and use in source and binary forms,
# with or without modification, are permitted provided
# that the following conditions are met:
#
#   Redistributions of source code must retain the above
#   copyright notice, this list of conditions and the
#   following disclaimer.
#
#   Redistributions in binary form must reproduce the above
#   copyright notice, this list of conditions and the following
#   disclaimer in the documentation and/or other materials
#   provided with the distribution.
#
#   Neither the name of the copyright holder nor the names
#   of any other contributors may be used to endorse or
#   promote products derived from this software without
#   specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
# CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
# INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
# USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
# OF SUCH DAMAGE.
#

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
	$(MAKE) -C KPMLib KPMLib RELEASE=$(RELEASE)

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
