Name:           wf-config
Version:        0.11.0
Release:        1%{?dist}
Summary:        Configuration library for Wayfire

License:        MIT
URL:            https://github.com/WayfireWM/wf-config
Source0:        %{url}/archive/refs/tags/v%{version}/%{name}-%{version}.tar.gz

BuildRewuires:  cmake
BuildRequires:  gcc-c++
BuildRequires:  meson
BuildRequires:  ninja-build
BuildRequires:  pkgconf-pkg-config
BuildRequires:  glm-devel
BuildRequires:  pkgconfig(libevdev)
BuildRequires:  pkgconfig(libxml-2.0)
BuildRequires:  doctest-devel

%description
A configuration library for the Wayfire compositor.

%package devel
Summary:        Development files for wf-config
Requires:       %{name}%{?_isa} = %{version}-%{release}
Requires:       glm-devel

%description devel
Headers and pkg-config metadata for building against wf-config.

%prep
%autosetup

%build
%meson -Dtests=enabled
%meson_build

%install
%meson_install

%check
%meson_test

%files
%license LICENSE
%{_libdir}/libwf-config.so.1*

%files devel
%{_includedir}/wayfire/
%{_libdir}/libwf-config.so
%{_libdir}/pkgconfig/wf-config.pc
