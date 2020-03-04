Name: qtcontacts-sqlite-qt5
Version: 0.2.36
Release: 0
Summary: SQLite-based plugin for QtPIM Contacts
Group: System/Plugins
License: BSD
URL: https://git.merproject.org/mer-core/qtcontacts-sqlite
Source0: %{name}-%{version}.tar.gz
BuildRequires: pkgconfig(Qt5Core)
BuildRequires: pkgconfig(Qt5Sql)
BuildRequires: pkgconfig(Qt5DBus)
BuildRequires: qt5-qtpim-contacts-devel >= 5.9.0
BuildRequires: pkgconfig(mlite5)
Requires: qt5-plugin-sqldriver-sqlite

%description
%{summary}.

%files
%defattr(-,root,root,-)
%{_libdir}/qt5/plugins/contacts/*.so*

%package extensions
Summary:    QtContacts extension headers for qtcontacts-sqlite-qt5
Group:      Development/Libraries
Requires:   %{name} = %{version}-%{release}

%description extensions
This package contains extension headers for the qtcontacts-sqlite-qt5 library.

%files extensions
%defattr(-,root,root,-)
%{_libdir}/pkgconfig/qtcontacts-sqlite-qt5-extensions.pc
%{_includedir}/qtcontacts-sqlite-qt5-extensions/*


%prep
%setup -q -n %{name}-%{version}

%build
%qmake5 "VERSION=%{version}"
make %{?_smp_mflags}

%install
rm -rf %{buildroot}
%qmake5_install

