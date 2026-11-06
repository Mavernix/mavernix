// The plugin "held-modifiers": tells the shell which modifier keys are held
// down right now, for the program switcher, which closes when its modifier
// is let go. The shell cannot tell by itself: it only hears keys while one
// of its windows has the keyboard, and a modifier let go meanwhile still
// counts as held there, even after the keyboard comes back.
//
// IPC: held-modifiers/get answers {"super", "alt", "ctrl", "shift"}, each
// true or false, for the keyboard last typed on.

#include <wayfire/core.hpp>
#include <wayfire/nonstd/wlroots-full.hpp>
#include <wayfire/plugin.hpp>
#include <wayfire/plugins/common/shared-core-data.hpp>
#include <wayfire/plugins/ipc/ipc-helpers.hpp>
#include <wayfire/plugins/ipc/ipc-method-repository.hpp>
#include <wayfire/seat.hpp>

namespace held_modifiers
{
const std::string GET_METHOD = "held-modifiers/get";

class plugin_t : public wf::plugin_interface_t
{
    wf::shared_data::ref_ptr_t<wf::ipc::method_repository_t> repo;

    wf::ipc::method_callback on_get = [](const wf::json_t&) {
        const uint32_t held = wf::get_core().seat->get_keyboard_modifiers();
        wf::json_t answer = wf::ipc::json_ok();
        answer["super"] = (held & WLR_MODIFIER_LOGO) != 0;
        answer["alt"] = (held & WLR_MODIFIER_ALT) != 0;
        answer["ctrl"] = (held & WLR_MODIFIER_CTRL) != 0;
        answer["shift"] = (held & WLR_MODIFIER_SHIFT) != 0;
        return answer;
    };

  public:
    void init() override { repo->register_method(GET_METHOD, on_get); }

    void fini() override { repo->unregister_method(GET_METHOD); }
};
} // namespace held_modifiers

DECLARE_WAYFIRE_PLUGIN(held_modifiers::plugin_t);
