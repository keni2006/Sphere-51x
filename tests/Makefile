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

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(OBJS)

$(OBJDIR)/%.o: %.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

clean:
	rm -rf $(OBJDIR) $(TARGET)

.PHONY: all clean
