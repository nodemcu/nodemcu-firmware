#!/bin/sh

set -e -u

genectls() {
  openssl ecparam -genkey -name "${1}" -out "${2}.key"
  openssl req -new -sha256 -subj "/CN=${1}" -key "${2}.key" -out "${2}.csr"
  openssl req -x509 -sha256 -days 365 -key "${2}.key" -in "${2}.csr" -out "${2}.crt"
}

PFX="${NODEMCU_TESTTMP}/tmp-ec256v1"
[ -r "${PFX}.key" ] || genectls "prime256v1" "${PFX}"

PFX="${NODEMCU_TESTTMP}/tmp-ec384r1"
[ -r "${PFX}.key" ] || genectls "secp384r1" "${PFX}"

PFX="${NODEMCU_TESTTMP}/tmp-rsa2048"
[ -r "${PFX}.key" ] || {
  openssl genrsa -out "${PFX}.key" "2048"
  openssl req -x509 -nodes -subj "/CN=rsa" -key "${PFX}.key" -out "${PFX}.crt" "-days" "365"
}
