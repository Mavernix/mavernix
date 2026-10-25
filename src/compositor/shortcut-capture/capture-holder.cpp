#include "capture-holder.hpp"

namespace shortcut_capture
{
holder_t::holder_t(switch_t set_keys_enabled) : set_keys_enabled(std::move(set_keys_enabled)) {}

bool holder_t::start(who_t who)
{
    if (holder == who && holder != nullptr)
    {
        return true;
    }

    if (holder != nullptr)
    {
        return false;
    }

    holder = who;
    set_keys_enabled(false);
    return true;
}

bool holder_t::stop(who_t who)
{
    if (holder == nullptr || holder != who)
    {
        return false;
    }

    release();
    return true;
}

void holder_t::connection_closed(who_t who)
{
    if (holder != nullptr && holder == who)
    {
        release();
    }
}

void holder_t::release()
{
    if (holder == nullptr)
    {
        return;
    }

    holder = nullptr;
    set_keys_enabled(true);
}

bool holder_t::is_recording() const
{
    return holder != nullptr;
}
} // namespace shortcut_capture
