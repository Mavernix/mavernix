Name:           quickshell
Version:        0.3.2
Release:        1%{?dist}
Summary:        Flexible QtQuick based desktop shell toolkit
License:        LGPL-3.0-only AND BSD-3-Clause AND HPND-sell-variant
URL:            https://quickshell.org
Source0:        https://github.com/quickshell-mirror/quickshell/archive/refs/tags/v%{version}/%{name}-%{version}.tar.gz
ExclusiveArch:  x86_64

BuildRequires:  gcc-c++
BuildRequires:  gdb
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
notifications and authentication integrations. The optional built-in crash
reporter is disabled because cpptrace is unavailable in Fedora 44.

%prep
%autosetup

%build
# Fedora 44 has no cpptrace package; disable only the optional crash reporter.
%cmake -GNinja -DCRASH_HANDLER=OFF -DBUILD_TESTING=ON -DDISTRIBUTOR="Mavernix Fedora RPM" -DINSTALL_QMLDIR=%{_libdir}/qt6/qml -DGIT_REVISION=4f508be500dea6e5732cc3d50382a0048b17e7b1 -DFETCHCONTENT_FULLY_DISCONNECTED=ON
%cmake_build

%install
%cmake_install

%check
export QT_QPA_PLATFORM=offscreen
export QT_QUICK_BACKEND=software
export QT_LOGGING_TO_CONSOLE=1
export QT_DEBUG_PLUGINS=1
export QS_NO_FILTER_QT_LOGS=1
if ! %ctest --verbose --output-on-failure; then
    # Capture one representative startup failure without suppressing test errors.
    find %{_vpath_builddir} -type f -name ringbuffer -executable -exec gdb --batch -ex 'set pagination off' -ex run -ex 'thread apply all bt' --args {} \; || :
    exit 1
fi

%files
%license LICENSE LICENSE-GPL
%doc README.md BUILD.md
%{_bindir}/quickshell
%{_bindir}/qs
%{_libdir}/qt6/qml/Quickshell/
%{_datadir}/applications/org.quickshell.desktop
%{_datadir}/icons/hicolor/scalable/apps/org.quickshell.svg
