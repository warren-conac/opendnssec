# $Id$

ACLOCAL_AMFLAGS = -I m4

MAINTAINERCLEANFILES = \
	config.log config.status \
	$(srcdir)/Makefile.in \
	$(srcdir)/common/config.h.in $(srcdir)/common/config.h.in~ \
	$(srcdir)/configure \
	$(srcdir)/install-sh $(srcdir)/ltmain.sh $(srcdir)/missing \
	$(srcdir)/depcomp $(srcdir)/aclocal.m4 $(srcdir)/compile \
	$(srcdir)/config.guess $(srcdir)/config.sub

SUBDIRS = common libhsm conf tools contrib

if ENABLE_SIGNER
SUBDIRS += signer
endif

if ENABLE_ENFORCERNG
SUBDIRS += enforcer-ng
endif

if ENABLE_EPPCLIENT
SUBDIRS += plugins/eppclient
endif

EXTRA_DIST =	$(srcdir)/LICENSE \
		$(srcdir)/KNOWN_ISSUES \
		$(srcdir)/MIGRATION \
		$(srcdir)/README.enforcer_testers \
		$(srcdir)/plugins/simple-dnskey-mailer/simple-dnskey-mailer.sh


install-data-hook:
	$(INSTALL) -d $(DESTDIR)$(localstatedir)
	$(INSTALL) -d $(DESTDIR)$(localstatedir)/opendnssec
	$(INSTALL) -d $(DESTDIR)$(localstatedir)/opendnssec/tmp
	$(INSTALL) -d $(DESTDIR)$(localstatedir)/opendnssec/signconf
	$(INSTALL) -d $(DESTDIR)$(localstatedir)/opendnssec/unsigned
	$(INSTALL) -d $(DESTDIR)$(localstatedir)/opendnssec/signed
	$(INSTALL) -d $(DESTDIR)$(localstatedir)/run
	$(INSTALL) -d $(DESTDIR)$(localstatedir)/run/opendnssec

docs:
	(cd libhsm; $(MAKE) doxygen)
if ENABLE_ENFORCERNG
	(cd enforcer-ng; $(MAKE) doxygen)
endif
if ENABLE_SIGNER
	(cd signer; $(MAKE) doxygen)
endif

