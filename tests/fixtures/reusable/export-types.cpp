#include "controls.h"
#include <sunfleck/component_metadata.hpp>
#include <filesystem>
#include <fstream>
int main(int argc, char **argv) {
    if (argc != 2) { return 1; }
    const auto file = (std::filesystem::path(__FILE__).parent_path() / "controls.h").generic_string();
    const sunfleck::metadata::Source properties[] = {{file.c_str(), example_value_line}};
    const sunfleck::metadata::Type types[] = {
        {example_user_card_type(), "Example.UserCard", "sunfleck::UserControl", {file.c_str(), Example::user_card_line}},
        {example_rating_type(), "Example.Rating", "sunfleck::Control", {file.c_str(), Example::rating_line}, "register_rating", properties}
    };
    std::ofstream output(argv[1], std::ios::binary);
    sunfleck::metadata::write(output, types);
    return output ? 0 : 1;
}
