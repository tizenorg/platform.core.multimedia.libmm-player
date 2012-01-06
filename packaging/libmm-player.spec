
Name:       libmm-player
Summary:    Multimedia Framework Player Library
Version:    0.2.9
Release:    22
Group:      System/Libraries
License:    TBD
Source0:    %{name}-%{version}.tar.bz2
Requires(post):  /sbin/ldconfig
Requires(postun):  /sbin/ldconfig
BuildRequires:  pkgconfig(mm-ta)
BuildRequires:  pkgconfig(mm-common)
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


BuildRoot:  %{_tmppath}/%{name}-%{version}-build

%description

%package devel
Summary:    Multimedia Framework Player Library (DEV)
Group:      Development/Libraries
Requires:   %{name} = %{version}-%{release}

%description devel

%package factory
Summary:    Multimedia Framework Player Library (Factory)
Group:      Development/Libraries
Requires:   %{name} = %{version}-%{release}

%description factory

%prep
%setup -q

%build

./autogen.sh

CFLAGS+=" -DMMFW_DEBUG_MODE -DGST_EXT_TIME_ANALYSIS -DAUDIO_FILTER_EFFECT -DEXPORT_API=\"__attribute__((visibility(\\\"default\\\")))\" "; export CFLAGS
LDFLAGS+="-Wl,--rpath=%{_prefix}/lib -Wl,--hash-style=both -Wl,--as-needed"; export LDFLAGS

# always enable sdk build. This option should go away
./configure --enable-sdk --prefix=%{_prefix} --disable-static

# Call make instruction with smp support
make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%make_install

rm -f %{buildroot}/usr/bin/test_alarmdb

%clean
rm -rf %{buildroot}



%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig


%files
%defattr(-,root,root,-)
%{_libdir}/*.so.*
%{_bindir}/*


%files devel
%defattr(-,root,root,-)
%{_libdir}/*.so
%{_includedir}/mmf/mm_player.h
%{_includedir}/mmf/mm_player_sndeffect.h
%{_includedir}/mmf/mm_player_internal.h
%{_libdir}/pkgconfig/*

%files factory
%defattr(-,root,root,-)
%{_includedir}/mmf/mm_player_factory.h





