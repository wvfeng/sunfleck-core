#include <sunfleck/ui.h>

static int pixel(ui_app* app, unsigned x, unsigned y, unsigned red)
{
    uint32_t width = 0, height = 0;
    const uint8_t* pixels = ui_app_pixels(app, &width, &height);
    return pixels && x < width && y < height && pixels[(y * width + x) * 4] == red;
}

#ifdef _WIN32
static int has_label_ink(ui_app* app, unsigned left, unsigned top, unsigned bottom)
{
    uint32_t width = 0, height = 0;
    const uint8_t* pixels = ui_app_pixels(app, &width, &height);
    if (!pixels || left + 62 > width || bottom > height) {
        return 0;
    }
    for (unsigned y = top; y < bottom; ++y) {
        for (unsigned x = left; x < left + 62; ++x) {
            if (pixels[(y * width + x) * 4] < 240) {
                return 1;
            }
        }
    }
    return 0;
}

static int has_smoothed_edge(ui_app* app)
{
    uint32_t width = 0, height = 0;
    const uint8_t* pixels = ui_app_pixels(app, &width, &height);
    if (!pixels || width < 110 || height < 112) {
        return 0;
    }
    for (unsigned y = 96; y < 112; ++y) {
        for (unsigned x = 48; x < 110; ++x) {
            const unsigned red = pixels[(y * width + x) * 4];
            if (red > 32 && red < 255) {
                return 1;
            }
        }
    }
    return 0;
}
#endif

int main(void)
{
    ui_app_options options = {sizeof(options), UI_API_VERSION, UI_SOFTWARE, UI_HEADLESS};
    ui_app* app = 0;
    if (ui_app_create(&options, &app) != UI_OK) {
        return 1;
    }
    uint8_t image[] = {173, 80, 40};
    const int passed = ui_app_set_gallery(app, 0, 0) == UI_OK &&
        ui_app_open_window(app) == UI_OK && ui_app_poll(app, 0) == UI_OK &&
        pixel(app, 0, 0, 251) && pixel(app, 30, 55, 136) && pixel(app, 40, 90, 255) &&
#ifdef _WIN32
        has_label_ink(app, 48, 96, 112) && has_label_ink(app, 48, 112, 131) &&
        has_label_ink(app, 178, 96, 112) && has_label_ink(app, 178, 112, 131) &&
        has_smoothed_edge(app) &&
#endif
        pixel(app, 56, 158, 136) &&
        ui_app_set_gallery_image(app, image, 129, 1) == UI_ERROR_INVALID_ARGUMENT &&
        ui_app_set_gallery_image(app, image, 1, 1) == UI_OK &&
        (image[0] = 0, 1) && ui_app_poll(app, 0) == UI_OK && pixel(app, 56, 158, 173) &&
        ui_app_activate_button(app) == UI_OK && ui_app_poll(app, 0) == UI_OK &&
        pixel(app, 30, 40, 243) &&
        ui_app_set_gallery(app, 1, 1) == UI_OK && ui_app_poll(app, 0) == UI_OK &&
        pixel(app, 0, 0, 33) && pixel(app, 30, 55, 33) && pixel(app, 30, 40, 80) &&
        ui_app_set_gallery(app, 1, 1) == UI_OK && ui_app_poll(app, 0) == UI_TIMEOUT &&
        ui_app_activate_button(app) == UI_OK && ui_app_poll(app, 0) == UI_OK &&
        pixel(app, 30, 40, 243);
    ui_app_destroy(app);
    return passed ? 0 : 1;
}
