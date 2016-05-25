%bcond_with wayland
%bcond_with x

Name:       libmm-player
Summary:    Multimedia Framework Player Library
Version:    0.5.78
Release:    0
Group:      Multimedia/Libraries
License:    Apache-2.0
URL:        http://source.tizen.org
Source0:    %{name}-%{version}.tar.gz
Source1001:     libmm-player.manifest
Requires(post):  /sbin/ldconfig
Requires(postun):  /sbin/ldconfig
BuildRequires:  pkgconfig(dlog)
BuildRequires:  pkgconfig(mm-common)
BuildRequires:  pkgconfig(mm-sound)
BuildRequires:  pkgconfig(gstreamer-1.0)
BuildRequires:  pkgconfig(gstreamer-plugins-base-1.0)
%if %{with wayland}
BuildRequires:  pkgconfig(gstreamer-wayland-1.0)
%endif
BuildRequires:  pkgconfig(gstreamer-video-1.0)
BuildRequires:  pkgconfig(gstreamer-app-1.0)
BuildRequires:  pkgconfig(appcore-efl)
BuildRequires:  pkgconfig(elementary)
BuildRequires:  pkgconfig(mmutil-imgp)
BuildRequires:  pkgconfig(evas)
BuildRequires:  pkgconfig(iniparser)
BuildRequires:  pkgconfig(vconf)
BuildRequires:  pkgconfig(icu-i18n)
BuildRequires:  pkgconfig(capi-media-tool)
BuildRequires:  pkgconfig(murphy-resource)
BuildRequires:  pkgconfig(murphy-glib)

%description
Multimedia Framework Player Library files.

%package devel
Summary:    Multimedia Framework Player Library (DEV)
Group:      Development/Libraries
Requires:   %{name} = %{version}-%{release}

%description devel
Multimedia Framework Player Library files (DEV).

%prep
%setup -q
cp %{SOURCE1001} .

%build
export CFLAGS+=" -Wall -DTIZEN_DEBUG -D_FILE_OFFSET_BITS=64 -DSYSCONFDIR=\\\"%{_sysconfdir}\\\" -DEXPORT_API=\"__attribute__((visibility(\\\"default\\\")))\" "
%if %{with x}
export CFLAGS+=" -DHAVE_X11"
%endif
%if %{with wayland}
export CFLAGS+=" -DHAVE_WAYLAND"
%endif
%if "%{?profile}" == "tv"
export CFLAGS+=" -DTIZEN_TV"
%endif

LDFLAGS+="-Wl,--rpath=%{_prefix}/lib -Wl,--hash-style=both -Wl,--as-needed"; export LDFLAGS
./autogen.sh
# always enable sdk build. This option should go away
CFLAGS=$CFLAGS LDFLAGS=$LDFLAGS ./configure --enable-sdk --prefix=%{_prefix} --disable-static
%configure \
%if %{with x}
--disable-static
%endif
%if %{with wayland}
--disable-static \
--enable-wayland
%endif
#%__make -j1
make %{?jobs:-j%jobs}

%install
%make_install

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%manifest %{name}.manifest
%license LICENSE.APLv2
%defattr(-,root,root,-)
%{_libdir}/*.so.*


%files devel
%manifest %{name}.manifest
%defattr(-,root,root,-)
%{_libdir}/*.so
%{_includedir}/mmf/*.h
%{_libdir}/pkgconfig/*
