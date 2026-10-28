Name:           mavernix
Version:        0.1.5
Release:        1%{?dist}
Summary:        A lightweight Wayland desktop session with a classic look

License:        GPL-3.0-or-later AND MIT
URL:            https://github.com/Mavernix/mavernix
Source0:        %{url}/archive/refs/tags/v%{version}/%{name}-%{version}.tar.gz

ExclusiveArch:  x86_64

BuildRequires:  gcc-c++
BuildRequires:  meson >= 1.3.0
BuildRequires:  ninja-build
BuildRequires:  pkgconf-pkg-config
BuildRequires:  python3-devel
BuildRequires:  python3-pillow
BuildRequires:  glm-devel
BuildRequires:  vulkan-headers
BuildRequires:  pkgconfig(wayfire) = 0.11.0
BuildRequires:  pkgconfig(tomlplusplus)
BuildRequires:  pkgconfig(gio-unix-2.0)
BuildRequires:  pkgconfig(librsvg-2.0)
BuildRequires:  qt6-qtbase-devel
BuildRequires:  qt6-qtbase-private-devel
BuildRequires:  qt6-qtdeclarative-devel

# The compositor plugins must match the Wayfire version they build against.
Requires:       wayfire%{?_isa} = 0.11.0
Requires:       quickshell >= 0.3.2
Requires:       qt6-qtdeclarative
Requires:       qt6-qtmultimedia
Requires:       qt6-qtwayland
Requires:       qt6-qtsvg
Requires:       python3 >= 3.11
Requires:       python3-pillow
Requires:       kvantum
Requires:       gsettings-desktop-schemas
Requires:       gvfs
Requires:       adwaita-icon-theme
Requires:       adwaita-icon-theme-legacy
Requires:       adwaita-cursor-theme
Requires:       dejavu-sans-fonts
Requires:       dejavu-sans-mono-fonts
Requires:       7zip
Requires:       cpio
Requires:       bzip2
Requires:       lbzip2
Requires:       libicns-utils
Requires:       ffmpeg-free
Requires:       poppler-utils
Requires:       bubblewrap

Recommends:     foot
Recommends:     pipewire
Recommends:     wireplumber
Recommends:     NetworkManager
Recommends:     network-manager-applet
Recommends:     upower
Recommends:     brightnessctl
Recommends:     playerctl
Recommends:     dex-autostart
Recommends:     hwdata
Recommends:     xdg-desktop-portal-gtk
Recommends:     xdg-desktop-portal-wlr

%description
Mavernix is a Wayland desktop session built on Wayfire and Quickshell.
It provides a menu bar, dock, compositor plugins, settings and a theme
installer. Proprietary artwork is not included.

%prep
%autosetup
cp src/compositor/decoration/LICENSE-wayfire LICENSE-decoration
cp src/compositor/minimize/LICENSE-wayfire LICENSE-minimize

%build
%meson
%meson_build

%install
%meson_install
%py3_shebang_fix %{buildroot}%{_libdir}/mavernix/session/compositor_call.py

%check
QT_QPA_PLATFORM=offscreen %meson_test

%files
%license LICENSE
%license LICENSE-decoration LICENSE-minimize
%doc README.md CHANGELOG.md
%{_libdir}/mavernix/
%{_datadir}/mavernix/
%{_datadir}/wayland-sessions/mavernix.desktop
%{_datadir}/icons/hicolor/*/apps/mavernix.svg
