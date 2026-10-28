Name:           wf-config
Version:        0.11.0
Release:        1%{?dist}
Summary:        Configuration library for Wayfire

License:        MIT
URL:            https://github.com/WayfireWM/wf-config
Source0:        %{url}/archive/refs/tags/v%{version}/%{name}-%{version}.tar.gz

BuildRequires:  cmake
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
# Stop the forked lock-test child before it executes subsequent tests.
python3 - <<'PY'
from pathlib import Path

path = Path("test/file_test.cpp")
source = path.read_text()
old = """        flock(fd, LOCK_UN);
        close(fd);
    }

    /* Wait for other process to lock the file */"""
new = """        flock(fd, LOCK_UN);
        close(fd);
        _exit(0);
    }

    /* Wait for other process to lock the file */"""
assert source.count(old) == 1, "Lock-test source changed; review patch"
path.write_text(source.replace(old, new))
PY


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
