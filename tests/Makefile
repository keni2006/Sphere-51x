CXX := g++
CXXFLAGS := -std=c++20 -Wall -Wextra -Wpedantic -I../GraySvr -I../GraySvr/Storage -I../GraySvr/Storage/MySql -I../GraySvr/Storage/Schema -I../Common -I./stubs -DUNIT_TEST -DUNIT_TEST_MYSQL_IMPLEMENTATION -DGRAY_SVR -pthread
LDFLAGS := -pthread

SRCS := \
        test_main.cpp \
        test_harness.cpp \
        storage_test_fixture.cpp \
        storage_accounts_test.cpp \
        storage_prefix_test.cpp \
        storage_world_objects_test.cpp \
        storage_unit_tests.cpp \
        stubs/mysql_stubs.cpp \
        ../GraySvr/MySqlStorageService.cpp \
        ../GraySvr/Storage/Database.cpp \
        ../GraySvr/Storage/DirtyQueue.cpp \
        ../GraySvr/Storage/MySql/ConnectionManager.cpp \
        ../GraySvr/Storage/MySql/MySqlConnection.cpp \
        ../GraySvr/Storage/MySql/MySqlLogging.cpp \
        ../GraySvr/Storage/Schema/SchemaManager.cpp

VPATH := .:stubs:..:../GraySvr:../GraySvr/Storage:../GraySvr/Storage/MySql:../GraySvr/Storage/Schema

OBJDIR := build
OBJS := $(addprefix $(OBJDIR)/,$(notdir $(SRCS:.cpp=.o)))
TARGET := storage_tests

SCRIPT_SRCS_LOCAL := \
        test_main.cpp \
        test_harness.cpp \
        script_memory_stream_test.cpp \
        script_test_stubs.cpp \
        stubs/cexpression_stub.cpp

SCRIPT_SRCS_COMMON := \
        ../Common/cscript.cpp \
        ../Common/cfile.cpp \
        ../Common/cstring.cpp \
        ../Common/cexpression.cpp

SCRIPT_OBJDIR := build_script
SCRIPT_OBJS := $(addprefix $(SCRIPT_OBJDIR)/,$(notdir $(SCRIPT_SRCS_LOCAL:.cpp=.o))) \
        $(addprefix $(SCRIPT_OBJDIR)/,$(notdir $(SCRIPT_SRCS_COMMON:.cpp=.o)))
SCRIPT_TARGET := script_tests
SCRIPT_CXXFLAGS := -std=c++20 -Wall -Wextra -Wpedantic -I../Common -pthread -DGRAY_MAP

all: $(TARGET) $(SCRIPT_TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(OBJS)

$(SCRIPT_TARGET): $(SCRIPT_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(SCRIPT_OBJS)

$(OBJDIR)/%.o: %.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(SCRIPT_OBJDIR)/%.o: %.cpp | $(SCRIPT_OBJDIR)
	$(CXX) $(SCRIPT_CXXFLAGS) -c $< -o $@

$(SCRIPT_OBJDIR)/%.o: ../Common/%.cpp | $(SCRIPT_OBJDIR)
	$(CXX) $(SCRIPT_CXXFLAGS) -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(SCRIPT_OBJDIR):
	mkdir -p $(SCRIPT_OBJDIR)

clean:
	rm -rf $(OBJDIR) $(TARGET) $(SCRIPT_OBJDIR) $(SCRIPT_TARGET)

.PHONY: all clean
