Summary: Universal LSB-compliant service start/stop
Name: startstop
Version: 0.74
Release: 5
Source: http://www.perdues.com/download/%{name}-%{version}.tar.gz
License: GPL
Group: Development/Tools
URL: http://www.perdues.com/
Vendor: Perdue Software
Packager: Cris Perdue <cris@perdues.com>
BuildRoot: %{_tmppath}/%{name}-root
Provides: startstop
Provides: monitor

%description

The universal LSB (Linux Standard Base) start/stop package streamlines
authoring of application init scripts for daemon-style servers.  It
provides a full set of LSB actions (start/stop/status/etc.), and
supports running the server as a non-root user, protection against
SIGHUP, reliable service restart, service monitoring with auto-restart,
logging of events and output.  It was built originally for Java
servers, but can be valuable for many other servers as well.

AUTHOR: Crispin Perdue <cris@perdues.com>

%prep
%setup -q

%build
# ./configure
make

%install
rm -rf $RPM_BUILD_ROOT
make DESTDIR=$RPM_BUILD_ROOT/usr install

%clean
make clean

%files
%defattr(-,root,root)
/usr/bin/startstop
/usr/bin/monitor
/usr/bin/setuser
/usr/bin/logto
%docdir /usr/share/man
/usr/share/man/man8/startstop.8.gz
/usr/share/man/man8/monitor.8.gz
/usr/share/man/man8/setuser.8.gz
%docdir /usr/share/doc
%defattr(0644,root,root)
/usr/share/doc/packages/startstop/README
/usr/share/doc/packages/startstop/CHANGES
/usr/share/doc/packages/startstop/myservice
/usr/share/doc/packages/startstop/myservice-redhat
