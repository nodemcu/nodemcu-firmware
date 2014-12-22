
#
# Generate the certificates and keys for encrypt.
#

# set default cert for use in the client
xxd -i client.cer | sed -e \
        "s/client_cer/default_certificate/" > cert.h
# set default key for use in the server
xxd -i server.key_1024 | sed -e \
        "s/server_key_1024/default_private_key/" > private_key.h
