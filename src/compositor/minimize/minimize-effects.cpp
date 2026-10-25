// The plugin "themed-minimize": registers the minimise effects "genie" and
// "scale" with Wayfire's animate plugin, which runs them (and reverses them
// for restoring). animate's `minimize_animation` option picks one.

#include <wayfire/plugin.hpp>
#include <wayfire/plugins/animate/animate.hpp>
#include <wayfire/plugins/common/shared-core-data.hpp>
#include <wayfire/toplevel-view.hpp>

#include "warp-transformer.hpp"

namespace themed_minimize
{
const std::string TRANSFORMER_NAME = "themed-minimize-warp";

/** One run of an effect on one window, in the form the animate plugin wants. */
class warp_animation_t : public wf::animate::animation_base_t
{
  public:
    explicit warp_animation_t(effect_t effect) : effect(effect)
    {}

    void init(wayfire_view view, wf::animation_description_t duration,
        wf::animate::animation_type type) override
    {
        this->view = view;
        remove_warp();

        // The dock sets the icon's place through foreign-toplevel's
        // "set rectangle"; Wayfire keeps it as the minimize hint.
        wf::geometry_t icon{0, 0, 0, 0};
        if (auto toplevel = wf::toplevel_cast(view))
        {
            icon = wf::from_integer_box(toplevel->get_minimize_hint());
        }

        auto warp = std::make_shared<warp_transformer_t>(view, effect, duration, icon);
        view->get_transformed_node()->add_transformer(warp, wf::TRANSFORMER_HIGHLEVEL + 1,
            TRANSFORMER_NAME);
        warp->start(type & WF_ANIMATE_HIDING_ANIMATION);
    }

    bool step() override
    {
        auto warp = current_warp();
        return warp && warp->is_running();
    }

    void reverse() override
    {
        if (auto warp = current_warp())
        {
            warp->reverse();
        }
    }

    ~warp_animation_t() override
    {
        remove_warp();
    }

  private:
    std::shared_ptr<warp_transformer_t> current_warp()
    {
        if (!view)
        {
            return nullptr;
        }
        return view->get_transformed_node()->get_transformer<warp_transformer_t>(TRANSFORMER_NAME);
    }

    void remove_warp()
    {
        if (view)
        {
            view->get_transformed_node()->rem_transformer(TRANSFORMER_NAME);
        }
    }

    effect_t effect;
    wayfire_view view;
};

class plugin_t : public wf::plugin_interface_t
{
    wf::option_wrapper_t<wf::animation_description_t> genie_duration{"themed-minimize/genie_duration"};
    wf::option_wrapper_t<wf::animation_description_t> scale_duration{"themed-minimize/scale_duration"};

    wf::shared_data::ref_ptr_t<wf::animate::animate_effects_registry_t> effects;

  public:
    void init() override
    {
        effects->register_effect("genie", wf::animate::effect_description_t{
            .generator = [] { return std::make_unique<warp_animation_t>(effect_t::GENIE); },
            .default_duration = [=] { return genie_duration.value(); },
        });
        effects->register_effect("scale", wf::animate::effect_description_t{
            .generator = [] { return std::make_unique<warp_animation_t>(effect_t::SCALE); },
            .default_duration = [=] { return scale_duration.value(); },
        });
    }

    void fini() override
    {
        effects->unregister_effect("genie");
        effects->unregister_effect("scale");
    }
};
}

DECLARE_WAYFIRE_PLUGIN(themed_minimize::plugin_t);
