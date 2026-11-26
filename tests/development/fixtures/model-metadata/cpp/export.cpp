#include "model.hpp"
#include <fstream>

int main(int argc, char** argv)
{
    if (argc != 2) { return 1; }
    try {
        sunfleck::App app(UI_SOFTWARE, true);
        sunfleck::PropertyModel properties(app);
        CounterModel model;
        model.registerProperties(properties);
        const auto json = properties.describe("CppCounter");
        if (model.reads) { return 1; }
        std::ofstream file(argv[1], std::ios::binary);
        file << "{\"version\":1,\"models\":[" << json << "]}\n";
        file.close();
        return file.fail() ? 1 : 0;
    } catch (...) { return 1; }
}
