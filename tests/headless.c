#include <sunfleck/ui.h>
#include <string.h>

int main(void)
{
    ui_app_options options = {sizeof(options), UI_API_VERSION, UI_SOFTWARE, UI_HEADLESS};
    ui_app* rejected = (ui_app*)1;
    ui_app_options old = options; old.api_version = UI_API_VERSION - 1;
    if (ui_api_version() != UI_API_VERSION || ui_app_create(&old, &rejected) != UI_ERROR_VERSION_MISMATCH || rejected != NULL) return 2;
    old.api_version = UI_API_VERSION + 1;
    if (ui_app_create(&old, &rejected) != UI_ERROR_VERSION_MISMATCH || rejected != NULL) return 3;
    for (int cycle = 0; cycle < 10; ++cycle) {
        ui_app* app = 0;
        if (ui_app_create(&options, &app) != UI_OK ||
            ui_app_open_window(app) != UI_OK) {
            ui_app_destroy(app);
            return 1;
        }
        int failed = 0;
        for (int active = 0; active < 2; ++active) {
            uint32_t width = 0, height = 0;
            failed |= ui_app_poll(app, 0) != UI_OK;
            const uint8_t* pixels = ui_app_pixels(app, &width, &height);
            if (!pixels || width != 320 || height != 200) {
                failed = 1;
                break;
            }
            const uint8_t background[] = {251, 248, 242, 255};
            const uint8_t idle[] = {136, 73, 0, 255};
            const uint8_t pressed[] = {243, 223, 190, 255};
            for (uint32_t y = 0; y < height; ++y) {
                for (uint32_t x = 0; x < width; ++x) {
                    const uint8_t* expected = x >= 24 && x < 144 && y >= 24 && y < 64
                        ? (active ? pressed : idle) : background;
                    failed |= memcmp(pixels + (y * width + x) * 4, expected, 4) != 0;
                }
            }
            failed |= ui_app_poll(app, 0) != UI_TIMEOUT;
            if (!active) {
                failed |= ui_app_activate_button(app) != UI_OK;
            }
        }
        failed |= ui_app_request_close(app) != UI_OK;
        failed |= ui_app_poll(app, 0) != UI_EXITED;
        ui_app_destroy(app);
        if (failed) {
            return 1;
        }
    }
    return 0;
}
