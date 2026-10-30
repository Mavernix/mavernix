# The compositor plugins load only into the Wayfire version they were built
# against (docs/building.md).
%global wayfire_version 0.11.0

Name:           mavernix
Version:        0.5.0
Release:        1%{?dist}
Summary:        A lightweight Wayland desktop session with a classic look

License:        GPL-3.0-or-later AND MIT
URL:            https://github.com/Mavernix/mavernix
Source0:        %{url}/archive/v%{version}/%{name}-%{version}.tar.gz

ExclusiveArch:  x86_64

BuildRequires:  gcc-c++
BuildRequires:  meson >= 1.3.0
BuildRequires:  python3
BuildRequires:  python3-pillow
BuildRequires:  glm-devel
BuildRequires:  vulkan-headers
BuildRequires:  pkgconfig(wayfire) = %{wayfire_version}
BuildRequires:  pkgconfig(tomlplusplus)
BuildRequires:  pkgconfig(gio-unix-2.0)
BuildRequires:  pkgconfig(librsvg-2.0)
BuildRequires:  qt6-qtbase-devel
BuildRequires:  qt6-qtbase-private-devel
BuildRequires:  qt6-qtdeclarative-devel

Requires:       wayfire%{?_isa} = %{wayfire_version}
Requires:       quickshell >= 0.3.2
Requires:       qt6-qtdeclarative
Requires:       qt6-qtmultimedia
Requires:       qt6-qtwayland
Requires:       qt6-qtsvg
Requires:       python3
Requires:       python3-pillow
Requires:       kvantum
Requires:       gsettings-desktop-schemas
Requires:       gvfs
Requires:       adwaita-icon-theme
Requires:       adwaita-icon-theme-legacy
Requires:       adwaita-cursor-theme
Requires:       dejavu-sans-fonts
# The Theme Installer: unpacking, converting icons, sounds and drawings, and
# its sandbox.
Requires:       7zip
Requires:       cpio
Requires:       bzip2
Requires:       lbzip2
Requires:       libicns-utils
Requires:       ffmpeg-free
Requires:       poppler-utils
Requires:       bubblewrap

# The same optional programs as packaging/arch/PKGBUILD.
Recommends:     foot
Recommends:     pipewire
Recommends:     wireplumber
Recommends:     NetworkManager
Recommends:     network-manager-applet
Recommends:     bluez
Recommends:     upower
Recommends:     grim
Recommends:     slurp
Recommends:     wl-clipboard
Recommends:     brightnessctl
Recommends:     playerctl
Recommends:     dex-autostart
Recommends:     hwdata
Recommends:     xdg-desktop-portal-gtk
Recommends:     xdg-desktop-portal-wlr

%description
Mavernix is a Wayland desktop session built on Wayfire and Quickshell: a menu
bar, a dock, compositor plugins, a Settings window and a theme installer.
No proprietary artwork is included.

%prep
%autosetup
cp src/compositor/decoration/LICENSE-wayfire LICENSE-wayfire

%build
%meson
%meson_build

%install
%meson_install

%check
%meson_test

%files
%license LICENSE LICENSE-wayfire
%doc README.md CHANGELOG.md
%{_libdir}/mavernix/
%{_datadir}/mavernix/
%{_datadir}/wayland-sessions/mavernix.desktop
%{_datadir}/icons/hicolor/*/apps/mavernix.svg
