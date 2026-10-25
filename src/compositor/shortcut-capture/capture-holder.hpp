// Who is recording a shortcut, and the switch that keeps the compositor's
// keys off meanwhile. Its one job: never leave the keys switched off.

#pragma once

#include <functional>

namespace shortcut_capture
{
class holder_t
{
  public:
    // Called with false when recording starts and true when it ends; the two
    // always alternate.
    using switch_t = std::function<void(bool)>;

    explicit holder_t(switch_t set_keys_enabled);

    // The client's IPC connection; only compared, never dereferenced.
    using who_t = const void*;

    // False if somebody else is recording; true again for the holder.
    bool start(who_t who);

    bool stop(who_t who);

    void connection_closed(who_t who);

    // Ends recording whoever holds it.
    void release();

    bool is_recording() const;

  private:
    switch_t set_keys_enabled;
    who_t holder = nullptr;
};
} // namespace shortcut_capture
