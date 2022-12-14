# Pass --without docs to rpmbuild if you don't want the documetnation
Name: 		git-core
Version: 	0.99.6
Release: 	1
Vendor: 	Junio C Hamano <junkio@cox.net>
Summary:  	Git core and tools
License: 	GPL
Group: 		Development/Tools
URL: 		http://kernel.org/pub/software/scm/git/
Source: 	http://kernel.org/pub/software/scm/git/%{name}-%{version}.tar.gz
BuildRequires:	zlib-devel, openssl-devel, curl-devel  %{!?_without_docs:, xmlto, asciidoc > 6.0.3}
BuildRoot:	%{_tmppath}/%{name}-%{version}-root
Requires: 	sh-utils, diffutils, rsync, rcs, mktemp >= 1.5

%description
This is a stupid (but extremely fast) directory content manager.  It
doesn't do a whole lot, but what it _does_ do is track directory
contents efficiently. It is intended to be the base of an efficient,
distributed source code management system. This package includes
rudimentary tools that can be used as a SCM, but you should look
elsewhere for tools for ordinary humans layered on top of this.

%prep
%setup -q

%build
make prefix=%{_prefix} all %{!?_without_docs: doc}

%install
rm -rf $RPM_BUILD_ROOT
make DESTDIR=$RPM_BUILD_ROOT prefix=%{_prefix} mandir=%{_mandir} \
     install %{!?_without_docs: install-doc}

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%{_bindir}/*
%{_datadir}/git-core/templates/*
%doc README COPYING Documentation/*.txt
%{!?_without_docs: %doc Documentation/*.html }
%{!?_without_docs: %{_mandir}/man1/*.1.gz}
%{!?_without_docs: %{_mandir}/man7/*.7.gz}

%changelog
* Sun Aug 07 2005 Horst H. von Brand <vonbrand@inf.utfsm.cl>
- Redid the description
- Cut overlong make line, loosened changelog a bit
- I think Junio (or perhaps OSDL?) should be vendor...

* Thu Jul 14 2005 Eric Biederman <ebiederm@xmission.com>
- Add the man pages, and the --without docs build option

* Wed Jul 7 2005 Chris Wright <chris@osdl.org>
- initial git spec file
