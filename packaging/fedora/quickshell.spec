Name:           quickshell
Version:        0.3.2
Release:        1%{?dist}
Summary:        Flexible QtQuick based desktop shell toolkit
License:        LGPL-3.0-only AND BSD-3-Clause AND HPND-sell-variant
URL:            https://quickshell.org
Source0:        https://github.com/quickshell-mirror/quickshell/archive/refs/tags/v%{version}/%{name}-%{version}.tar.gz
ExclusiveArch:  x86_64

BuildRequires:  gcc-c++
BuildRequires:  cmake
BuildRequires:  ninja-build
BuildRequires:  pkgconf-pkg-config
BuildRequires:  qt6-qtbase-devel
BuildRequires:  qt6-qtbase-private-devel
BuildRequires:  qt6-qtdeclarative-devel
BuildRequires:  qt6-qtwayland-devel
BuildRequires:  qt6-qtshadertools-devel
BuildRequires:  spirv-tools
BuildRequires:  cmake(CLI11)
BuildRequires:  cmake(cpptrace)
BuildRequires:  cmake(VulkanHeaders)
BuildRequires:  pkgconfig(jemalloc)
BuildRequires:  pkgconfig(libdrm)
BuildRequires:  pkgconfig(gbm)
BuildRequires:  pkgconfig(egl)
BuildRequires:  pkgconfig(wayland-client)
BuildRequires:  pkgconfig(wayland-protocols) >= 1.41
BuildRequires:  wayland-devel
BuildRequires:  pkgconfig(xcb)
BuildRequires:  pkgconfig(libpipewire-0.3)
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(gobject-2.0)
BuildRequires:  pkgconfig(polkit-agent-1)
BuildRequires:  pkgconfig(polkit-gobject-1)
BuildRequires:  pam-devel
Requires:       qt6-qtsvg
Requires:       qt6-qtwayland

%description
Quickshell is a toolkit for building desktop shells using Qt Quick.
This package enables Wayland, system tray, audio, networking, Bluetooth,
notifications and authentication integrations.

%prep
%autosetup

%build
%cmake -GNinja -DBUILD_TESTING=ON -DDISTRIBUTOR="Mavernix Fedora RPM" -DINSTALL_QMLDIR=%{_libdir}/qt6/qml -DGIT_REVISION=4f508be500dea6e5732cc3d50382a0048b17e7b1 -DFETCHCONTENT_FULLY_DISCONNECTED=ON
%cmake_build

%install
%cmake_install

%check
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software %ctest --output-on-failure

%files
%license LICENSE LICENSE-GPL
%doc README.md BUILD.md
%{_bindir}/quickshell
%{_bindir}/qs
%{_libdir}/qt6/qml/Quickshell/
%{_datadir}/applications/org.quickshell.desktop
%{_datadir}/icons/hicolor/scalable/apps/org.quickshell.svg
