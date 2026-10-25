#ifndef DECO_SUBSURFACE_HPP
#define DECO_SUBSURFACE_HPP

#include "wayfire/object.hpp"
#include "wayfire/toplevel.hpp"
#include <wayfire/signal-definitions.hpp>
#include <wayfire/toplevel-view.hpp>

#include "corner-clip.hpp"

class simple_decoration_node_t;
namespace wf
{
/**
 * A decorator object attached as custom data to a toplevel object.
 */
class simple_decorator_t : public wf::custom_data_t
{
    wayfire_toplevel_view view;
    // For the destructor: a transaction can keep the toplevel (and this)
    // alive after the window is gone, leaving `view` dangling.
    std::weak_ptr<wf::toplevel_view_interface_t> view_while_alive;
    std::shared_ptr<simple_decoration_node_t> deco;
    // Not used for a fullscreen window, or when it cannot draw.
    std::shared_ptr<corner_clip_transformer_t> corner_clip;
    void update_corner_clip();

    wf::signal::connection_t<wf::view_activated_state_signal> on_view_activated;
    wf::signal::connection_t<wf::view_geometry_changed_signal> on_view_geometry_changed;
    wf::signal::connection_t<wf::view_fullscreen_signal> on_view_fullscreen;

  public:
    /** @param tool_bar_joined  The window matches the plugin's joined_views. */
    simple_decorator_t(wayfire_toplevel_view view, bool tool_bar_joined);
    ~simple_decorator_t();
    wf::decoration_margins_t get_margins(const wf::toplevel_state_t& state);
};
}

#endif /* end of include guard: DECO_SUBSURFACE_HPP */
