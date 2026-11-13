#pragma once

#include <iostream>
#include <string>

class TestRunner
{
public:
    TestRunner()
        : _passed(0), _failed(0), _printPass(true)
    {
    }

    void checkTrue(
        const std::string& name,
        bool condition
    )
    {
        if (condition)
        {
            ++_passed;
            if (_printPass)
                std::cout << "[PASS] " << name << '\n';
        }
        else
        {
            ++_failed;
            std::cerr << "[FAIL] " << name << '\n';
        }
    }

    template <typename T>
    void checkEqual(
        const std::string& name,
        const T& expected,
        const T& actual
    )
    {
        if (expected == actual)
        {
            ++_passed;
            if (_printPass)
                std::cout << "[PASS] " << name << '\n';
        }
        else
        {
            ++_failed;
            std::cerr
                << "[FAIL] " << name
                << "\n  expected: " << expected
                << "\n  actual:   " << actual
                << '\n';
        }
    }

    void setPrintPass(bool enabled) { _printPass = enabled; }

    int failedCount() const { return _failed; }

    int finish() const
    {
        std::cout
            << _passed << " passed, "
            << _failed << " failed.\n\n";

        return _failed == 0 ? 0 : 1;
    }

private:
    int _passed;
    int _failed;
    bool _printPass;
};