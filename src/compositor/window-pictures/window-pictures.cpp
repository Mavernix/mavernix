// The plugin "window-pictures": writes a small picture of one window (frame
// included, shadow left out) for the dock's miniatures. Clients can only
// capture whole outputs, and a minimised window is on none.
//
// IPC method "window-pictures/write": id, path (".png", or ".rgba" for plain
// pixels), largest_side; answers with the picture's "width" and "height".
// A ".rgba" file is the line "WINDOWPICTURE width height" followed by RGBA
// rows; it takes milliseconds where a PNG of a large window stalls the
// compositor for about 100 ms.
//
// (wayfire-plugins-extra's "view-shot" writes full size and is not packaged
// on Arch.)

#include <fstream>

#include <drm_fourcc.h>

#include <wayfire/img.hpp>
#include <wayfire/plugin.hpp>
#include <wayfire/plugins/common/shared-core-data.hpp>
#include <wayfire/plugins/ipc/ipc-helpers.hpp>
#include <wayfire/plugins/ipc/ipc-method-repository.hpp>
#include <wayfire/render.hpp>
#include <wayfire/toplevel-view.hpp>
#include <wayfire/view.hpp>

#include "shrink.hpp"

namespace window_pictures
{
const std::string WRITE_METHOD = "window-pictures/write";
const std::string PNG_FILE_ENDING = ".png";
const std::string PLAIN_FILE_ENDING = ".rgba";
const std::string PLAIN_FILE_WORD = "WINDOWPICTURE";

static bool ends_with(const std::string& text, const std::string& ending)
{
    return (text.size() > ending.size()) && (text.compare(text.size() - ending.size(), std::string::npos, ending) == 0);
}

/** False if the file could not be written. */
static bool write_plain(const std::string& path, const picture_t& picture)
{
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file << PLAIN_FILE_WORD << " " << picture.width << " " << picture.height << "\n";
    file.write((const char*)picture.pixels.data(), (std::streamsize)picture.pixels.size());
    return file.good();
}

/** The window's contents at its output's density; empty if it has none. */
static picture_t contents_of(wayfire_view view)
{
    picture_t contents;

    wf::auxilliary_buffer_t snapshot;
    view->take_snapshot(snapshot);
    const auto size = snapshot.get_size();
    if ((size.width <= 0) || (size.height <= 0))
    {
        return contents;
    }

    contents.width = size.width;
    contents.height = size.height;
    contents.pixels.resize(size_t(size.width) * size.height * 4);

    // Bytes R, G, B, A: DRM names formats from the least significant byte.
    wlr_texture_read_pixels_options options{};
    options.data = contents.pixels.data();
    options.format = DRM_FORMAT_ABGR8888;
    options.stride = uint32_t(size.width) * 4;
    if (!wlr_texture_read_pixels(snapshot.get_texture(), &options))
    {
        return picture_t{};
    }

    return contents;
}

class plugin_t : public wf::plugin_interface_t
{
    wf::shared_data::ref_ptr_t<wf::ipc::method_repository_t> methods;

    wf::ipc::method_callback write_picture = [](const wf::json_t& data) -> wf::json_t {
        auto view = wf::ipc::json_find_view_or_throw(data);
        const std::string path = wf::ipc::json_get_string(data, "path");
        const int largest_side = int(wf::ipc::json_get_int64(data, "largest_side"));

        const bool as_plain_pixels = ends_with(path, PLAIN_FILE_ENDING);
        if ((!as_plain_pixels && !ends_with(path, PNG_FILE_ENDING)) || (largest_side <= 0))
        {
            return wf::ipc::json_error(
                "window-pictures/write needs a path ending in .png or .rgba and a largest_side above 0");
        }

        const picture_t contents = contents_of(view);
        if (contents.pixels.empty())
        {
            return wf::ipc::json_error("the window has no contents to take a picture of");
        }

        // Cut away the shadow. The rectangles are in points, the contents in
        // pixels.
        const wf::geometry_t everything = view->get_surface_root_node()->get_bounding_box();
        wf::geometry_t window = everything;
        if (auto toplevel = wf::toplevel_cast(view))
        {
            window = toplevel->get_geometry();
        }

        const double pixels_per_point = contents.width / std::max(1.0, double(everything.width));
        const int left = std::clamp(int((window.x - everything.x) * pixels_per_point), 0, contents.width - 1);
        const int top = std::clamp(int((window.y - everything.y) * pixels_per_point), 0, contents.height - 1);
        const int width = std::min(int(window.width * pixels_per_point), contents.width - left);
        const int height = std::min(int(window.height * pixels_per_point), contents.height - top);

        picture_t small = shrink_part(contents, left, top, width, height, largest_side);
        if (small.pixels.empty())
        {
            return wf::ipc::json_error("the window is empty");
        }

        if (as_plain_pixels)
        {
            if (!write_plain(path, small))
            {
                return wf::ipc::json_error("the picture could not be written to " + path);
            }
        }
        else
        {
            image_io::write_to_file(path, small.pixels.data(), small.width, small.height, "png");
        }

        auto answer = wf::ipc::json_ok();
        answer["width"] = small.width;
        answer["height"] = small.height;
        return answer;
    };

  public:
    void init() override { methods->register_method(WRITE_METHOD, write_picture); }

    void fini() override { methods->unregister_method(WRITE_METHOD); }
};
} // namespace window_pictures

DECLARE_WAYFIRE_PLUGIN(window_pictures::plugin_t);
