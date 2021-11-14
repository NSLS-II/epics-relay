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
BuildRequires:  pcre2-devel
BuildRequires:  libconfig-devel
BuildRequires:  systemd-devel
BuildRequires:  systemd-rpm-macros
Requires:       libnet
Requires:       pcre2
Requires:       libconfig

%description
epics-relay UDP Relay and tunnel

%prep
%autosetup

%build
cmake3 -DCMAKE_INSTALL_PREFIX:PATH=/usr -DCPPLINT_CHECK=0 -DNO_IN_SOURCE_BUILDS=NO -DDEBUG=NO
VERBOSE=1 make

%install
%make_install

%files
%license LICENSE
%{_bindir}/epics_udp_collector
%{_bindir}/epics_udp_emitter
%{_unitdir}/epics_udp_emitter@.service
%{_unitdir}/epics_udp_collector@.service
%{_sysconfdir}/epics-relay_default.conf

%changelog
* Sun Nov 07 2021 Stuart B. Wilkins <swilkins@bnl.gov> - %{version}-0.el8
- Added deps
