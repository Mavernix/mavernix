// The plugin "closed-programs": writes one line to the journal when a
// program that had a window loses its connection to the compositor, so a
// program that "just closed" can be looked up afterwards:
//
//   journalctl --user -t mavernix-compositor
//
// The line names the program and its windows' app id, says how long it was
// connected and how many of its windows were still open, and who ended the
// connection: the program (it quit, crashed or was killed) or the compositor
// (it cut the program off). When the compositor sent the program a protocol
// error first, the line has it; libwayland says nothing of its own then.
//
// Programs that never showed a window, such as wl-copy, are left out, and at
// most LINES_A_MINUTE lines are written a minute, so this stays small.

#include <wayfire/core.hpp>
#include <wayfire/plugin.hpp>
#include <wayfire/signal-definitions.hpp>
#include <wayfire/view.hpp>

#include <wayland-server-core.h>
#include <wayland-server-protocol.h>

#include <poll.h>
#include <syslog.h>

#include <chrono>
#include <fstream>
#include <map>
#include <memory>
#include <string>

namespace closed_programs
{
const int LINES_A_MINUTE = 30;

using clock = std::chrono::steady_clock;

class plugin_t;

// A wl_listener that knows its plugin; plain, so a pointer to its first
// member is a pointer to it.
struct hook_t
{
    wl_listener listener;
    plugin_t* plugin;
};

// One connected program.
struct program_t
{
    hook_t on_destroy;
    pid_t pid = 0;
    std::string name;   // from /proc/PID/comm, as it was when it connected
    std::string app_id; // of the last window it showed; empty: none yet
    std::string error;  // the protocol error the compositor sent it
    clock::time_point connected = clock::now();
};

std::string process_name(pid_t pid)
{
    std::ifstream file("/proc/" + std::to_string(pid) + "/comm");
    std::string name;
    std::getline(file, name);
    return name.empty() ? "a program" : name;
}

std::string how_long(clock::duration taken)
{
    const long seconds = std::chrono::duration_cast<std::chrono::seconds>(taken).count();
    if (seconds < 60)
    {
        return std::to_string(seconds) + " s";
    }

    if (seconds < 3600)
    {
        return std::to_string(seconds / 60) + " min";
    }

    return std::to_string(seconds / 3600) + " h " + std::to_string(seconds / 60 % 60) + " min";
}

// Whether the program's end of the connection is closed. When the compositor
// cuts a program off, the program's end is still open.
bool program_hung_up(wl_client* client)
{
    pollfd connection = {wl_client_get_fd(client), POLLRDHUP, 0};
    return (poll(&connection, 1, 0) > 0) && (connection.revents & (POLLRDHUP | POLLHUP | POLLERR));
}

class plugin_t : public wf::plugin_interface_t
{
    std::map<wl_client*, std::unique_ptr<program_t>> programs;
    hook_t on_client_created = {{}, this};
    wl_protocol_logger* error_listener = nullptr;

    clock::time_point minute_began = clock::now();
    int lines_this_minute = 0;
    int left_out = 0;

    void add(wl_client* client)
    {
        auto program = std::make_unique<program_t>();
        program->on_destroy.plugin = this;
        program->on_destroy.listener.notify = [](wl_listener* listener, void* data) {
            reinterpret_cast<hook_t*>(listener)->plugin->closed(static_cast<wl_client*>(data));
        };
        wl_client_get_credentials(client, &program->pid, nullptr, nullptr);
        program->name = process_name(program->pid);
        wl_client_add_destroy_listener(client, &program->on_destroy.listener);
        programs[client] = std::move(program);
    }

    void closed(wl_client* client)
    {
        auto found = programs.find(client);
        if (found == programs.end())
        {
            return;
        }

        const program_t& program = *found->second;
        if (!program.app_id.empty() || !program.error.empty())
        {
            int windows = 0;
            for (auto& view : wf::get_core().get_all_views())
            {
                windows += (view->get_client() == client) && view->is_mapped();
            }

            const bool by_program = program_hung_up(client);
            std::string line = program.name + " (pid " + std::to_string(program.pid);
            line += program.app_id.empty() ? "" : ", " + program.app_id;
            line += by_program ? ") closed its connection" : ") was cut off by the compositor";
            line += " after " + how_long(clock::now() - program.connected);
            line += ", with " + std::to_string(windows) + (windows == 1 ? " window" : " windows") + " open";
            line += program.error.empty() ? "" : "; it was sent the error: " + program.error;
            write((by_program && program.error.empty()) ? LOG_INFO : LOG_WARNING, line);
        }

        wl_list_remove(&found->second->on_destroy.listener.link);
        programs.erase(found);
    }

    void write(int priority, const std::string& line)
    {
        if (clock::now() - minute_began >= std::chrono::minutes(1))
        {
            if (left_out > 0)
            {
                syslog(LOG_INFO, "%d more closed programs were left out", left_out);
            }

            minute_began = clock::now();
            lines_this_minute = 0;
            left_out = 0;
        }

        if (lines_this_minute >= LINES_A_MINUTE)
        {
            left_out++;
            return;
        }

        lines_this_minute++;
        syslog(priority, "%s", line.c_str());
    }

    // Every message between the compositor and its programs passes here;
    // all but wl_display.error, the protocol error, leave at once.
    static void on_message(void* data, wl_protocol_logger_type direction, const wl_protocol_logger_message* message)
    {
        if ((direction != WL_PROTOCOL_LOGGER_EVENT) ||
            (message->message != &wl_display_interface.events[WL_DISPLAY_ERROR]) || (message->arguments_count < 3))
        {
            return;
        }

        auto* self = static_cast<plugin_t*>(data);
        auto found = self->programs.find(wl_resource_get_client(message->resource));
        if (found == self->programs.end())
        {
            return;
        }

        // The arguments: the object at fault, the error's number, its text.
        auto* object = reinterpret_cast<wl_resource*>(message->arguments[0].o);
        std::string error =
            object ? std::string(wl_resource_get_class(object)) + "#" + std::to_string(wl_resource_get_id(object)) + " "
                   : std::string();
        error += "error " + std::to_string(message->arguments[1].u);
        if (message->arguments[2].s)
        {
            error += ": " + std::string(message->arguments[2].s);
        }

        found->second->error = error;
    }

    wf::signal::connection_t<wf::view_mapped_signal> on_mapped = [this](wf::view_mapped_signal* event) {
        auto found = programs.find(event->view->get_client());
        if (found != programs.end())
        {
            std::string app_id = event->view->get_app_id();
            found->second->app_id = app_id.empty() ? "no app id" : app_id;
        }
    };

  public:
    void init() override
    {
        openlog("mavernix-compositor", 0, LOG_USER);

        wl_display* display = wf::get_core().display;
        wl_client* client;
        wl_client_for_each(client, wl_display_get_client_list(display))
        {
            add(client);
        }

        on_client_created.listener.notify = [](wl_listener* listener, void* data) {
            reinterpret_cast<hook_t*>(listener)->plugin->add(static_cast<wl_client*>(data));
        };
        wl_display_add_client_created_listener(display, &on_client_created.listener);
        error_listener = wl_display_add_protocol_logger(display, on_message, this);
        wf::get_core().connect(&on_mapped);
    }

    void fini() override
    {
        on_mapped.disconnect();
        wl_protocol_logger_destroy(error_listener);
        wl_list_remove(&on_client_created.listener.link);
        for (auto& [client, program] : programs)
        {
            wl_list_remove(&program->on_destroy.listener.link);
        }

        programs.clear();
        closelog();
    }
};
} // namespace closed_programs

DECLARE_WAYFIRE_PLUGIN(closed_programs::plugin_t);
