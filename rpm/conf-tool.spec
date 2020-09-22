Name:    conf-tool
Summary: Config file manupulation tool
Version: 1.0.0
Release: 1
License: BSD
URL:     https://github.com/monich/conf-tool
Source0: %{name}-%{version}.tar.bz2

%define glib_version 2.6
Requires: glib2 >= %{glib_version}
BuildRequires: pkgconfig(glib-2.0) >= %{glib_version}

%description
Utility for manipulating config files from the command line.

%prep
%setup -q -n %{name}-%{version}

%build
make KEEP_SYMBOLS=1 release

%install
rm -rf %{buildroot}
make DESTDIR=%{buildroot} install

%files
%defattr(-,root,root,-)
%{_bindir}/conf-tool
