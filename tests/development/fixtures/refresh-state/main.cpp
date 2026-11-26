#include <sunfleck/ui.hpp>
#include <iostream>

int main()
{
    try {
        sunfleck::App app(UI_SOFTWARE, true);
        app.loadFleck("<Window xmlns='urn:sunfleck:v1'><TextBox Id='draft'/></Window>");
        app.findFleckNode("draft").setText("Application-owned draft");
        app.openWindow();
        struct State { sunfleck::App& app; std::string draft; unsigned restored = 0; } state{app};
        app.setFleckRefreshStateCallback([](ui_app*, ui_fleck_refresh_phase phase, ui_result result, void* context) -> ui_result {
            auto& state = *static_cast<State*>(context);
            try {
                if (phase == UI_FLECK_REFRESH_CAPTURE) { state.draft = state.app.findFleckNode("draft").text(); }
                else {
                    if (result == UI_OK) {
                        // Re-resolve the ID; the old TextBox handle has been retired.
                        state.app.findFleckNode("draft").setText(state.draft.c_str());
                        ++state.restored;
                    }
                    state.draft.clear();
                }
                return UI_OK;
            } catch (...) { return UI_ERROR_INVALID_STATE; }
        }, &state);
        app.queueFleckRefresh("<Window xmlns='urn:sunfleck:v1'><TextBlock Id='draft'/></Window>");
        app.poll(0);
        if (app.findFleckNode("draft").text() != "Application-owned draft" || state.restored != 1) { return 1; }
        app.setFleckRefreshStateCallback(nullptr);
        std::cout << "C++ restored application state onto the replacement node\n";
        return 0;
    } catch (...) { return 1; }
}
