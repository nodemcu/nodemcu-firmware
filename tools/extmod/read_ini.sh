#!/bin/bash
# Copyright (c) 2009    Kevin Porter / Advanced Web Construction Ltd
#                       (http://coding.tinternet.info, http://webutils.co.uk)
# Copyright (c) 2010-2014     Ruediger Meier <sweet_f_a@gmx.de>
#                             (https://github.com/rudimeier/)
#
# License: BSD-3-Clause, see LICENSE file
#
# Simple INI file parser.
#
# See README for usage.
#
#

function read_ini() {
    # Be strict with the prefix, since it's going to be run through eval
    function check_prefix() {
        if ! [[ "${VARNAME_PREFIX}" =~ ^[a-zA-Z_][a-zA-Z0-9_]*$ ]]; then
            echo "read_ini: invalid prefix '${VARNAME_PREFIX}'" >&2
            return 1
        fi
    }

    function check_ini_file() {
        if [ ! -r "$INI_FILE" ]; then
            echo "read_ini: '${INI_FILE}' doesn't exist or not" \
                "readable" >&2
            return 1
        fi
    }

    # enable some optional shell behavior (shopt)
    function pollute_bash() {
        if ! shopt -q extglob; then
            SWITCH_SHOPT="${SWITCH_SHOPT} extglob"
        fi
        if ! shopt -q nocasematch; then
            SWITCH_SHOPT="${SWITCH_SHOPT} nocasematch"
        fi
        shopt -q -s ${SWITCH_SHOPT}
    }

    # unset all local functions and restore shopt settings before returning
    # from read_ini()
    function cleanup_bash() {
        shopt -q -u ${SWITCH_SHOPT}
        unset -f check_prefix check_ini_file pollute_bash cleanup_bash
    }

    local INI_FILE=""
    local INI_SECTION=""

    # {{{ START Deal with command line args

    # Set defaults
    local BOOLEANS=1
    local VARNAME_PREFIX=INI
    local CLEAN_ENV=0

    # {{{ START Options

    # Available options:
    #	--boolean		Whether to recognise special boolean values: ie for 'yes', 'true'
    #					and 'on' return 1; for 'no', 'false' and 'off' return 0. Quoted
    #					values will be left as strings
    #					Default: on
    #
    #	--prefix=STRING	String to begin all returned variables with (followed by '__').
    #					Default: INI
    #
    #	First non-option arg is filename, second is section name

    while [ $# -gt 0 ]; do

        case $1 in

        --clean | -c)
            CLEAN_ENV=1
            ;;

        --booleans | -b)
            shift
            BOOLEANS=$1
            ;;

        --prefix | -p)
            shift
            VARNAME_PREFIX=$1
            ;;

        *)
            if [ -z "$INI_FILE" ]; then
                INI_FILE=$1
            else
                if [ -z "$INI_SECTION" ]; then
                    INI_SECTION=$1
                fi
            fi
            ;;

        esac

        shift
    done

    if [ -z "$INI_FILE" ] && [ "${CLEAN_ENV}" = 0 ]; then
        echo -e "Usage: read_ini [-c] [-b 0| -b 1]] [-p PREFIX] FILE" \
            "[SECTION]\n  or   read_ini -c [-p PREFIX]" >&2
        cleanup_bash
        return 1
    fi

    if ! check_prefix; then
        cleanup_bash
        return 1
    fi

    local INI_ALL_VARNAME="${VARNAME_PREFIX}__ALL_VARS"
    local INI_ALL_SECTION="${VARNAME_PREFIX}__ALL_SECTIONS"
    local INI_NUMSECTIONS_VARNAME="${VARNAME_PREFIX}__NUMSECTIONS"
    if [ "${CLEAN_ENV}" = 1 ]; then
        eval unset "\$${INI_ALL_VARNAME}"
    fi
    unset ${INI_ALL_VARNAME}
    unset ${INI_ALL_SECTION}
    unset ${INI_NUMSECTIONS_VARNAME}

    if [ -z "$INI_FILE" ]; then
        cleanup_bash
        return 0
    fi

    if ! check_ini_file; then
        cleanup_bash
        return 1
    fi

    # Sanitise BOOLEANS - interpret "0" as 0, anything else as 1
    if [ "$BOOLEANS" != "0" ]; then
        BOOLEANS=1
    fi

    # }}} END Options

    # }}} END Deal with command line args

    local LINE_NUM=0
    local SECTIONS_NUM=0
    local SECTION=""

    # IFS is used in "read" and we want to switch it within the loop
    local IFS=$' \t\n'
    local IFS_OLD="${IFS}"

    # we need some optional shell behavior (shopt) but want to restore
    # current settings before returning
    local SWITCH_SHOPT=""
    pollute_bash

    while read -r line || [ -n "$line" ]; do
        #echo line = "$line"

        ((LINE_NUM++))

        # Skip blank lines and comments
        if [ -z "$line" -o "${line:0:1}" = ";" -o "${line:0:1}" = "#" ]; then
            continue
        fi

        # Section marker?
        if [[ "${line}" =~ ^\[[a-zA-Z0-9_]{1,}\]$ ]]; then

            # Set SECTION var to name of section (strip [ and ] from section marker)
            SECTION="${line#[}"
            SECTION="${SECTION%]}"
            eval "${INI_ALL_SECTION}=\"\${${INI_ALL_SECTION}# } $SECTION\""
            ((SECTIONS_NUM++))

            continue
        fi

        # Are we getting only a specific section? And are we currently in it?
        if [ ! -z "$INI_SECTION" ]; then
            if [ "$SECTION" != "$INI_SECTION" ]; then
                continue
            fi
        fi

        # Valid var/value line? (check for variable name and then '=')
        if ! [[ "${line}" =~ ^[a-zA-Z0-9._]{1,}[[:space:]]*= ]]; then
            echo "Error: Invalid line:" >&2
            echo " ${LINE_NUM}: $line" >&2
            cleanup_bash
            return 1
        fi

        # split line at "=" sign
        IFS="="
        read -r VAR VAL <<<"${line}"
        IFS="${IFS_OLD}"

        # delete spaces around the equal sign (using extglob)
        VAR="${VAR%%+([[:space:]])}"
        VAL="${VAL##+([[:space:]])}"
        VAR=$(echo $VAR)

        # Construct variable name:
        # ${VARNAME_PREFIX}__$SECTION__$VAR
        # Or if not in a section:
        # ${VARNAME_PREFIX}__$VAR
        # In both cases, full stops ('.') are replaced with underscores ('_')
        if [ -z "$SECTION" ]; then
            VARNAME=${VARNAME_PREFIX}__${VAR//./_}
        else
            VARNAME=${VARNAME_PREFIX}__${SECTION}__${VAR//./_}
        fi
        eval "${INI_ALL_VARNAME}=\"\${${INI_ALL_VARNAME}# } ${VARNAME}\""

        if [[ "${VAL}" =~ ^\".*\"$ ]]; then
            # remove existing double quotes
            VAL="${VAL##\"}"
            VAL="${VAL%%\"}"
        elif [[ "${VAL}" =~ ^\'.*\'$ ]]; then
            # remove existing single quotes
            VAL="${VAL##\'}"
            VAL="${VAL%%\'}"
        elif [ "$BOOLEANS" = 1 ]; then
            # Value is not enclosed in quotes
            # Booleans processing is switched on, check for special boolean
            # values and convert

            # here we compare case insensitive because
            # "shopt nocasematch"
            case "$VAL" in
            yes | true | on)
                VAL=1
                ;;
            no | false | off)
                VAL=0
                ;;
            esac
        fi

        # enclose the value in single quotes and escape any
        # single quotes and backslashes that may be in the value
        VAL="${VAL//\\/\\\\}"
        VAL="\$'${VAL//\'/\'}'"

        eval "$VARNAME=$VAL"
    done <"${INI_FILE}"

    # return also the number of parsed sections
    eval "$INI_NUMSECTIONS_VARNAME=$SECTIONS_NUM"

    cleanup_bash
}
