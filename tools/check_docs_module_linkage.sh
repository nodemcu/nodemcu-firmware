#!/bin/bash

# get all linked module docs for mkdocs.yml
grep "modules/" mkdocs.yml | sed "s/ *- .*: *'//" | sed "s/'//" | sort > /tmp/doc

# get all module and lua_module *.md files
find docs/modules/ docs/lua-modules/ -name "*.md" | sed "sxdocs/xx" | sort > /tmp/files

diff /tmp/doc /tmp/files && echo "all *.md files are reflected in mkdocs.yml"

