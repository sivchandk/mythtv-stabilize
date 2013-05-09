#!/bin/bash

set -x
# Changelog version needs to be of the format
# 0.26-20130501-4653bg3

export DEBEMAIL="sysadmin@digital-nirvana.com"
export DEBFULLNAME="Digital Nirvana"

TODAY=$(date +%Y%m%d)
GIT_LAST_HASH=$(dpkg-parsechangelog | sed -e '/Version/!d' | awk -F'-' '{print $3}')
GIT_CURRENT_HASH=$(git log --oneline -1 | awk '{print $1}')
GIT_BRANCH=$(git branch | grep "*" | sed "s/* //" | awk '{printf $0}' | cut -d '/' -f2 | sed -e "s/^[ \t]*//" | sed -e "s/[ \t]*$//")

# Update changelog

# Checking if previous changelog has a GIT HASH
if [ -z ${GIT_LAST_HASH} ]
then
	echo "ERROR: Missing LAST Commit HASH in changelog"
	echo "       Changelog of the last entry should be like"
	echo "       0.26-2013-0501-4563bg3 (BRANCH-DATE-GIT-HASH)"
	exit 1
fi

# Checking if the GIT_LAST_HASH is a valid commit hash
if [ $(git branch -r --contains ${GIT_LAST_HASH} > /dev/null 2>&1; echo $? ) -ne 0 ]
then
	echo "Invalid GIT HASH"
	exit 1
fi

GIT_LAST_BRANCH=$(git branch -r --contains ${GIT_LAST_HASH} | sed -e "s/^[ \t]*//" | sed -e "s/[ \t]*$//")

#if [ $(git rev-list --boundary ${GIT_LAST_HASH}..${GIT_CURRENT_HASH} > /dev/null 2>&1; echo $? ) -ne 0 ]
#then
#	echo "Current Commit ${GIT_CURRENT_HASH} is not reachable from the previous commit ${GIT_LAST_HASH}"
#fi

if [ ${GIT_CURRENT_HASH} != ${GIT_LAST_HASH} ]
then
	if [ ${GIT_BRANCH} == ${GIT_LAST_BRANCH} ] 
	then
		dch -v "${GIT_BRANCH}-${TODAY}-${GIT_CURRENT_HASH}" "Upstream changes since last package ${GIT_LAST_HASH}"
		git log --oneline $GIT_LAST_HASH...$GIT_CURRENT_HASH | sed 's,^,[,; s, ,] ,; s,Version,version,' > .gitout
		while read line
		do
			dch -a "$line"
		done < .gitout
		rm -f .gitout
	else
		dch -v "${GIT_BRANCH}-${TODAY}-${GIT_CURRENT_HASH}" "New checkout since last package ${GIT_LAST_BRANCH} / ${GIT_LAST_HASH}"
	fi
fi
