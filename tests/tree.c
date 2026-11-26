#include <sunfleck/ui.h>

static ui_node* create(ui_app* app, ui_node* parent, ui_node_kind kind)
{
    ui_node* node = 0;
    if (ui_node_create(app, kind, &node) != UI_OK ||
        (parent && ui_node_append(parent, node) != UI_OK)) {
        return 0;
    }
    return node;
}

static int color(ui_app* app, unsigned x, unsigned y, unsigned red)
{
    uint32_t width, height;
    const uint8_t* pixels = ui_app_pixels(app, &width, &height);
    return pixels && x < width && y < height && pixels[(y * width + x) * 4] == red;
}

static int verify(ui_app* app)
{
    ui_node* scroll = create(app, 0, UI_SCROLL_VIEW);
    if (!scroll) {
        return 1;
    }
    ui_node* panel = create(app, scroll, UI_PANEL);
    if (!panel) {
        return 1;
    }
    ui_node* card = create(app, panel, UI_CARD);
    if (!card) {
        return 1;
    }
    ui_node* image = create(app, card, UI_IMAGE);
    ui_node* progress = create(app, card, UI_PROGRESS_BAR);
    uint8_t pixels[] = {173, 80, 40, 179, 40, 80};
    if (!image || !progress || ui_node_set_image(image, pixels, 2, 1) != UI_OK ||
        ui_node_set_value(progress, 70) != UI_OK || ui_app_set_root(app, scroll) != UI_OK ||
        ui_app_open_window(app) != UI_OK || ui_app_poll(app, 0) != UI_OK) {
        return 1;
    }
    if (!color(app, 36, 36, 173) || !color(app, 70, 36, 179) ||
        !color(app, 36, 56, 255) || !color(app, 60, 103, 136)) {
        return 2;
    }
    pixels[0] = 90;
    if (ui_app_poll(app, 0) != UI_TIMEOUT || !color(app, 36, 36, 173) ||
        ui_node_set_image(image, pixels, 1, 1) != UI_OK ||
        ui_node_set_value(progress, 0) != UI_OK ||
        ui_app_set_style(app, 1, 0) != UI_OK || ui_app_poll(app, 0) != UI_OK ||
        !color(app, 36, 36, 90) || !color(app, 60, 103, 58)) {
        return 3;
    }
    ui_theme theme;
    if (ui_theme_init(&theme, 1, 0) != UI_OK ||
        ui_theme_apply_overrides(&theme, "background=#102030\ngap=999") != UI_ERROR_INVALID_ARGUMENT ||
        theme.colors[UI_THEME_BACKGROUND] != 0x211c17 ||
        ui_theme_apply_overrides(&theme, " background = #102030\r\nprimary=#aa5533\n") != UI_OK ||
        ui_node_set_theme_color(progress, UI_THEME_PRIMARY, 0x554433) != UI_OK ||
        ui_node_set_value(progress, 80) != UI_OK || ui_app_set_theme(app, &theme) != UI_OK ||
        ui_app_poll(app, 0) != UI_OK || !color(app, 0, 0, 16) || !color(app, 60, 103, 85) ||
        ui_app_set_theme(app, &theme) != UI_OK || ui_app_poll(app, 0) != UI_TIMEOUT) {
        return 6;
    }
    if (ui_app_set_style(app, 1, 0) != UI_OK) {
        return 7;
    }
    ui_node* overlay_root = create(app, 0, UI_STACK_PANEL);
    ui_node* popup = overlay_root ? create(app, overlay_root, UI_POPUP) : 0;
    if (!popup || !create(app, popup, UI_BUTTON) || ui_app_set_root(app, overlay_root) != UI_OK ||
        ui_app_poll(app, 0) != UI_OK || !color(app, 20, 20, 33) ||
        ui_node_set_value(popup, 1) != UI_OK || ui_app_poll(app, 0) != UI_OK) {
        return 4;
    }
    if (!color(app, 25, 25, 243) || ui_node_set_value(popup, 0) != UI_OK ||
        ui_app_poll(app, 0) != UI_OK || !color(app, 20, 20, 33)) {
        return 5;
    }
    ui_node* table = create(app, 0, UI_DATA_GRID);
    if (!table) {
        return 8;
    }
    for (unsigned i = 0; i < 8; ++i) {
        ui_node* row = create(app, table, UI_GRID);
        if (!row || !create(app, row, UI_TEXT_BOX)) {
            return 8;
        }
    }
    const unsigned thumb_red = (theme.colors[UI_THEME_DIVIDER] >> 16) & 0xff;
    const unsigned track_red = (theme.colors[UI_THEME_CONTROL] >> 16) & 0xff;
    if (ui_app_set_root(app, table) != UI_OK || ui_app_poll(app, 0) != UI_OK ||
        !color(app, 300, 20, thumb_red) || !color(app, 300, 180, track_red)) {
        return 9;
    }
    return 0;
}

static int verify_image_source(ui_app* app)
{
    const char page[] = "<Window xmlns='urn:sunfleck:v1'><Image Width='80' Height='80' Stretch='Uniform' "
                        "Source='{Binding Picture}'/></Window>";
    const uint8_t first[] = {193, 34, 56, 255};
    const uint8_t second[] = {37, 64, 212, 255};
    ui_model* model = 0;
    ui_text_source* key = 0;
    if (ui_model_create(app, &model) != UI_OK || ui_text_source_create(app, "first", &key) != UI_OK ||
        ui_model_add_text_source(model, "Picture", key) != UI_OK ||
        ui_app_load_fleck_with_resources(app, page, sizeof(page) - 1, 0, 0, model, 0) != UI_OK ||
        ui_app_open_window(app) != UI_OK ||
        ui_app_register_image_rgba(app, "first", first, 1, 1) != UI_OK ||
        ui_app_poll(app, 0) != UI_OK || !color(app, 60, 60, first[0])) {
        return 10;
    }
    if (ui_app_register_image_rgba(app, "second", second, 1, 1) != UI_OK ||
        ui_text_source_set(key, "second") != UI_OK || ui_app_poll(app, 0) != UI_OK ||
        !color(app, 60, 60, second[0])) {
        return 11;
    }
    if (ui_text_source_set(key, "missing") != UI_OK || ui_app_poll(app, 0) != UI_OK ||
        color(app, 60, 60, second[0])) {
        return 12;
    }
    return 0;
}

int main(void)
{
    const ui_app_options options = {sizeof(options), UI_API_VERSION, UI_SOFTWARE, UI_HEADLESS};
    ui_app* app = 0;
    if (ui_app_create(&options, &app) != UI_OK) {
        return 1;
    }
    int result = verify(app);
    ui_app_destroy(app);
    if (!result) {
        if (ui_app_create(&options, &app) != UI_OK) {
            return 1;
        }
        result = verify_image_source(app);
        ui_app_destroy(app);
    }
    return result;
}
