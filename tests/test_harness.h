#pragma once

#include <functional>
#include <string>
#include <vector>

struct TestCase
{
        std::string name;
        std::function<void()> func;
};

void RegisterTest( const std::string & name, std::function<void()> func );
const std::vector<TestCase> & GetRegisteredTests();
bool RunAllTests();

class TestRegistrar
{
public:
        TestRegistrar( const char * name, std::function<void()> func );
};

#define TEST_CASE(name) \
        void name(); \
        static TestRegistrar name##_registrar{ #name, name }; \
        void name()
