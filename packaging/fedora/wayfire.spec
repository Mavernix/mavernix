# The submodules pinned by Wayfire's v0.11.0 tag.
%global wf_json_commit  70039e13cdeaebd8ec498ed30bf5ab91c2e313ec
%global wf_utils_commit 329c3ff01724d82947f61c45332f75d3534e8454
%global wf_touch_commit d7ae5e7fa4de9536e2de2af74f168f981eb8a77a

Name:           wayfire
Version:        0.11.0
# Below 1, so a Fedora build of the same version replaces it.
Release:        0.1%{?dist}
Summary:        A modular Wayland compositor

License:        MIT
URL:            https://github.com/WayfireWM/wayfire
Source0:        %{url}/archive/v%{version}/%{name}-%{version}.tar.gz
Source1:        https://github.com/WayfireWM/wf-json/archive/%{wf_json_commit}/wf-json-%{wf_json_commit}.tar.gz
Source2:        https://github.com/WayfireWM/wf-utils/archive/%{wf_utils_commit}/wf-utils-%{wf_utils_commit}.tar.gz
Source3:        https://github.com/WayfireWM/wf-touch/archive/%{wf_touch_commit}/wf-touch-%{wf_touch_commit}.tar.gz

ExclusiveArch:  x86_64

BuildRequires:  gcc-c++
BuildRequires:  meson
BuildRequires:  cmake
BuildRequires:  doctest-devel
BuildRequires:  glm-devel
BuildRequires:  python3
BuildRequires:  pkgconfig(wf-config) = %{version}
BuildRequires:  pkgconfig(wlroots-0.20)
BuildRequires:  pkgconfig(wayland-server)
BuildRequires:  pkgconfig(wayland-client)
BuildRequires:  pkgconfig(wayland-cursor)
BuildRequires:  pkgconfig(wayland-protocols) >= 1.37
BuildRequires:  pkgconfig(cairo)
BuildRequires:  pkgconfig(pango)
BuildRequires:  pkgconfig(pangocairo)
BuildRequires:  pkgconfig(libdrm)
BuildRequires:  pkgconfig(egl)
BuildRequires:  pkgconfig(glesv2)
BuildRequires:  pkgconfig(libinput)
BuildRequires:  pkgconfig(pixman-1)
BuildRequires:  pkgconfig(xkbcommon)
BuildRequires:  pkgconfig(libudev)
BuildRequires:  pkgconfig(xcb)
BuildRequires:  pkgconfig(libjpeg)
BuildRequires:  pkgconfig(libpng)
BuildRequires:  pkgconfig(yyjson)
# Its tests start Xwayland.
BuildRequires:  xorg-x11-server-Xwayland

Requires:       wf-config%{?_isa} = %{version}
# X11 programs.
Recommends:     xorg-x11-server-Xwayland

%description
A modular Wayland compositor based on wlroots.

%package devel
Summary:        Development files for Wayfire plugins
Requires:       %{name}%{?_isa} = %{version}-%{release}
Requires:       glm-devel

%description devel
Headers, helper libraries and pkg-config metadata for Wayfire plugins.

%prep
%autosetup
for module in wf-json:%{SOURCE1} wf-utils:%{SOURCE2} wf-touch:%{SOURCE3}; do
    mkdir -p subprojects/${module%%:*}
    tar -xf ${module#*:} -C subprojects/${module%%:*} --strip-components=1
    cp subprojects/${module%%:*}/LICENSE LICENSE-${module%%:*}
done

%build
%meson -Duse_system_wfconfig=enabled -Duse_system_wlroots=enabled -Dxwayland=enabled -Dtests=enabled -Dbuild_locales=disabled
%meson_build

%install
%meson_install

%check
%meson_test

%files
%license LICENSE LICENSE-wf-json LICENSE-wf-utils LICENSE-wf-touch
%{_bindir}/wayfire
%{_libdir}/wayfire/
%{_libdir}/libwf-utils.so.*
%{_libdir}/libwayfire-blur-base.so
%{_datadir}/wayfire/
%{_datadir}/wayland-sessions/wayfire.desktop
%{_datadir}/xdg-desktop-portal/wayfire-portals.conf
%{_mandir}/man1/wayfire.1*

%files devel
%{_bindir}/wayfire-plugin
%{_includedir}/wayfire/
%{_libdir}/libwf-utils.so
%{_libdir}/*.a
%{_libdir}/pkgconfig/wayfire.pc
%{_libdir}/pkgconfig/wf-utils.pc
