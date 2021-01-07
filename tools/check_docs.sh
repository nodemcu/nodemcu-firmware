#!/bin/bash

# get all linked module docs for mkdocs.yml
grep "modules/" mkdocs.yml |sed "s/ *- .*: *'//" | sed "s/'//" | sort> /tmp/doc

# get all module and lus_module *.md files
ls docs/modules/* docs/lua-modules/*|sed "sxdocs/xx" | sort > /tmp/files

diff /tmp/doc /tmp/files && echo "all *.md files are reflected in mkdocs.yml"

