# Generic makefile capable of building the Sphere server on Linux.
#
# The historical makefile shipped with the project expected a
# case-insensitive filesystem and listed every source file explicitly
# using lowercase paths (e.g. `../common/carray.cpp`).  The repository
# however stores the sources inside `Common/` with mixed-case file
# names, which means a modern Linux build would immediately stop with
# "No rule to make target" errors.  In addition the original compile
# commands used the broken token `$ <` instead of `$<`, so even if the
# paths were fixed the compiler never received the name of the current
# translation unit.
#
# This replacement automates the source discovery, wires up the correct
# include paths and resolves the MySQL dependencies through
# `mysql_config` so we no longer have to guess distribution specific
# directories.

EXE            ?= graysvr

CXX            ?= g++
WARN           ?= -w
OPT            ?=

# ---------------------------------------------------------------------------
# Compiler and pre-processor flags
CXXFLAGS       += $(WARN) $(OPT) -pipe
CPPFLAGS       += -DGRAY_SVR -I../Common

# Ask `mysql_config` for the necessary include and library flags.  This keeps
# the build portable across distributions where MySQL headers and libraries
# might live in different directories.
MYSQL_CFLAGS   := $(shell mysql_config --cflags 2>/dev/null)
MYSQL_LIBS     := $(shell mysql_config --libs 2>/dev/null)

ifneq ($(strip $(MYSQL_CFLAGS)),)
CPPFLAGS       += $(MYSQL_CFLAGS)
endif

ifneq ($(strip $(MYSQL_LIBS)),)
LDLIBS         += $(MYSQL_LIBS)
endif

# ---------------------------------------------------------------------------
# Source discovery
SRC            := $(filter-out GrayLog.cpp,$(wildcard *.cpp))
COMMON_EXCLUDE := ../Common/cbitmap.cpp ../Common/cdata.cpp ../Common/cwindow.cpp ../Common/cmodver.cpp
COMMONSRC      := $(filter-out $(COMMON_EXCLUDE),$(wildcard ../Common/*.cpp))

OBJ            := $(SRC:.cpp=.o)
COMMONOBJ      := $(patsubst ../Common/%.cpp,../Common/%.o,$(COMMONSRC))

.PHONY: all clean tidy

all: $(EXE)

$(EXE): $(OBJ) $(COMMONOBJ)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

clean: tidy
	rm -f $(OBJ) $(COMMONOBJ) $(EXE)

tidy:
	rm -f *~ *orig *bak *rej ../Common/*~ ../Common/*orig ../Common/*bak

# ---------------------------------------------------------------------------
# Pattern rules
%.o: %.cpp
	@echo " Compiling $<"
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

../Common/%.o: ../Common/%.cpp
	@echo " Compiling $<"
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@
