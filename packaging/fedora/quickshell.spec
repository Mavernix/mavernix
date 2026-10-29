# The commit Quickshell's v0.3.2 tag points to, for `qs --version`.
%global commit 4f508be500dea6e5732cc3d50382a0048b17e7b1

Name:           quickshell
Version:        0.3.2
# Below 1, so a Fedora build of the same version replaces it.
Release:        0.1%{?dist}
Summary:        Flexible QtQuick based desktop shell toolkit

License:        LGPL-3.0-only AND BSD-3-Clause AND HPND-sell-variant
URL:            https://quickshell.org
Source0:        https://github.com/quickshell-mirror/quickshell/archive/v%{version}/%{name}-%{version}.tar.gz

ExclusiveArch:  x86_64

BuildRequires:  gcc-c++
BuildRequires:  cmake
BuildRequires:  ninja-build
BuildRequires:  qt6-qtbase-devel
BuildRequires:  qt6-qtbase-private-devel
BuildRequires:  qt6-qtdeclarative-devel
BuildRequires:  qt6-qtwayland-devel
BuildRequires:  qt6-qtshadertools-devel
BuildRequires:  spirv-tools
BuildRequires:  cmake(CLI11)
BuildRequires:  cmake(VulkanHeaders)
BuildRequires:  wayland-devel
BuildRequires:  pkgconfig(wayland-client)
BuildRequires:  pkgconfig(wayland-protocols) >= 1.41
BuildRequires:  pkgconfig(jemalloc)
BuildRequires:  pkgconfig(libdrm)
BuildRequires:  pkgconfig(gbm)
BuildRequires:  pkgconfig(egl)
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
Quickshell is a toolkit for building desktop shells using Qt Quick. This
build leaves out the crash reporter, which needs cpptrace, not packaged by
Fedora.

%prep
%autosetup

%build
%cmake -GNinja -DCRASH_HANDLER=OFF -DBUILD_TESTING=ON -DDISTRIBUTOR="Mavernix (Fedora)" -DGIT_REVISION=%{commit} -DINSTALL_QMLDIR=%{_qt6_qmldir}
%cmake_build

%install
%cmake_install

%check
export QT_QPA_PLATFORM=offscreen
# popupwindow checks where a popup lands, which Qt's offscreen backend gets
# wrong on some machines (12 pixels in, not 20).
%ctest --exclude-regex '^popupwindow$'

%files
%license LICENSE LICENSE-GPL
%doc README.md
%{_bindir}/quickshell
%{_bindir}/qs
%{_qt6_qmldir}/Quickshell/
%{_datadir}/applications/org.quickshell.desktop
%{_datadir}/icons/hicolor/scalable/apps/org.quickshell.svg
