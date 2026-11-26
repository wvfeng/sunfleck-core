#include <sunfleck/ui.h>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <initializer_list>

int main() {
    struct Example { const char* text; double value; };
    const Example valid[] = {
        {"0", 0}, {"-0", -0.0}, {".5", .5}, {"-.5", -.5}, {"1.", 1},
        {"1e+2", 100}, {"1E-2", .01}, {"0e9999", 0},
        {"1.7976931348623157e308", std::numeric_limits<double>::max()},
        {"2.2250738585072014e-308", std::numeric_limits<double>::min()},
        {"4.9406564584124654e-324", std::numeric_limits<double>::denorm_min()},
        {"-4.9406564584124654e-324", -std::numeric_limits<double>::denorm_min()}
    };
    for (const auto& example : valid) {
        ui_binding_value result{};
        if (ui_value_parse(UI_VALUE_DOUBLE, example.text, &result, nullptr, 0) != UI_OK ||
            std::bit_cast<uint64_t>(result.number) != std::bit_cast<uint64_t>(example.value)) {
            std::fprintf(stderr, "Valid decimal failed: %s\n", example.text); return 1;
        }
    }
    for (const auto* text : {"", "+1", " 1", "1 ", "0x1", "1,5", "1e", "-", ".", "nan", "inf", "1e9999", "1e-9999"}) {
        ui_binding_value result{}; result.kind = UI_VALUE_INT; result.integer = 73;
        if (ui_value_parse(UI_VALUE_DOUBLE, text, &result, nullptr, 0) != UI_ERROR_INVALID_ARGUMENT ||
            result.kind != UI_VALUE_INT || result.integer != 73) {
            std::fprintf(stderr, "Invalid decimal changed output: %s\n", text); return 2;
        }
    }
    double geometry[2] = {7, 8}; ui_binding_value point{};
    if (ui_value_parse(UI_VALUE_POINT, " .5 , -2e+1 ", &point, geometry, 2) != UI_OK ||
        geometry[0] != .5 || geometry[1] != -20) return 3;
    if (ui_value_parse(UI_VALUE_POINT, "1,1e-9999", &point, geometry, 2) != UI_ERROR_INVALID_ARGUMENT ||
        geometry[0] != .5 || geometry[1] != -20) return 4;
    std::puts("Invariant numeric literals and atomic output passed.");
}
