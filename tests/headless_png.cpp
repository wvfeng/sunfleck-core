#include "../src/theme.h"
#include "../src/ui_tree.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include <array>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <set>
#include <string>
#include <span>
#include <vector>

namespace {
using Bytes = std::vector<std::uint8_t>;

void append_be(Bytes& bytes, std::uint32_t value)
{
    for (int shift = 24; shift >= 0; shift -= 8) {
        bytes.push_back(static_cast<std::uint8_t>(value >> shift));
    }
}

void write_chunk(std::ofstream& output, std::array<std::uint8_t, 4> type,
                 std::span<const std::uint8_t> data)
{
    Bytes header;
    header.reserve(8);
    append_be(header, static_cast<std::uint32_t>(data.size()));
    header.insert(header.end(), type.begin(), type.end());
    std::uint32_t crc = 0xffffffffu;
    const auto update = [&crc](std::span<const std::uint8_t> bytes) {
        for (auto byte : bytes) {
            crc ^= byte;
            for (int bit = 0; bit < 8; ++bit) {
                crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320u : 0u);
            }
        }
    };
    update(type);
    update(data);
    Bytes checksum;
    append_be(checksum, ~crc);
    output.write(reinterpret_cast<const char*>(header.data()), header.size());
    if (!data.empty()) {
        output.write(reinterpret_cast<const char*>(data.data()), data.size());
    }
    output.write(reinterpret_cast<const char*>(checksum.data()), checksum.size());
}

bool save_png(const char* path, const std::uint8_t* rgba, std::uint32_t width,
              std::uint32_t height)
{
    Bytes scanlines;
    const auto stride = static_cast<std::size_t>(width) * 4;
    for (std::uint32_t y = 0; y < height; ++y) {
        const auto* row = rgba + y * stride;
        scanlines.push_back(0); // PNG filter: None
        scanlines.insert(scanlines.end(), row, row + stride);
    }

    // zlib stream with stored DEFLATE blocks and an Adler-32 trailer.
    Bytes stream{0x78, 0x01};
    for (std::size_t offset = 0; offset < scanlines.size();) {
        const auto remaining = scanlines.size() - offset;
        const auto length = static_cast<std::uint16_t>(remaining > 65535 ? 65535 : remaining);
        const auto inverse = static_cast<std::uint16_t>(~length);
        stream.insert(stream.end(), {static_cast<std::uint8_t>(length == remaining),
            static_cast<std::uint8_t>(length), static_cast<std::uint8_t>(length >> 8),
            static_cast<std::uint8_t>(inverse), static_cast<std::uint8_t>(inverse >> 8)});
        stream.insert(stream.end(), scanlines.begin() + offset, scanlines.begin() + offset + length);
        offset += length;
    }
    std::uint32_t a = 1, b = 0;
    for (auto byte : scanlines) {
        a = (a + byte) % 65521;
        b = (b + a) % 65521;
    }
    append_be(stream, (b << 16) | a);

    std::ofstream output(path, std::ios::binary);
    constexpr unsigned char signature[] = {137, 80, 78, 71, 13, 10, 26, 10};
    output.write(reinterpret_cast<const char*>(signature), sizeof(signature));
    Bytes header;
    append_be(header, width);
    append_be(header, height);
    header.insert(header.end(), {8, 6, 0, 0, 0});
    write_chunk(output, {'I', 'H', 'D', 'R'}, header);
    write_chunk(output, {'I', 'D', 'A', 'T'}, stream);
    write_chunk(output, {'I', 'E', 'N', 'D'}, {});
    output.close();
    return static_cast<bool>(output);
}
}

static double contrast(unsigned a, unsigned b)
{
    const auto luminance = [](unsigned rgb) {
        const auto linear = [](unsigned channel) {
            const double srgb = channel / 255.0;
            return srgb <= 0.04045 ? srgb / 12.92 : std::pow((srgb + 0.055) / 1.055, 2.4);
        };
        return 0.2126 * linear((rgb >> 16) & 255) + 0.7152 * linear((rgb >> 8) & 255) +
            0.0722 * linear(rgb & 255);
    };
    const auto first = luminance(a);
    const auto second = luminance(b);
    return (std::max(first, second) + 0.05) / (std::min(first, second) + 0.05);
}

static unsigned mix(unsigned foreground, unsigned background, unsigned alpha, unsigned scale)
{
    unsigned result = 0;
    for (unsigned shift : {16U, 8U, 0U}) {
        const unsigned front = (foreground >> shift) & 255;
        const unsigned back = (background >> shift) & 255;
        result |= ((front * alpha + back * (scale - alpha) + scale / 2) / scale) << shift;
    }
    return result;
}

static bool verify_contrast(const BuiltinTheme& theme, unsigned dark)
{
    const auto& color = theme.colors[dark];
    for (auto surface : {UI_THEME_BACKGROUND, UI_THEME_PANEL, UI_THEME_CARD, UI_THEME_CONTROL}) {
        if (contrast(color[UI_THEME_TEXT], color[surface]) < 4.5 ||
            contrast(color[UI_THEME_FOCUS], color[surface]) < 3 ||
            contrast(color[UI_THEME_DIVIDER], color[surface]) < 3) {
            return false;
        }
    }
    if (contrast(color[UI_THEME_TEXT], color[UI_THEME_SELECTED]) < 4.5) {
        return false;
    }
    for (unsigned state : {0U, 8U, 16U}) {
        if (contrast(color[UI_THEME_ON_PRIMARY],
                     mix(color[UI_THEME_ON_PRIMARY], color[UI_THEME_PRIMARY], state, 100)) < 4.5) {
            return false;
        }
    }
    if (theme.shape.glass) {
        for (unsigned backdrop : {0U, 0xffffffU}) {
            if (contrast(color[UI_THEME_TEXT],
                         mix(color[UI_THEME_PANEL], backdrop, glass_opacity, 255)) < 4.5) {
                return false;
            }
        }
    }
    return true;
}

static bool tree_images(ui_app* app, const char* output_prefix)
{
    ui_node *root = nullptr, *card = nullptr, *title = nullptr, *tree = nullptr, *branch = nullptr;
    ui_node *leaf = nullptr, *collapsed = nullptr, *hidden = nullptr, *sibling = nullptr;
    const auto add = [&](ui_node*& node, ui_node* parent, ui_node_kind kind, const char* text) {
        return ui_node_create(app, kind, &node) == UI_OK &&
            (!parent || ui_node_append(parent, node) == UI_OK) &&
            (!text || ui_node_set_text(node, text) == UI_OK);
    };
    if (!add(root, nullptr, UI_STACK_PANEL, nullptr) || !add(card, root, UI_CARD, nullptr) ||
        !add(title, card, UI_LABEL, "TreeView / TreeItem - Fluent 2 reference structure") ||
        !add(tree, card, UI_TREE_VIEW, nullptr) ||
        !add(branch, tree, UI_TREE_ITEM, "level 1, item 1") ||
        !add(leaf, branch, UI_TREE_ITEM, "level 2, item 1")) {
        return false;
    }
    for (const auto* text : {"level 2, item 2", "level 2, item 3"}) {
        ui_node* child = nullptr;
        if (!add(child, branch, UI_TREE_ITEM, text)) {
            return false;
        }
    }
    if (!add(collapsed, tree, UI_TREE_ITEM, "level 1, item 2") ||
        !add(hidden, collapsed, UI_TREE_ITEM, "level 2, item 1") ||
        !add(sibling, tree, UI_TREE_ITEM, "level 1, item 3") ||
        ui_node_set_value(branch, 1) != UI_OK || ui_app_set_root(app, root) != UI_OK ||
        ui_app_open_window(app) != UI_OK) {
        return false;
    }
    for (unsigned id = 0; id < UI_BUILTIN_THEME_COUNT; ++id) {
        for (unsigned dark : {0U, 1U}) {
            for (unsigned compact : {0U, 1U}) {
                if (ui_app_set_builtin_theme(app, static_cast<ui_builtin_theme>(id), dark, compact) != UI_OK ||
                    ui_app_poll(app, 0) != UI_OK) {
                    return false;
                }
                std::uint32_t width = 0, height = 0;
                const auto* pixels = ui_app_pixels(app, &width, &height);
                if (!pixels || leaf->bounds.left - branch->bounds.left != 24 ||
                    leaf->bounds.top != branch->visible.bottom ||
                    sibling->bounds.top != collapsed->visible.bottom) {
                    std::fprintf(stderr, "Tree geometry: indent=%d child=%d parent=%d sibling=%d collapsed=%d\n",
                        leaf->bounds.left - branch->bounds.left, leaf->bounds.top, branch->visible.bottom,
                        sibling->bounds.top, collapsed->visible.bottom);
                    return false;
                }
                const auto same = [&](int x, int y, int other_x, int other_y) {
                    const auto first = (y * width + x) * 4;
                    const auto second = (other_y * width + other_x) * 4;
                    return std::equal(pixels + first, pixels + first + 3, pixels + second);
                };
                // Default rows are transparent; a leaf must not draw a disclosure glyph.
                for (const auto* item : {branch, leaf, collapsed, sibling}) {
                    const auto bounds = item->visible;
                    const int y = (bounds.top + bounds.bottom) / 2;
                    if (!same(bounds.right - 4, y, bounds.right - 4, title->visible.top + 2)) {
                        std::fprintf(stderr, "Tree default background failed: %u/%u/%u\n", id, dark, compact);
                        return false;
                    }
                    unsigned ink = 0;
                    for (int row = bounds.top; row < bounds.bottom; ++row) {
                        for (int x = bounds.left + 8; x < bounds.left + 20; ++x) {
                            ink += !same(x, row, bounds.right - 4, row);
                        }
                    }
                    if ((ink != 0) != !item->children.empty()) {
                        std::fprintf(stderr, "Tree disclosure failed: ink=%u children=%zu\n", ink, item->children.size());
                        return false;
                    }
                }
                if (!compact) {
                    const auto path = std::string(output_prefix) + "-tree-" + std::to_string(id) +
                        (dark ? "-dark.png" : "-light.png");
                    if (!save_png(path.c_str(), pixels, width, std::min(height,
                            static_cast<unsigned>(sibling->visible.bottom + 20)))) {
                        return false;
                    }
                }
            }
        }
    }
    return true;
}

static bool showcase_images(ui_app* app, const char* source_path, const char* output_prefix)
{
    ui_app* tree_app = nullptr;
    const ui_app_options options{sizeof(options), UI_API_VERSION, UI_SOFTWARE, UI_HEADLESS};
    if (ui_app_create(&options, &tree_app) != UI_OK) {
        return false;
    }
    const bool tree_ok = ui_app_set_window_options(tree_app, "Tree reference", 1000, 760) == UI_OK &&
        tree_images(tree_app, output_prefix);
    ui_app_destroy(tree_app);
    if (!tree_ok) {
        return false;
    }
    std::ifstream source(source_path, std::ios::binary);
    const std::string fleck{std::istreambuf_iterator<char>(source), std::istreambuf_iterator<char>()};
    ui_fleck_diagnostic diagnostic{};
    ui_node *pages = nullptr, *selector = nullptr, *appearance = nullptr, *editor = nullptr, *popup = nullptr;
    if (ui_app_load_fleck(app, fleck.data(), fleck.size(), &diagnostic) != UI_OK ||
        ui_app_find_fleck_node(app, "pages", &pages) != UI_OK ||
        ui_app_find_fleck_node(app, "theme", &selector) != UI_OK ||
        ui_app_find_fleck_node(app, "dark", &appearance) != UI_OK ||
        ui_app_find_fleck_node(app, "editor", &editor) != UI_OK ||
        ui_app_find_fleck_node(app, "popup", &popup) != UI_OK ||
        ui_node_set_text(editor, "Retained input") != UI_OK ||
        ui_app_open_window(app) != UI_OK) {
        return false;
    }
    std::set<std::array<std::uint8_t, 3>> backgrounds;
    for (unsigned id = 0; id < UI_BUILTIN_THEME_COUNT; ++id) {
        const auto theme = static_cast<ui_builtin_theme>(id);
        for (unsigned dark = 0; dark <= 1; ++dark) {
            if (!ui_builtin_theme_name(theme) || !verify_contrast(builtin_themes[id], dark) ||
                ui_node_set_value(selector, id) != UI_OK ||
                ui_node_set_value(appearance, dark) != UI_OK ||
                ui_app_set_builtin_theme(app, theme, dark, 1) != UI_OK ||
                ui_app_set_builtin_theme(app, theme, dark, 0) != UI_OK ||
                ui_app_set_builtin_theme(app, UI_BUILTIN_THEME_COUNT, dark, 0) != UI_ERROR_INVALID_ARGUMENT ||
                ui_app_set_builtin_theme(app, theme, 2, 0) != UI_ERROR_INVALID_ARGUMENT ||
                ui_app_set_builtin_theme(app, theme, dark, 2) != UI_ERROR_INVALID_ARGUMENT ||
                ui_node_set_value(popup, 1) != UI_OK) {
                return false;
            }
            constexpr unsigned panel_height = 760;
            Bytes image;
            for (unsigned page : {0U, 2U, 4U}) {
                if (ui_node_set_value(pages, page) != UI_OK || ui_app_poll(app, 0) != UI_OK) {
                    return false;
                }
                std::uint32_t width = 0, height = 0;
                const auto* pixels = ui_app_pixels(app, &width, &height);
                if (!pixels || width != 1000 || height < panel_height) {
                    return false;
                }
                const auto background = (static_cast<size_t>(height - 3) * width + width - 3) * 4;
                if (page == 0 && !backgrounds.insert({pixels[background], pixels[background + 1], pixels[background + 2]}).second) {
                    return false;
                }
                image.insert(image.end(), pixels, pixels + width * panel_height * 4);
            }
            char input[32]{};
            std::uint32_t input_size = sizeof(input);
            if (ui_node_get_text(editor, input, &input_size) != UI_OK ||
                std::string(input) != "Retained input") {
                return false;
            }
            const auto path = std::string(output_prefix) + "-" + std::to_string(id) +
                (dark ? "-dark.png" : "-light.png");
            if (!save_png(path.c_str(), image.data(), 1000, panel_height * 3)) {
                return false;
            }
            if (ui_node_set_value(pages, 5) != UI_OK || ui_app_poll(app, 0) != UI_OK) { return false; }
            std::uint32_t dock_width = 0, dock_height = 0;
            const auto* dock_pixels = ui_app_pixels(app, &dock_width, &dock_height);
            const auto dock_path = std::string(output_prefix) + "-dock-" + std::to_string(id) + (dark ? "-dark.png" : "-light.png");
            if (!dock_pixels || !save_png(dock_path.c_str(), dock_pixels, dock_width, dock_height)) { return false; }
        }
    }
    return ui_builtin_theme_name(UI_BUILTIN_THEME_COUNT) == nullptr;
}

int main(int argc, char** argv)
{
    const ui_app_options options{sizeof(options), UI_API_VERSION, UI_SOFTWARE, UI_HEADLESS};
    ui_app* app = nullptr;
    if ((argc != 2 && argc != 3) || ui_app_create(&options, &app) != UI_OK) {
        return 1;
    }
    if (argc == 3) {
        const bool saved = showcase_images(app, argv[1], argv[2]);
        ui_app_destroy(app);
        return saved ? 0 : 1;
    }
    bool saved = false;
    if (ui_app_open_window(app) == UI_OK && ui_app_poll(app, 0) == UI_OK) {
        std::uint32_t width = 0, height = 0;
        if (const auto* rgba = ui_app_pixels(app, &width, &height)) {
            saved = save_png(argv[1], rgba, width, height);
        }
    }
    ui_app_destroy(app);
    return saved ? 0 : 1;
}
