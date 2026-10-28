# Initial Fedora build; sources are assembled by fedora-rpm.yml with pinned submodules.
Name:           wayfire
Version:        0.11.0
Release:        1%{?dist}
Summary:        A modular Wayland compositor
License:        MIT
URL:            https://github.com/WayfireWM/wayfire
Source0:        wayfire-%{version}.tar.gz
ExclusiveArch:  x86_64

BuildRequires:  gcc-c++
BuildRequires:  meson
BuildRequires:  ninja-build
BuildRequires:  cmake
BuildRequires:  pkgconf-pkg-config
BuildRequires:  doctest-devel
BuildRequires:  glm-devel
BuildRequires:  python3-devel
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
BuildRequires:  pkgconfig(wlroots-0.20)
BuildRequires:  pkgconfig(wf-config) = 0.11.0
Requires:       wf-config%{?_isa} = 0.11.0
BuildRequires:  xorg-x11-server-Xwayland

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
cp subprojects/wf-utils/LICENSE LICENSE-wf-utils
%build
%meson -Duse_system_wfconfig=enabled -Duse_system_wlroots=enabled -Dtests=enabled -Dbuild_locales=disabled -Dxwayland=enabled
%meson_build

%install
%meson_install
%py3_shebang_fix %{buildroot}%{_bindir}/wayfire-plugin

%check
%meson_test

%files
%license LICENSE
%license LICENSE-wf-utils
%{_bindir}/wayfire
%{_libdir}/wayfire/
%{_libdir}/libwf-utils.so.*
%{_datadir}/wayfire/
%{_datadir}/wayland-sessions/wayfire.desktop
%{_datadir}/xdg-desktop-portal/wayfire-portals.conf
%{_mandir}/man1/wayfire.1*
%{_libdir}/libwayfire-blur-base.so

%files devel
%{_bindir}/wayfire-plugin
%{_includedir}/wayfire/
%{_libdir}/pkgconfig/wayfire.pc
%{_libdir}/pkgconfig/wf-utils.pc
%{_libdir}/libwf-utils.so
%{_libdir}/*.a
