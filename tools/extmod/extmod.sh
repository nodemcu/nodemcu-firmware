#!/bin/bash

# External modules update script.

# This script parses the repository root extmods.ini file to locate
# external modules to download

export EXTMOD_DIR="components/modules/external"      # Location where to store external modules:
EXTMOD_BIN_PATH="./tools/extmod"                     # Location of this script
TEMPLATE_MK="$EXTMOD_BIN_PATH/component.mk.template" # Location of the template component.mk

# Include the ini file reader script
. $EXTMOD_BIN_PATH/read_ini.sh

# Returns the given value in the INI file, passing section and value
function sectionVar() {
    local varname="INI__$1__$2"
    echo "${!varname}"
}

# helper pushd to make it silent
function pushd() {
    command pushd "$@" >/dev/null
}

# helper popd to make it silent
function popd() {
    command popd >/dev/null
}

function usage() {

    echo ""
    echo "extmod.sh - Manages external modules"
    echo "Usage:"
    echo "extmod.sh <commands>"
    echo "update : Parses extmods.ini and updates all modules"
    echo "clean  : Effectively cleans the contents of the external modules directory ($EXTMOD_DIR)"
    echo ""

}

# Generic command line parser
function readCommandLine() {
    while test ${#} -gt 0; do
        case "$1" in
        "clean")
            CLEAN=1
            ;;
        "update")
            UPDATE=1
            ;;
        *)
            echo -e "Error: Unrecognized parameter\n"
            usage
            exit 1
            ;;
        esac
        shift
    done
}

function updateMod() {
    local modname="$1"
    local url="$(sectionVar "$modname" "url")"
    local ref="$(sectionVar "$modname" "ref")"
    local disabled="$(sectionVar "$modname" "disabled")"
    local path="$EXTMOD_DIR/$modname"
    local component_mk="$path/component.mk"

    if [[ ! -d "$path" ]]; then
        echo "$modname not present. Downloading from $url ..."
        if ! git clone --quiet "$url" -b "$ref" "$path"; then
            echo "Error cloning $modname in $url"
            return 1
        fi
        # Add "component.mk" to local repo gitignore
        echo "component.mk" >>"$path/.git/info/exclude"
    fi
    echo "Updating $modname ..."

    if ! pushd "$path"; then
        echo "Cannot change to $path".
        return 1
    fi
    if ! git status >/dev/null; then
        echo "Error processing $modname. Error reading git repo status."
        popd
        return 1
    fi
    if [ "$(git status --short)" == "" ]; then
        if ! git fetch --quiet; then
            echo "Error fetching $modname"
            popd
            return 1
        fi
        if ! git checkout "$ref" --quiet; then
            echo "Error setting ref $ref in $modname. Does $ref exist?"
            popd
            return 1
        fi
        if ! git clean -d -f --quiet; then
            echo "Error repo $modname after checkout."
            popd
            return 1
        fi
        # check if HEAD was detached (like when checking out a tag or commit)
        if git symbolic-ref HEAD 2>/dev/null; then
            # This is a branch. Update it.
            if ! git pull --quiet; then
                echo "Error pulling $ref from $url."
                popd
                return 1
            fi
        fi

    else
        echo "$modname working directory in $path is not clean. Skipping..."
        popd
        return 0
    fi
    popd

    if [ "$disabled" != "1" ]; then
        if ! sed 's/%%MODNAME%%/'"$modname"'/g' "$TEMPLATE_MK" >"$component_mk"; then
            echo "Error generating $component_mk"
            return 1
        fi
    else
        echo "Warning: Module $modname is disabled and won't be included in build"
        [ -f "$component_mk" ] && rm "$component_mk"
    fi
    echo "Successfully updated $modname."
    return 0
}

function update() {

    if ! read_ini "extmods.ini"; then
        echo "Error reading extmods.ini"
    fi

    mkdir -p "$EXTMOD_DIR"

    for modname in $INI__ALL_SECTIONS; do
        if ! updateMod "$modname"; then
            echo "Error updating $modname"
            return 1
        fi
    done
    echo "Successfully updated all modules"
}

function main() {
    if [ "$CLEAN" == "1" ]; then
        echo "Cleaning ${EXTMOD_DIR:?} ..."
        rm -rf "${EXTMOD_DIR:?}/"*
    fi

    if [ "$UPDATE" == "1" ]; then
        update
    fi
}

readCommandLine "$@"
main
