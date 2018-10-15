#!/bin/bash
#
#  git msg checker
#
#  author: xiaobo.gu@amlogic.com
#
#  2018.04.20: init version
#  2018.04.30: proj list and some detail update
#  2018.10.11: BL parser use same one as all other gits, seperate BUGZILLA/JIRA parser
#
#
#

# variables
declare -i IS_BOOTLOADER_GIT=0
declare -i MSG_LENGTH_LIMIT=80
declare -i MSG_CHECK_RESULT=0

declare CHECK_COMMIT_ID=""
declare CHECK_PROJECT=""
declare CHECK_LOG_FILE=""
declare CHECK_PATCH=""

# add all pre check git repository here
declare -a BOOTLOADER_GIT=("bootrom/ap" \
                        "bootrom/sp" \
                        "bootrom/scp" \
                        "bootloader/spl" \
                        "firmware/scp" \
                        "ARM-software/arm-trusted-firmware" \
                        "OP-TEE/optee_os" \
                        "uboot" \
			"rtos/freertos")

# msg format: module: [submodule:] content\n\nPD#XXXXXX\nProblem:\nXXX\nSolution:\nXXX\nVerify:\nXXX\nChange-Id: XXXX\nSigned-off-by: XXXX
MSG_PARSER_BUGZILLA='^.*: .* \[[1-9]\/[1-9]\].^$.^PD# ?[1-9][0-9]{4,8}$.^$.^Problem:.^.*$.^$.^Solution:$.^.*$.^$.^Verify:$.^.*$.^$.^Change-Id: .*$.^Signed-off-by: .*$'
MSG_PARSER_JIRA='^.*: .* \[[1-9]\/[1-9]\].^$.^PD# ?[A-Z]{1,8}-[0-9]{1,7}$.^$.^Problem:.^.*$.^$.^Solution:$.^.*$.^$.^Verify:$.^.*$.^$.^Change-Id: .*$.^Signed-off-by: .*$'
PARA_PARSER_COMMIT='^commit=.*$'
PARA_PARSER_LOGFILE='^logfile=.*$'
PARA_PARSER_PATCH='^patch=.*$'
PARA_PARSER_PROJ='^proj=.*$'

MSG_REPORT=""
MSG_LINE_CHECK=""

declare GIT_MSG_FORMAT_LINK="http://wiki-china.amlogic.com/Platform/Bootloader/Bootloader_commit_message_format"
declare GIT_MSG_DEMO=\
" \n\
module: sub-module: subject [n/m]\n\
 \n\
PD#XXXXXX\n\
 \n\
Problem:\n\
1. xxxxx\n\
2. xxxxx\n\
 \n\
Solution:\n\
1. xxxxx\n\
2. xxxxx\n\
 \n\
Verify:\n\
1. xxxxx\n\
2. xxxxx\n\
 \n\
Change-ID: xxxxx\n\
Signed-off-by: xxxxx\n\
 \n\
"

function gen_log() {
	# $1: log file name
	if [[ 0 -eq ${MSG_CHECK_RESULT} ]]; then
		cat << EOF > $1
{"message": "This is an automated message.\n${MSG_REPORT}", "labels": {"Verified": 1}, "notify": "NONE"}
EOF
	else
		cat << EOF > $1
{"message": "This is an automated message.\n${MSG_REPORT}", "labels": {"Verified": -1}, "notify": "OWNER"}
EOF
	fi
}

function report() {
	echo -e "MSG_REPORT: \n"
	echo -e ${MSG_REPORT}
	if [[ "" != ${CHECK_LOG_FILE} ]]; then
		gen_log ${CHECK_LOG_FILE}
	fi
}

function check_length() {
	# $1: line_msg, $2: line_num
	local line_msg=$1
	local length=${#line_msg}
	local -i line_num=$2
	line_num=$((line_num + 1))
	if [[ $length -gt ${MSG_LENGTH_LIMIT} ]]; then
		MSG_LINE_CHECK=${MSG_LINE_CHECK}"  Line $line_num($length)	check FAILED!\n"
		MSG_CHECK_RESULT=$((MSG_CHECK_RESULT + 1))
#	else
#		MSG_REPORT=${MSG_REPORT}"Line $line_num	check PASS!\n"
	fi
}

function bootloader_git_check() {
	# $1: git proj
	local project=$1
	IS_BOOTLOADER_GIT=0
	for loop in ${!BOOTLOADER_GIT[@]}; do
		if [ "$project" == "${BOOTLOADER_GIT[${loop}]}" ]; then
			IS_BOOTLOADER_GIT=1
			break
		fi
	done
	if [ 1 -eq $IS_BOOTLOADER_GIT ]; then
		echo "this is a bootloader git repository"
	else
		echo "this is not a bootloader git repository"
	fi
}

function check_commit_msg() {
	#echo "check git msg"
	#echo $MSG
	#echo ""

	MSG_REPORT=" \n \nGIT MSG checker\n"
	MSG_REPORT=${MSG_REPORT}" \n1. Check commit message length(max ${MSG_LENGTH_LIMIT} char each line): "

	# convert to lines
	IFS=$'\n' readarray -t MSG_LINES <<< "$MSG"
	for line_num in ${!MSG_LINES[@]}; do
		# process line by line
		# line1, title
		echo "MSG_LINES$line_num, len: ${#MSG_LINES[$line_num]}: ${MSG_LINES[$line_num]}"
		check_length "${MSG_LINES[$line_num]}" $line_num
	done
	if [[ 0 -eq ${MSG_CHECK_RESULT} ]]; then
		MSG_REPORT=${MSG_REPORT}"PASS!\n"
	else
		MSG_REPORT=${MSG_REPORT}"FAILED!\n"
		MSG_REPORT=${MSG_REPORT}${MSG_LINE_CHECK}
	fi

	MSG_REPORT=${MSG_REPORT}" \n2. Check commit message format: "
	# check whole git msg
	if [[ $MSG =~ $MSG_PARSER_BUGZILLA ]]; then
		echo "GIT MSG check match (bugzilla)"
		MSG_REPORT=${MSG_REPORT}"PASS!\n"
	elif [[ $MSG =~ $MSG_PARSER_JIRA ]]; then
		echo "GIT MSG check match (jira)"
		MSG_REPORT=${MSG_REPORT}"PASS!\n"
	else
		echo "GIT MSG check unmatch"
		MSG_REPORT=${MSG_REPORT}"FAILED!\n"
		#MSG_REPORT=${MSG_REPORT}"  Please follow this format:\n"
		#MSG_REPORT=${MSG_REPORT}${GIT_MSG_DEMO}
		MSG_REPORT=${MSG_REPORT}"  Please review:\n  "${GIT_MSG_FORMAT_LINK}
		MSG_CHECK_RESULT=$((MSG_CHECK_RESULT + 1))
	fi
}

function get_commit_msg() {
	if [[ "" != ${GERRIT_PROJECT} ]]; then
		# get git log from patch file
		MSG=`cat ${CHECK_PATCH}`
		#MSG=${MSG#*[PATCH] %%---*} # get info before "---" = git msg
		MSG=${MSG#*\[PATCH\] }
		MSG=${MSG%%---*}
	else
		# get git log from local git (for test)
		MSG=`git cat-file commit "${CHECK_COMMIT_ID}" | sed '1,/^$/d'`
	fi
	echo -e $MSG
}

function usage() {
	cat << EOF
	Usage:
	$(basename $0) --help

	$(basename $0) [commit=XXX] [logfile=YYY] [proj=ZZZ] [patch=NNN]

	1. commit=XXX: specify local commit id. use HEAD~0 by default
	2. logfile=YYY: specify log file, for jenkins use
	3. proj=ZZZ: specify git repository name, for jenkins use
	4. patch=NNN: check specified patch file(generated by git format-patch)

EOF
	exit 1
}

function parser() {
	for arg in "$@" ; do
		echo "arg: ${arg}"
		if [[ ${arg} =~ ${PARA_PARSER_COMMIT} ]]; then
			# found commit=xx para
			echo "commit match"
			CHECK_COMMIT_ID=${arg#*=}
		elif [[ ${arg} =~ ${PARA_PARSER_LOGFILE} ]]; then
			echo "logfile match"
			CHECK_LOG_FILE=${arg#*=}
		elif [[ ${arg} =~ ${PARA_PARSER_PATCH} ]]; then
			echo "patch match"
			CHECK_PATCH=${arg#*=}
		elif [[ ${arg} =~ ${PARA_PARSER_PROJ} ]]; then
			echo "proj match"
			CHECK_PROJECT=${arg#*=}
		elif [[ ${arg} == "--help" ]]; then
			usage
		fi
	done
}

function postreview() {
    # $1: change number
    # $2: patch set number
    # $3: ReviewInput JSON filename
    local url=http://$CHECKPATCH_GERRIT_SERVER/a/changes/$1/revisions/$2/review
    #    --trace-ascii -
    curl   \
        -f \
        -s -S \
        --digest \
        --netrc \
        -H "Content-Type: application/json" \
        -H "Accept: application/json" \
        --data-binary @$3 \
        "$url"
    return $?
}

function main() {
	CHECK_PROJECT=${GERRIT_PROJECT}
	parser $@

	# check commit id
	if [[ "" == "${CHECK_COMMIT_ID}" ]]; then
		CHECK_COMMIT_ID="HEAD~0"
	fi

	# check proj name
	if [[ "" == "${CHECK_PROJECT}" ]]; then
		# just for local test
		CHECK_PROJECT=${BOOTLOADER_GIT[0]}
	fi
	echo "CHECK_PROJECT: ${CHECK_PROJECT}"

	# check log file
	if [[ "" == ${CHECK_LOG_FILE} ]]; then
		CHECK_LOG_FILE="git_msg_check_log"
	fi
	if [ ! -f "${CHECK_LOG_FILE}" ]; then
		touch ${CHECK_LOG_FILE}
	fi

	# bootloader_git_check "${CHECK_PROJECT}"
	get_commit_msg
	check_commit_msg
	report

	if [[ "" != ${CHECK_LOG_FILE} ]] && [[ "" != ${GERRIT_PROJECT} ]]; then
		echo "Posting review."
		cat ${CHECK_LOG_FILE}
		if ! time postreview $GERRIT_CHANGE_NUMBER $GERRIT_PATCHSET_NUMBER ${CHECK_LOG_FILE}; then
			exit 1;
		fi
		echo "Posted review."
	fi
}

main $@
