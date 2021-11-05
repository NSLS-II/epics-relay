# Undefine CMake in-source builds in order to be consistent with f33+
%undefine __cmake_in_source_build

%global _hardened_build 1
%global debug_package %{nil}
%global __cmake cmake3

Name:           epics-relay
Version:        %{version}
Release:        0%{?dist}
Summary:        EPICS UDP Relay and tunnel

License:        BSD
URL:            https://github.com/NSLS-II/epics-relay
Source0:        https://github.com/NSLS-II/epics-relay/archive/%v{version}/epics-relay-v%{version}.tar.gz

BuildRequires:  cmake
BuildRequires:  libnet-devel
BuildRequires:  systemd-devel
BuildRequires:  systemd-rpm-macros
Requires:       libnet
Requires:       pcre2-devel

%description
epics-relay UDP Relay and tunnel

%prep
%autosetup

%build
cmake3 -DCMAKE_INSTALL_PREFIX:PATH=/usr -DCPPLINT_CHECK=0 -DNO_IN_SOURCE_BUILDS=NO -DDEBUG=NO
VERBOSE=1 make

%install
%make_install

#%post
#%systemd_post arpwatch.service

#%preun
#%systemd_preun arpwatch.service

#%postun
#%systemd_postun_with_restart arpwatch.service

%files
%license LICENSE
%{_bindir}/epics_udp_collector
%{_bindir}/epics_udp_emitter
%{_unitdir}/epics_udp_emitter.service
%{_unitdir}/epics_udp_collector.service
%{_sysconfdir}/epics-relay.conf

#%changelog
#* Thu Jul 22 2021 Stuart Campbell <scampbell@bnl.gov> 0.1.2-2
#- Added systemd support
