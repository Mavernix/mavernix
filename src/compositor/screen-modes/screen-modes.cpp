// The plugin "screen-modes": tells the settings window which modes each
// screen can show, which wlroots knows and Wayfire's own IPC does not say.
// A mode is chosen in the config ([display.screens.NAME] mode), not here.
//
// IPC: screen-modes/list answers { "screens": [ { "name", "description",
// "current": MODE, "modes": [MODE...] } ] }, a MODE being { "width",
// "height", "refresh" (millihertz), "preferred" }.

#include <wayfire/core.hpp>
#include <wayfire/nonstd/wlroots-full.hpp>
#include <wayfire/output-layout.hpp>
#include <wayfire/output.hpp>
#include <wayfire/plugin.hpp>
#include <wayfire/plugins/common/shared-core-data.hpp>
#include <wayfire/plugins/ipc/ipc-helpers.hpp>
#include <wayfire/plugins/ipc/ipc-method-repository.hpp>

namespace screen_modes
{
const std::string LIST_METHOD = "screen-modes/list";

static wf::json_t mode_to_json(int32_t width, int32_t height, int32_t refresh, bool preferred)
{
    wf::json_t mode;
    mode["width"] = width;
    mode["height"] = height;
    mode["refresh"] = refresh;
    mode["preferred"] = preferred;
    return mode;
}

static wf::json_t screen_to_json(wlr_output* handle)
{
    wf::json_t screen;
    screen["name"] = handle->name;
    screen["description"] = handle->description != nullptr ? handle->description : "";
    screen["current"] = mode_to_json(handle->width, handle->height, handle->refresh, false);

    // A virtual screen (headless, nested) has none: any size goes.
    wf::json_t modes = wf::json_t::array();
    wlr_output_mode* mode;
    wl_list_for_each(mode, &handle->modes, link)
    {
        modes.append(mode_to_json(mode->width, mode->height, mode->refresh, mode->preferred));
    }
    screen["modes"] = modes;
    return screen;
}

class plugin_t : public wf::plugin_interface_t
{
    wf::shared_data::ref_ptr_t<wf::ipc::method_repository_t> repo;

    wf::ipc::method_callback on_list = [](const wf::json_t&) {
        wf::json_t screens = wf::json_t::array();
        for (wf::output_t* output : wf::get_core().output_layout->get_outputs())
        {
            screens.append(screen_to_json(output->handle));
        }

        wf::json_t answer = wf::ipc::json_ok();
        answer["screens"] = screens;
        return answer;
    };

  public:
    void init() override { repo->register_method(LIST_METHOD, on_list); }

    void fini() override { repo->unregister_method(LIST_METHOD); }
};
} // namespace screen_modes

DECLARE_WAYFIRE_PLUGIN(screen_modes::plugin_t);
