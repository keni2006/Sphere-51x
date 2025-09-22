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

# Ask a MariaDB/MySQL config helper for the necessary include and library
# flags.  Preference is given to the MariaDB Connector/C drop that
# `scripts/fetch_mariadb_connector.bat` installs under `../third_party`, while
# still falling back to any system-wide mariadb_config/mysql_config helpers.
MARIADB_DIR            ?= ../third_party/mariadb-connector-c-3.3.8-linux
MARIADB_CONFIG_LOCAL   := $(abspath $(MARIADB_DIR))/bin/mariadb_config
MARIADB_CONFIG_SYSTEM  := $(shell command -v mariadb_config 2>/dev/null)
MYSQL_CONFIG_SYSTEM    := $(shell command -v mysql_config 2>/dev/null)

ifeq ($(wildcard $(MARIADB_CONFIG_LOCAL)),)
  ifneq ($(strip $(MARIADB_CONFIG_SYSTEM)),)
    MYSQL_CFLAGS   := $(shell $(MARIADB_CONFIG_SYSTEM) --cflags 2>/dev/null)
    MYSQL_LIBS     := $(shell $(MARIADB_CONFIG_SYSTEM) --libs 2>/dev/null)
  else ifneq ($(strip $(MYSQL_CONFIG_SYSTEM)),)
    MYSQL_CFLAGS   := $(shell $(MYSQL_CONFIG_SYSTEM) --cflags 2>/dev/null)
    MYSQL_LIBS     := $(shell $(MYSQL_CONFIG_SYSTEM) --libs 2>/dev/null)
  endif
else
  MYSQL_CFLAGS     := $(shell $(MARIADB_CONFIG_LOCAL) --cflags 2>/dev/null)
  MYSQL_LIBS       := $(shell $(MARIADB_CONFIG_LOCAL) --libs 2>/dev/null)
endif

ifneq ($(strip $(MYSQL_CFLAGS)),)
CPPFLAGS       += $(MYSQL_CFLAGS)
endif

ifneq ($(strip $(MYSQL_LIBS)),)
LDLIBS         += $(MYSQL_LIBS)
endif

ifneq ($(wildcard $(MARIADB_DIR)/include),)
CPPFLAGS       += -I$(MARIADB_DIR)/include -I$(MARIADB_DIR)/include/mysql
endif

ifneq ($(strip $(MYSQL_LIBS)),)
RPATH_DIRS     := $(patsubst -L%,%,$(filter -L%,$(MYSQL_LIBS)))
comma          := ,
LDFLAGS        += $(addprefix -Wl$(comma)-rpath$(comma),$(RPATH_DIRS))
endif

# ---------------------------------------------------------------------------
# Source discovery
SRC            := $(filter-out GrayLog.cpp,$(wildcard *.cpp))
SRC            += $(wildcard Storage/*.cpp) $(wildcard Storage/*/*.cpp)
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
