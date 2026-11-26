#include "page.hpp"
#include "../controls.h"
#include <string>
#include <sunfleck/components.hpp>
class MainModel {
  public:
    ui_model *PrimaryUser;
    ui_model *SecondaryUser;
    int Score = 40;
};
extern "C" ui_result load_reusable_with_model(ui_app *, ui_model *,
                                              const ui_fleck_data_context *,
                                              ui_fleck_diagnostic *);
using Example::Rating;
extern "C" ui_result register_rating(ui_app *app) {
    uint32_t registered = 0;
    auto status = ui_app_has_component_type(app, "urn:example:controls",
                                            "Rating", &registered);
    if (status != UI_OK || registered) {
        return status;
    }
    ui_component_type type = example_rating_type();
    type.lifecycle = [](ui_component *c, ui_component_phase phase,
                        void *) -> ui_result {
        if (phase == UI_COMPONENT_INITIALIZE) {
            auto state = std::make_unique<Rating>(c);
            auto status = ui_component_set_state(c, state.get(), [](void *p) {
                delete static_cast<Rating *>(p);
            });
            if (status == UI_OK) {
                state.release();
            }
            return status;
        }
        return UI_OK;
    };
    type.changed = [](ui_component *c, const char *name, void *) {
        void *state = nullptr;
        sunfleck::detail::check(ui_component_get_state(c, &state));
        static_cast<Rating *>(state)->onPropertyChanged(name);
    };
    return ui_app_register_component_type(app, &type);
}
int main(int argc, char **argv) {
    bool test = argc > 1 && std::string_view(argv[1]) == "--test";
    sunfleck::App app(UI_SOFTWARE, test);
    sunfleck::PropertyModel first(app), second(app);
    std::string alice = "Alice", bob = "Bob";
    first.property("Name", [&] { return alice; });
    second.property("Name", [&] { return bob; });
    MainModel model{first.nativeHandle(), second.nativeHandle()};
    auto page = sunfleck::generated::load_reusable::load(app, model);
    sunfleck::Control rating(app.findFleckNode("rating"));
    rating.applyTemplate("urn:example:controls", "CompactRatingTemplate");
    rating.set("Value", 70);
    if (model.Score != 70 || rating.findNode("display").text() != "70") {
        return 1;
    }
    app.openWindow();
    if (test) {
        app.requestClose();
    } else {
        while (app.poll(100) != UI_EXITED) {
        }
    }
}
