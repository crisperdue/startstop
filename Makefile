DESTDIR:=/usr
DOCDIR:=$(DESTDIR)/share/doc/packages/startstop

# Try making them statically.  Awfully big.
# LDFLAGS := -static

# Try for backward compatibility.  Interesting but no go.
# CFLAGS := -D_GLIBC_2_0_SOURCE

%.gz : %
	gzip -c $< >$@

MANPAGES:=monitor.8.gz setuser.8.gz startstop.8.gz

version:=$(shell echo startstop-$$(grep "^Version: " startstop.spec | (read word n; echo $$n)))

.PHONY=tarball clean install

default: monitor setuser logto $(MANPAGES)

tarball: clean
	cd ..; rm -f $(version); ln -s startstop $(version); tar --exclude='*/.directory' -czvhf $(version).tar.gz $(version)

clean:
	rm -f monitor setuser logto *.gz *~

install:
	install -d $(DESTDIR)/bin $(DESTDIR)/share/man/man8
	install -d $(DOCDIR)
	install monitor setuser logto startstop $(DESTDIR)/bin
	install $(MANPAGES) $(DESTDIR)/share/man/man8
	install README CHANGES myservice myservice-redhat $(DOCDIR)
