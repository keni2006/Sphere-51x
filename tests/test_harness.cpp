#include "test_harness.h"

#include <exception>
#include <iostream>
#include <utility>
#include <vector>

namespace
{
        std::vector<TestCase> & Registry()
        {
                static std::vector<TestCase> tests;
                return tests;
        }
}

void RegisterTest( const std::string & name, std::function<void()> func )
{
        Registry().push_back( TestCase{ name, std::move( func ) } );
}

const std::vector<TestCase> & GetRegisteredTests()
{
        return Registry();
}

TestRegistrar::TestRegistrar( const char * name, std::function<void()> func )
{
        RegisterTest( name != nullptr ? std::string( name ) : std::string(), std::move( func ));
}

bool RunAllTests()
{
        bool success = true;
        auto & tests = Registry();
        for ( const auto & test : tests )
        {
                try
                {
                        test.func();
                        std::cout << "[PASS] " << test.name << '\n';
                }
                catch ( const std::exception & ex )
                {
                        std::cout << "[FAIL] " << test.name << ": " << ex.what() << '\n';
                        success = false;
                }
                catch ( ... )
                {
                        std::cout << "[FAIL] " << test.name << ": unknown exception" << '\n';
                        success = false;
                }
        }

        std::cout << "Executed " << tests.size() << " test(s)" << std::endl;
        return success;
}
