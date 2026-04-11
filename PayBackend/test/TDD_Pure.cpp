/**
 * Pure TDD Test - No External Dependencies
 *
 * This is a minimal TDD test that compiles with any C++17 compiler.
 * It demonstrates the complete TDD cycle without complex dependencies.
 */

#include <iostream>
#include <string>
#include <regex>
#include <cassert>

// ============================================================================
// FUNCTION UNDER TEST (Will be implemented in GREEN phase)
// ============================================================================

// RED STATE: Function not implemented (will cause compilation error)
// bool validateAmount(const std::string& amount);

// GREEN STATE: Function implemented
bool validateAmount(const std::string& amount) {
    // 1. Check not empty
    if (amount.empty()) return false;

    // 2. Check format: only digits
    std::regex digitPattern("^\\d+$");
    if (!std::regex_match(amount, digitPattern)) {
        return false;
    }

    // 3. Check value > 0
    if (std::stoi(amount) <= 0) {
        return false;
    }

    return true;
}

// ============================================================================
// TEST CASES
// ============================================================================

void testValidAmounts() {
    std::cout << "Testing valid amounts..." << std::endl;

    // Test 1: 100 (1.00 yuan)
    assert(validateAmount("100") == true && "Test 1 failed: '100' should be valid");
    std::cout << "  ✅ '100' (1.00 yuan) - PASSED" << std::endl;

    // Test 2: 10000 (100.00 yuan)
    assert(validateAmount("10000") == true && "Test 2 failed: '10000' should be valid");
    std::cout << "  ✅ '10000' (100.00 yuan) - PASSED" << std::endl;

    // Test 3: 1 (0.01 yuan)
    assert(validateAmount("1") == true && "Test 3 failed: '1' should be valid");
    std::cout << "  ✅ '1' (0.01 yuan) - PASSED" << std::endl;
}

void testInvalidAmounts() {
    std::cout << "\nTesting invalid amounts..." << std::endl;

    // Test 1: Empty string
    assert(validateAmount("") == false && "Test 1 failed: empty should be invalid");
    std::cout << "  ✅ Empty string - PASSED" << std::endl;

    // Test 2: Non-numeric
    assert(validateAmount("abc") == false && "Test 2 failed: 'abc' should be invalid");
    std::cout << "  ✅ 'abc' (non-numeric) - PASSED" << std::endl;

    // Test 3: Negative
    assert(validateAmount("-100") == false && "Test 3 failed: negative should be invalid");
    std::cout << "  ✅ '-100' (negative) - PASSED" << std::endl;

    // Test 4: Decimal format
    assert(validateAmount("0.01") == false && "Test 4 failed: decimal should be invalid");
    std::cout << "  ✅ '0.01' (decimal) - PASSED" << std::endl;
}

// ============================================================================
// MAIN
// ============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "TDD Pure Test - Amount Validation" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "\nTDD Cycle Status: GREEN ✅" << std::endl;
    std::cout << "- ✅ RED: Tests written first" << std::endl;
    std::cout << "- ✅ Verify RED: Tests failed (function not implemented)" << std::endl;
    std::cout << "- ✅ GREEN: Function implemented" << std::endl;
    std::cout << "- ⏳ Verify GREEN: Running tests now..." << std::endl;
    std::cout << "========================================\n" << std::endl;

    try {
        testValidAmounts();
        testInvalidAmounts();

        std::cout << "\n========================================" << std::endl;
        std::cout << "🎉 ALL TESTS PASSED! (GREEN state confirmed)" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "\nTDD Cycle Summary:" << std::endl;
        std::cout << "✅ RED   - Tests written first" << std::endl;
        std::cout << "✅ Verify RED - Tests failed (no function)" << std::endl;
        std::cout << "✅ GREEN  - Function implemented" << std::endl;
        std::cout << "✅ Verify GREEN - All tests pass" << std::endl;
        std::cout << "⏳ REFACTOR - Next step (optional)" << std::endl;

        return 0;  // Success

    } catch (const std::exception& e) {
        std::cerr << "\n❌ TEST FAILED!" << std::endl;
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;  // Failure
    }
}
