#!/bin/bash

echo "Installing IDF prerequisites..."

IDF_DIR=./sdk/esp32-esp-idf

${IDF_DIR}/install.sh
. ${IDF_DIR}/export.sh

echo "Installing NodeMCU prerequisites..."

export PATH=${IDF_PYTHON_ENV_PATH}:${PATH}
pip install -r requirements.txt

cat << EOF

Installation of prerequisites complete.
EOF
