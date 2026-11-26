#include <sunfleck/ui.hpp>

#include <type_traits>
#include <utility>

static_assert(!std::is_copy_constructible_v<sunfleck::App>);
static_assert(std::is_nothrow_move_constructible_v<sunfleck::App>);
static_assert(std::is_copy_constructible_v<sunfleck::Node>);

struct ActionState {
    unsigned calls = 0;
    ui_result status = UI_OK;
};

static void defer(ui_node* node, void* context) noexcept
{
    auto& state = *static_cast<ActionState*>(context);
    ++state.calls;
    state.status = ui_node_defer_action(node);
}

static bool showsDisabledFill(sunfleck::App& app)
{
    uint32_t width = 0, height = 0;
    const auto* pixels = ui_app_pixels(app.nativeHandle(), &width, &height);
    if (!pixels) {
        return false;
    }
    for (size_t i = 0; i < static_cast<size_t>(width) * height * 3; i += 3) {
        if (pixels[i] == 0xad && pixels[i + 1] == 0x64 && pixels[i + 2] == 0x9a) {
            return true;
        }
    }
    return false;
}

static bool verifyActionConcurrency(const ui_app_options& options)
{
    sunfleck::App app(options);
    auto button = app.createNode(UI_BUTTON);
    ActionState state;
    if (ui_node_set_theme_color(button.nativeHandle(), UI_THEME_CONTROL, 0xad649a) != UI_OK) {
        return false;
    }
    button.setAction(defer, &state);
    app.setRoot(button);
    app.openWindow();
    app.poll(0);

    app.activateButton();
    app.activateButton();
    if (state.calls != 1 || state.status != UI_OK || app.poll(0) != UI_OK ||
        !showsDisabledFill(app)) {
        return false;
    }
    button.completeAction();
    if (app.poll(0) != UI_OK || showsDisabledFill(app)) {
        return false;
    }
    app.activateButton();
    if (state.calls != 2 || state.status != UI_OK) {
        return false;
    }
    button.completeAction();

    button.setAction(defer, &state, sunfleck::ActionConcurrency::AllowOverlap);
    app.activateButton();
    app.activateButton();
    if (state.calls != 4 || state.status != UI_OK || app.poll(0) != UI_OK ||
        showsDisabledFill(app)) {
        return false;
    }
    button.completeAction();
    button.completeAction();
    return ui_node_complete_action(button.nativeHandle()) == UI_ERROR_INVALID_STATE;
}

int main()
{
    const ui_app_options options{sizeof(options), UI_API_VERSION, UI_SOFTWARE, UI_HEADLESS};
    sunfleck::App original(options);
    auto label = original.createNode(UI_TEXT_BLOCK);
    label.setText("Ready");
    const auto handle = original.nativeHandle();
    sunfleck::App app(std::move(original));
    if (original.nativeHandle() || app.nativeHandle() != handle || label.text() != "Ready") {
        return 1;
    }

    try {
        app.setWindowOptions("Test", 0, 180);
        return 2;
    } catch (const sunfleck::Error& error) {
        if (error.result() != UI_ERROR_INVALID_ARGUMENT) {
            return 3;
        }
    }

    app.setRoot(label);
    app.openWindow();
    if (app.poll(0) != UI_OK || app.poll(0) != UI_TIMEOUT) {
        return 4;
    }
    app.requestClose();
    return app.run() == UI_EXITED && verifyActionConcurrency(options) ? 0 : 5;
}
