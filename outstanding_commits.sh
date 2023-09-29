#!/bin/bash
#i.e. ./outstanding_commits.sh linux_next/master drivers/usb/mtu3
base=`git merge-base HEAD $1`
git log --oneline --reverse $base..HEAD $2 >a.txt; echo "" >>a.txt
git log --oneline --reverse $base..$1 $2 >b.txt; echo "" >>b.txt
./commit_unique.awk a.txt b.txt
rm a.txt b.txt

