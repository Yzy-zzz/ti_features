#!/bin/sh
DST=${RPM_INSTALL_PREFIX}
mkdir -p ${DST}/plug/business/
touch ${DST}/plug/conflist.inf
mkdir -p ${DST}/ticonf/i
touch ${DST}/ticonf/ti_features.conf

if [[ -z `grep -rn '\[business\]' ${DST}/plug/conflist.inf` ]];then
	echo '[business]' >> ${DST}/plug/conflist.inf
fi

if [[ -z `grep -rn 'ti_features.inf' ${DST}/plug/conflist.inf` ]];then
	sed -i '/\[business\]/a\./plug/business/ti_features/ti_features.inf' ${DST}/plug/conflist.inf
fi
