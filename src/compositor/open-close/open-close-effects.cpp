// The plugin "themed-open-close": registers the effect "pop" with Wayfire's
// animate plugin, which runs it as a window opens (and backwards as one
// closes). animate's open_animation and close_animation options pick it.
//
// A popping window grows to its size from a little smaller while it fades
// in, as new windows have opened on OS X since 10.7. Wayfire's own "zoom"
// starts from a third of the size, which reads as a jump more than a pop.

#include <wayfire/plugin.hpp>
#include <wayfire/plugins/animate/animate.hpp>
#include <wayfire/plugins/common/shared-core-data.hpp>
#include <wayfire/view-transform.hpp>
#include <wayfire/view.hpp>

namespace themed_open_close
{
const std::string TRANSFORMER_NAME = "themed-open-close-pop";

// Where a window's growth starts, as a share of its size.
const double POP_START_SCALE = 0.85;

/** The effect's clock, with the two things it moves. */
class pop_progress_t : public wf::animation::duration_t
{
  public:
    using duration_t::duration_t;
    wf::animation::timed_transition_t scale{*this};
    wf::animation::timed_transition_t alpha{*this};
};

/** One run of "pop" on one window, in the form the animate plugin wants. */
class pop_animation_t : public wf::animate::animation_base_t
{
  public:
    void init(wayfire_view view, wf::animation_description_t duration,
        wf::animate::animation_type type) override
    {
        this->view = view;
        // The transitions are made anew for the new clock, not copied with it.
        progress = pop_progress_t(wf::create_option<wf::animation_description_t>(duration));
        progress.scale = wf::animation::timed_transition_t(progress, POP_START_SCALE, 1);
        progress.alpha = wf::animation::timed_transition_t(progress, 0, 1);
        if (type & WF_ANIMATE_HIDING_ANIMATION)
        {
            progress.scale.flip();
            progress.alpha.flip();
        }
        progress.start();

        // Scaled about the window's middle: the transformer's default.
        view->get_transformed_node()->add_transformer(std::make_shared<wf::scene::view_2d_transformer_t>(view),
            wf::TRANSFORMER_HIGHLEVEL, TRANSFORMER_NAME);
    }

    bool step() override
    {
        auto transformer = view->get_transformed_node()->get_transformer<wf::scene::view_2d_transformer_t>(
            TRANSFORMER_NAME);
        if (!transformer)
        {
            return false;
        }

        transformer->scale_x = float(progress.scale);
        transformer->scale_y = float(progress.scale);
        transformer->alpha = float(progress.alpha);
        return progress.running();
    }

    void reverse() override
    {
        progress.reverse();
    }

    ~pop_animation_t() override
    {
        if (view)
        {
            view->get_transformed_node()->rem_transformer(TRANSFORMER_NAME);
        }
    }

  private:
    wayfire_view view;
    pop_progress_t progress;
};

class plugin_t : public wf::plugin_interface_t
{
    wf::option_wrapper_t<wf::animation_description_t> pop_duration{"themed-open-close/pop_duration"};

    wf::shared_data::ref_ptr_t<wf::animate::animate_effects_registry_t> effects;

  public:
    void init() override
    {
        effects->register_effect("pop", wf::animate::effect_description_t{
            .generator = [] { return std::make_unique<pop_animation_t>(); },
            .default_duration = [=] { return pop_duration.value(); },
        });
    }

    void fini() override
    {
        effects->unregister_effect("pop");
    }
};
} // namespace themed_open_close

DECLARE_WAYFIRE_PLUGIN(themed_open_close::plugin_t);
