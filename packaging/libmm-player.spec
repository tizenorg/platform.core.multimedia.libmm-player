Name:       libmm-player
Summary:    Multimedia Framework Player Library
Version:    0.2.13
Release:    0
Group:      System/Libraries
License:    Apache-2.0
URL:        http://source.tizen.org
Source0:    %{name}-%{version}.tar.gz
BuildRequires:  pkgconfig(mm-ta)
BuildRequires:  pkgconfig(mm-common)
BuildRequires:  pkgconfig(mm-sound)
BuildRequires:  pkgconfig(gstreamer-0.10)
BuildRequires:  pkgconfig(gstreamer-plugins-base-0.10)
BuildRequires:  pkgconfig(gstreamer-interfaces-0.10)
BuildRequires:  pkgconfig(gstreamer-app-0.10)
BuildRequires:  pkgconfig(mm-session)
BuildRequires:  pkgconfig(mmutil-imgp)
BuildRequires:  pkgconfig(audio-session-mgr)
BuildRequires:  pkgconfig(iniparser)
BuildRequires:  pkgconfig(vconf)
BuildRequires:  pkgconfig(icu-i18n)

%description
Multimedia Framework Player Library.

%package devel
Summary:    Multimedia Framework Player Library (DEV)
Group:      Development/Libraries
Requires:   %{name} = %{version}-%{release}

%description devel
Multimedia Framework Player Library (DEV).

%prep
%setup -q

%build

./autogen.sh

CFLAGS+=" -DEXPORT_API=\"__attribute__((visibility(\\\"default\\\")))\" "; export CFLAGS
LDFLAGS+="-Wl,--rpath=%{_prefix}/lib -Wl,--hash-style=both -Wl,--as-needed"; export LDFLAGS

CFLAGS=$CFLAGS LDFLAGS=$LDFLAGS ./configure --prefix=%{_prefix} --disable-static

make -j1 

%install
%make_install

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig


%files
%manifest libmm-player.manifest
%defattr(-,root,root,-)
%{_libdir}/*.so.*


%files devel
%defattr(-,root,root,-)
%{_libdir}/*.so
%{_includedir}/mmf/*.h
%{_libdir}/pkgconfig/*

