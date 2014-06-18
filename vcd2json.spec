%define python_sitearch %(%{__python} -c "from distutils.sysconfig import get_python_lib; print get_python_lib(1)")

Summary: Library to extract information from Value Change Dump files
Name: vcd2json
Version: 0.1.1
Release: 0
License: BSD
Group: Development/Libraries
URL: http://fortylines.com/
Source: http://fortylines.com/resources/srcs/vcd2json-%{version}.tar.bz2
Packager: Sebastien Mirolo <smirolo@fortylines.com>

%description
Library to extract information from Value Change Dump files

%package devel
Summary: Development files for the VCD libraries
Group: Development/Libraries
Requires: %{name} = %{version}-%{release}

%description devel
This package contains development files required to build applications which
will use the VCD library.

%package python
Summary: Python wrapper for the VCD libraries
Group: Development/Languages
Requires: %{name} = %{version}-%{release}

%description python
This package contains python wrappers to access the VCD library.

%prep
%setup -q

%build
%configure
make

%install
rm -rf %{buildroot}
make install DESTDIR=%{buildroot}

%clean
%{__rm} -rf %{buildroot}

%files
%defattr(-,root,root,-)
%{_libdir}/*.so*

%files devel
%defattr(-,root,root,-)
%{_includedir}/*

%files python
%defattr(-,root,root,-)
%{python_sitearch}/vcd.so
%{python_sitearch}/vcd-*.egg-info

%changelog
* Sun Jan 6 2013 Sebastien Mirolo <smirolo@fortylines.com> - 0.1-0
- Initial RPM release
