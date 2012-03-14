Name:       libmm-player
Summary:    Multimedia Framework Player Library
Version:    0.2.12
Release:    1
Group:      System/Libraries
License:    Apache-2.0
Source0:    %{name}-%{version}.tar.gz
Patch0:     0001-enable-attrs-get-for-ximagesink.patch
Requires(post):  /sbin/ldconfig
Requires(postun):  /sbin/ldconfig
BuildRequires:  pkgconfig(mm-ta)
BuildRequires:  pkgconfig(mm-common), libmm-common-internal-devel
BuildRequires:  pkgconfig(mm-sound)
BuildRequires:  pkgconfig(gstreamer-0.10)
BuildRequires:  pkgconfig(gstreamer-plugins-base-0.10)
BuildRequires:  pkgconfig(gstreamer-interfaces-0.10)
BuildRequires:  pkgconfig(gstreamer-app-0.10)
BuildRequires:  pkgconfig(appcore-efl)
BuildRequires:  pkgconfig(elementary)
BuildRequires:  pkgconfig(mm-session)
BuildRequires:  pkgconfig(mmutil-imgp)
BuildRequires:  pkgconfig(audio-session-mgr)
BuildRequires:  pkgconfig(ecore-x)
BuildRequires:  pkgconfig(evas)
BuildRequires:  pkgconfig(iniparser)
BuildRequires:  pkgconfig(libcrypto)

%description

%package devel
Summary:    Multimedia Framework Player Library (DEV)
Group:      Development/Libraries
Requires:   %{name} = %{version}-%{release}

%description devel


%prep
%setup -q
%patch0 -p1

%build

./autogen.sh

CFLAGS+=" -DMMFW_DEBUG_MODE -DGST_EXT_TIME_ANALYSIS -DAUDIO_FILTER_EFFECT -DEXPORT_API=\"__attribute__((visibility(\\\"default\\\")))\" "; export CFLAGS
LDFLAGS+="-Wl,--rpath=%{_prefix}/lib -Wl,--hash-style=both -Wl,--as-needed"; export LDFLAGS

%configure --prefix=%{_prefix} 
make 

%install
rm -rf %{buildroot}
%make_install

rm -f %{buildroot}/usr/bin/test_alarmdb


%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig


%files
%{_libdir}/*.so.*
%{_bindir}/*


%files devel
%{_libdir}/*.so
%{_includedir}/mmf/mm_player.h
%{_includedir}/mmf/mm_player_sndeffect.h
%{_includedir}/mmf/mm_player_internal.h
%{_libdir}/pkgconfig/*.pc






