#!/bin/sh
if [ $1 == 0 ]; then
	DST=${RPM_INSTALL_PREFIX}

	mkdir -p ${DST}/plug/business/
	touch ${DST}/plug/conflist.inf
	mkdir -p ${DST}/ticonf/
	touch ${DST}/ticonf/ti_features.conf

	sed -i '/ti_features.inf/d' ${DST}/plug/conflist.inf
fi
