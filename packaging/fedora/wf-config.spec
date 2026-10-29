Name:           wf-config
Version:        0.11.0
# Below 1, so a Fedora build of the same version replaces it.
Release:        0.1%{?dist}
Summary:        Configuration library for Wayfire

License:        MIT
URL:            https://github.com/WayfireWM/wf-config
Source0:        %{url}/archive/v%{version}/%{name}-%{version}.tar.gz
Patch0:         wf-config-lock-test.patch

BuildRequires:  gcc-c++
BuildRequires:  meson
BuildRequires:  cmake
BuildRequires:  glm-devel
BuildRequires:  doctest-devel
BuildRequires:  pkgconfig(libevdev)
BuildRequires:  pkgconfig(libxml-2.0)

%description
A configuration library for the Wayfire compositor.

%package devel
Summary:        Development files for wf-config
Requires:       %{name}%{?_isa} = %{version}-%{release}
Requires:       glm-devel

%description devel
Headers and pkg-config metadata for building against wf-config.

%prep
%autosetup -p1

%build
%meson -Dtests=enabled
%meson_build

%install
%meson_install

%check
%meson_test

%files
%license LICENSE
%{_libdir}/libwf-config.so.*

%files devel
%{_includedir}/wayfire/
%{_libdir}/libwf-config.so
%{_libdir}/pkgconfig/wf-config.pc
