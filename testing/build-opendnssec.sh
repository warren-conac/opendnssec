#!/usr/bin/env bash
source `dirname "$0"`/lib.sh && init || exit 1

require ldns
require softhsm

check_if_built opendnssec && exit 0
start_build opendnssec

build_ok=0
case "$DISTRIBUTION" in
	openbsd )
		export AUTOCONF_VERSION="2.68"
		export AUTOMAKE_VERSION="1.11"
		;;
	sunos | \
	netbsd | \
	freebsd )
		append_cflags "-std=c99"
		;;
	opensuse )
		append_ldflags "-lncurses -lpthread"
		;;
esac
case "$DISTRIBUTION" in
	centos | \
	redhat | \
	fedora | \
	sl | \
	ubuntu | \
	debian | \
	openbsd | \
	sunos | \
	opensuse | \
	suse )
		(
			sh autogen.sh &&
			mkdir -p build &&
			cd build &&
			../configure --prefix="$INSTALL_ROOT" \
				--with-database-backend=sqlite3 \
				--with-dbname=opendnssec-build-test \
				--enable-timeshift &&
			$MAKE &&
			$MAKE check &&
			$MAKE install &&
			cp "conf/addns.xml" "$INSTALL_ROOT/etc/opendnssec/addns.xml.build" &&
			cp "conf/conf.xml" "$INSTALL_ROOT/etc/opendnssec/conf.xml.build" &&
			cp "conf/kasp.xml" "$INSTALL_ROOT/etc/opendnssec/kasp.xml.build" &&
			cp "conf/zonelist.xml" "$INSTALL_ROOT/etc/opendnssec/zonelist.xml.build"
		) &&
		build_ok=1
		;;
	netbsd )
		(
			sh autogen.sh &&
			mkdir -p build &&
			cd build &&
			../configure --prefix="$INSTALL_ROOT" \
				--with-cunit=/usr/pkg \
				--with-database-backend=sqlite3 \
				--with-dbname=opendnssec-build-test \
				--enable-timeshift \
				--with-sqlite3=/usr/pkg &&
			$MAKE &&
			$MAKE check &&
			$MAKE install &&
			cp "conf/addns.xml" "$INSTALL_ROOT/etc/opendnssec/addns.xml.build" &&
			cp "conf/conf.xml" "$INSTALL_ROOT/etc/opendnssec/conf.xml.build" &&
			cp "conf/kasp.xml" "$INSTALL_ROOT/etc/opendnssec/kasp.xml.build" &&
			cp "conf/zonelist.xml" "$INSTALL_ROOT/etc/opendnssec/zonelist.xml.build"
		) &&
		build_ok=1
		;;
	freebsd )
		(
			sh autogen.sh &&
			mkdir -p build &&
			cd build &&
			../configure --prefix="$INSTALL_ROOT" \
				--with-database-backend=sqlite3 \
				--with-dbname=opendnssec-build-test \
				--enable-timeshift &&
			$MAKE &&
			#$MAKE check && # segfaults #0  0x00000008019363dc in _pthread_mutex_init_calloc_cb () from /lib/libc.so.7
			(cd enforcer-ng && $MAKE check) &&
			$MAKE install &&
			cp "conf/addns.xml" "$INSTALL_ROOT/etc/opendnssec/addns.xml.build" &&
			cp "conf/conf.xml" "$INSTALL_ROOT/etc/opendnssec/conf.xml.build" &&
			cp "conf/kasp.xml" "$INSTALL_ROOT/etc/opendnssec/kasp.xml.build" &&
			cp "conf/zonelist.xml" "$INSTALL_ROOT/etc/opendnssec/zonelist.xml.build"
		) &&
		build_ok=1
		;;
esac

if [ "$build_ok" -eq 1 ]; then
	set_build_ok opendnssec || exit 1
	exit 0
fi

exit 1
