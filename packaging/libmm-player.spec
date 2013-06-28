Name:       libmm-player
Summary:    Multimedia Framework Player Library
Version:    0.2.19
Release:    0
Group:      System/Libraries
License:    Apache-2.0
URL:        http://source.tizen.org
Source0:    %{name}-%{version}.tar.gz
Source1001: 	libmm-player.manifest
BuildRequires:  pkgconfig(mm-ta)
BuildRequires:  pkgconfig(mm-common)
BuildRequires:  pkgconfig(mm-sound)

%if %{defined with_Gstreamer0.10}
BuildRequires:  pkgconfig(gstreamer-0.10)
BuildRequires:  pkgconfig(gstreamer-plugins-base-0.10)
BuildRequires:  pkgconfig(gstreamer-interfaces-0.10)
BuildRequires:  pkgconfig(gstreamer-app-0.10)
%else
BuildRequires:  pkgconfig(gstreamer-1.0)
BuildRequires:  pkgconfig(gstreamer-plugins-base-1.0)
BuildRequires:  pkgconfig(gstreamer-app-1.0)
%endif

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
cp %{SOURCE1001} .

%build

##  %autogen.sh

%if %{defined with_Gstreamer0.10}
export GSTREAMER_API=""
%else
export GSTREAMER_API="-DGST_API_VERSION_1=1"
export use_gstreamer_1=1
%endif

CFLAGS+=" -DEXPORT_API=\"__attribute__((visibility(\\\"default\\\")))\" $GSTREAMER_API" ; export CFLAGS
LDFLAGS+="-Wl,--rpath=%{_libdir} -lgstvideo-1.0 -Wl,--hash-style=both -Wl,--as-needed"; export LDFLAGS

%configure --disable-static

make -j1 

%install
%make_install

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig


%files
%manifest %{name}.manifest
%defattr(-,root,root,-)
%{_libdir}/*.so.*


%files devel
%manifest %{name}.manifest
%defattr(-,root,root,-)
%{_libdir}/*.so
%{_includedir}/mmf/*.h
%{_libdir}/pkgconfig/*

