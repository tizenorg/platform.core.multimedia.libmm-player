%bcond_with wayland
%bcond_with x

Name:       libmm-player
Summary:    Multimedia Framework Player Library
Version:    0.5.62
Release:    0
Group:      Multimedia/Libraries
License:    Apache-2.0
URL:        http://source.tizen.org
Source0:    %{name}-%{version}.tar.gz
Source1001:     libmm-player.manifest
Requires(post):  /sbin/ldconfig
Requires(postun):  /sbin/ldconfig
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
%if %{with x}
CFLAGS+="  -Wall -D_FILE_OFFSET_BITS=64 -DTEST_ES -DHAVE_X11 -DEXPORT_API=\"__attribute__((visibility(\\\"default\\\")))\" "; export CFLAGS
%endif
%if %{with wayland}
CFLAGS+="  -Wall -D_FILE_OFFSET_BITS=64 -DTEST_ES -DHAVE_WAYLAND -DEXPORT_API=\"__attribute__((visibility(\\\"default\\\")))\" "; export CFLAGS
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
mkdir -p %{buildroot}%{_datadir}/license
cp -rf LICENSE.APLv2 %{buildroot}%{_datadir}/license/%{name}
%make_install

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%manifest %{name}.manifest
%defattr(-,root,root,-)
%{_libdir}/*.so.*
%{_datadir}/license/%{name}

%files devel
%manifest %{name}.manifest
%defattr(-,root,root,-)
%{_libdir}/*.so
%{_includedir}/mmf/*.h
%{_libdir}/pkgconfig/*
