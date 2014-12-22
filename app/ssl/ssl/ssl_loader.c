/*
 * Copyright (c) 2007, Cameron Rich
 * 
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, 
 *   this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright notice, 
 *   this list of conditions and the following disclaimer in the documentation 
 *   and/or other materials provided with the distribution.
 * * Neither the name of the axTLS project nor the names of its contributors 
 *   may be used to endorse or promote products derived from this software 
 *   without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * Load certificates/keys into memory. These can be in many different formats.
 * PEM support and other formats can be processed here.
 *
 * The PEM private keys may be optionally encrypted with AES128 or AES256. 
 * The encrypted PEM keys were generated with something like:
 *
 * openssl genrsa -aes128 -passout pass:abcd -out axTLS.key_aes128.pem 512
 */

//#include <stdlib.h>
//#include <string.h>
//#include <stdio.h>
#include "ssl/ssl_os_port.h"
#include "ssl/ssl_ssl.h"

static int do_obj(SSL_CTX *ssl_ctx, int obj_type, 
                    SSLObjLoader *ssl_obj, const char *password);
#ifdef CONFIG_SSL_HAS_PEM
static int ssl_obj_PEM_load(SSL_CTX *ssl_ctx, int obj_type, 
                        SSLObjLoader *ssl_obj, const char *password);
#endif

/*
 * Load a file into memory that is in binary DER (or ascii PEM) format.
 */
EXP_FUNC int STDCALL ICACHE_FLASH_ATTR ssl_obj_load(SSL_CTX *ssl_ctx, int obj_type, 
                            const char *filename, const char *password)
{
#ifndef CONFIG_SSL_SKELETON_MODE
    static const char * const begin = "-----BEGIN";
    int ret = SSL_OK;
    SSLObjLoader *ssl_obj = NULL;

    if (filename == NULL)
    {
        ret = SSL_ERROR_INVALID_KEY;
        goto error;
    }

    ssl_obj = (SSLObjLoader *)os_zalloc(sizeof(SSLObjLoader));
    ssl_obj->len = get_file(filename, &ssl_obj->buf); 
    if (ssl_obj->len <= 0)
    {
        ret = SSL_ERROR_INVALID_KEY;
        goto error;
    }

    /* is the file a PEM file? */
    if ((char *)os_strstr((const char *)ssl_obj->buf, begin) != NULL)
    {
#ifdef CONFIG_SSL_HAS_PEM
        ret = ssl_obj_PEM_load(ssl_ctx, obj_type, ssl_obj, password);
#else
        ssl_printf(unsupported_str);
        ret = SSL_ERROR_NOT_SUPPORTED;
#endif
    }
    else
        ret = do_obj(ssl_ctx, obj_type, ssl_obj, password);

error:
    ssl_obj_free(ssl_obj);
    return ret;
#else
    ssl_printf(unsupported_str);
    return SSL_ERROR_NOT_SUPPORTED;
#endif /* CONFIG_SSL_SKELETON_MODE */
}

/*
 * Transfer binary data into the object loader.
 */
EXP_FUNC int STDCALL ICACHE_FLASH_ATTR ssl_obj_memory_load(SSL_CTX *ssl_ctx, int mem_type, 
        const uint8_t *data, int len, const char *password)
{
    int ret;
    SSLObjLoader *ssl_obj;

    ssl_obj = (SSLObjLoader *)os_zalloc(sizeof(SSLObjLoader));
    ssl_obj->buf = (uint8_t *)os_malloc(len);
    os_memcpy(ssl_obj->buf, data, len);
    ssl_obj->len = len;
    ret = do_obj(ssl_ctx, mem_type, ssl_obj, password);
    ssl_obj_free(ssl_obj);
    return ret;
}

/*
 * Actually work out what we are doing 
 */
static int ICACHE_FLASH_ATTR do_obj(SSL_CTX *ssl_ctx, int obj_type, 
                    SSLObjLoader *ssl_obj, const char *password)
{
    int ret = SSL_OK;

    switch (obj_type)
    {
        case SSL_OBJ_RSA_KEY:
            ret = add_private_key(ssl_ctx, ssl_obj);
            break;

        case SSL_OBJ_X509_CERT:
            ret = add_cert(ssl_ctx, ssl_obj->buf, ssl_obj->len);
            break;

#ifdef CONFIG_SSL_CERT_VERIFICATION
        case SSL_OBJ_X509_CACERT:
            add_cert_auth(ssl_ctx, ssl_obj->buf, ssl_obj->len);
            break;
#endif

#ifdef CONFIG_SSL_USE_PKCS12
        case SSL_OBJ_PKCS8:
            ret = pkcs8_decode(ssl_ctx, ssl_obj, password);
            break;

        case SSL_OBJ_PKCS12:
            ret = pkcs12_decode(ssl_ctx, ssl_obj, password);
            break;
#endif
        default:
            ssl_printf(unsupported_str);
            ret = SSL_ERROR_NOT_SUPPORTED;
            break;
    }

    return ret;
}

/*
 * Clean up our mess.
 */
void ICACHE_FLASH_ATTR ssl_obj_free(SSLObjLoader *ssl_obj)
{
    if (ssl_obj)
    {
        os_free(ssl_obj->buf);
        os_free(ssl_obj);
    }
}

/*
 * Support for PEM encoded keys/certificates.
 */
#ifdef CONFIG_SSL_HAS_PEM

#define NUM_PEM_TYPES               4
#define IV_SIZE                     16
#define IS_RSA_PRIVATE_KEY          0
#define IS_ENCRYPTED_PRIVATE_KEY    1
#define IS_PRIVATE_KEY              2
#define IS_CERTIFICATE              3

static const char * const begins[NUM_PEM_TYPES] =
{
    "-----BEGIN RSA PRIVATE KEY-----",
    "-----BEGIN ENCRYPTED PRIVATE KEY-----",
    "-----BEGIN PRIVATE KEY-----",
    "-----BEGIN CERTIFICATE-----",
};

static const char * const ends[NUM_PEM_TYPES] =
{
    "-----END RSA PRIVATE KEY-----",
    "-----END ENCRYPTED PRIVATE KEY-----",
    "-----END PRIVATE KEY-----",
    "-----END CERTIFICATE-----",
};

static const char * const aes_str[2] =
{
    "DEK-Info: AES-128-CBC,",
    "DEK-Info: AES-256-CBC," 
};

/**
 * Take a base64 blob of data and decrypt it (using AES) into its 
 * proper ASN.1 form.
 */
static int ICACHE_FLASH_ATTR pem_decrypt(const char *where, const char *end,
                        const char *password, SSLObjLoader *ssl_obj)
{
    int ret = -1;
    int is_aes_256 = 0;
    char *start = NULL;
    uint8_t iv[IV_SIZE];
    int i, pem_size;
    MD5_CTX md5_ctx;
    AES_CTX aes_ctx;
    uint8_t key[32];        /* AES256 size */

    if (password == NULL || os_strlen(password) == 0)
    {
#ifdef CONFIG_SSL_FULL_MODE
        ssl_printf("Error: Need a password for this PEM file\n"); //TTY_FLUSH();
#endif
        goto error;
    }

    if ((start = (char *)os_strstr((const char *)where, aes_str[0])))         /* AES128? */
    {
        start += os_strlen(aes_str[0]);
    }
    else if ((start = (char *)os_strstr((const char *)where, aes_str[1])))    /* AES256? */
    {
        is_aes_256 = 1;
        start += os_strlen(aes_str[1]);
    }
    else 
    {
#ifdef CONFIG_SSL_FULL_MODE
        ssl_printf("Error: Unsupported password cipher\n"); //TTY_FLUSH();
#endif
        goto error;
    }

    /* convert from hex to binary - assumes uppercase hex */
    for (i = 0; i < IV_SIZE; i++)
    {
        char c = *start++ - '0';
        iv[i] = (c > 9 ? c + '0' - 'A' + 10 : c) << 4;
        c = *start++ - '0';
        iv[i] += (c > 9 ? c + '0' - 'A' + 10 : c);
    }

    while (*start == '\r' || *start == '\n')
        start++;

    /* turn base64 into binary */
    pem_size = (int)(end-start);
    if (base64_decode(start, pem_size, ssl_obj->buf, &ssl_obj->len) != 0)
        goto error;

    /* work out the key */
    MD5_Init(&md5_ctx);
    MD5_Update(&md5_ctx, (const uint8_t *)password, os_strlen(password));
    MD5_Update(&md5_ctx, iv, SALT_SIZE);
    MD5_Final(key, &md5_ctx);

    if (is_aes_256)
    {
        MD5_Init(&md5_ctx);
        MD5_Update(&md5_ctx, key, MD5_SIZE);
        MD5_Update(&md5_ctx, (const uint8_t *)password, os_strlen(password));
        MD5_Update(&md5_ctx, iv, SALT_SIZE);
        MD5_Final(&key[MD5_SIZE], &md5_ctx);
    }

    /* decrypt using the key/iv */
    AES_set_key(&aes_ctx, key, iv, is_aes_256 ? AES_MODE_256 : AES_MODE_128);
    AES_convert_key(&aes_ctx);
    AES_cbc_decrypt(&aes_ctx, ssl_obj->buf, ssl_obj->buf, ssl_obj->len);
    ret = 0;

error:
    return ret; 
}

/**
 * Take a base64 blob of data and turn it into its proper ASN.1 form.
 */
static int ICACHE_FLASH_ATTR new_pem_obj(SSL_CTX *ssl_ctx, int is_cacert, char *where, 
                    int remain, const char *password)
{
    int ret = SSL_ERROR_BAD_CERTIFICATE;
    SSLObjLoader *ssl_obj = NULL;

    while (remain > 0)
    {
        int i, pem_size, obj_type;
        char *start = NULL, *end = NULL;

        for (i = 0; i < NUM_PEM_TYPES; i++)
        {
            if ((start = (char *)os_strstr(where, begins[i])) &&
                    (end = (char *)os_strstr(where, ends[i])))
            {
                remain -= (int)(end-where);
                start += os_strlen(begins[i]);
                pem_size = (int)(end-start);

                ssl_obj = (SSLObjLoader *)os_zalloc(sizeof(SSLObjLoader));

                /* 4/3 bigger than what we need but so what */
                ssl_obj->buf = (uint8_t *)os_zalloc(pem_size);
                ssl_obj->len = pem_size;

                if (i == IS_RSA_PRIVATE_KEY && 
                            os_strstr(start, "Proc-Type:") && 
                            os_strstr(start, "4,ENCRYPTED"))
                {
                    /* check for encrypted PEM file */
                    if (pem_decrypt(start, end, password, ssl_obj) < 0)
                    {
                        ret = SSL_ERROR_BAD_CERTIFICATE;
                        goto error;
                    }
                }
                else 
                {
                    ssl_obj->len = pem_size;
                    if (base64_decode(start, pem_size, 
                                ssl_obj->buf, &ssl_obj->len) != 0)
                    {
                        ret = SSL_ERROR_BAD_CERTIFICATE;
                        goto error;
                    }
                }

                switch (i)
                {
                    case IS_RSA_PRIVATE_KEY:
                        obj_type = SSL_OBJ_RSA_KEY;
                        break;

                    case IS_ENCRYPTED_PRIVATE_KEY:
                    case IS_PRIVATE_KEY:
                        obj_type = SSL_OBJ_PKCS8;
                        break;

                    case IS_CERTIFICATE:
                        obj_type = is_cacert ?
                                        SSL_OBJ_X509_CACERT : SSL_OBJ_X509_CERT;
                        break;

                    default:
                        ret = SSL_ERROR_BAD_CERTIFICATE;
                        goto error;
                }

                /* In a format we can now understand - so process it */
                if ((ret = do_obj(ssl_ctx, obj_type, ssl_obj, password)))
                    goto error;

                end += os_strlen(ends[i]);
                remain -= os_strlen(ends[i]);
                while (remain > 0 && (*end == '\r' || *end == '\n'))
                {
                    end++;
                    remain--;
                }

                where = end;
                break;
            }
        }

        ssl_obj_free(ssl_obj);
        ssl_obj = NULL;
        if (start == NULL)
           break;
    }
error:
    ssl_obj_free(ssl_obj);
    return ret;
}

/*
 * Load a file into memory that is in ASCII PEM format.
 */
static int ICACHE_FLASH_ATTR ssl_obj_PEM_load(SSL_CTX *ssl_ctx, int obj_type, 
                        SSLObjLoader *ssl_obj, const char *password)
{
    char *start;

    /* add a null terminator */
    ssl_obj->len++;
    ssl_obj->buf = (uint8_t *)os_realloc(ssl_obj->buf, ssl_obj->len);
    ssl_obj->buf[ssl_obj->len-1] = 0;
    start = (char *)ssl_obj->buf;
    return new_pem_obj(ssl_ctx, obj_type == SSL_OBJ_X509_CACERT,
                                start, ssl_obj->len, password);
}
#endif /* CONFIG_SSL_HAS_PEM */

/**
 * Load the key/certificates in memory depending on compile-time and user
 * options. 
 */
int ICACHE_FLASH_ATTR load_key_certs(SSL_CTX *ssl_ctx)
{
    int ret = SSL_OK;
    uint32_t options = ssl_ctx->options;
#ifdef CONFIG_SSL_GENERATE_X509_CERT 
    uint8_t *cert_data = NULL;
    int cert_size;
    static const char *dn[] = 
    {
        CONFIG_SSL_X509_COMMON_NAME,
        CONFIG_SSL_X509_ORGANIZATION_NAME,
        CONFIG_SSL_X509_ORGANIZATION_UNIT_NAME
    };
#endif

    /* do the private key first */
    if (os_strlen(CONFIG_SSL_PRIVATE_KEY_LOCATION) > 0)
    {
        if ((ret = ssl_obj_load(ssl_ctx, SSL_OBJ_RSA_KEY, 
                                CONFIG_SSL_PRIVATE_KEY_LOCATION,
                                CONFIG_SSL_PRIVATE_KEY_PASSWORD)) < 0)
            goto error;
    }
    else if (!(options & SSL_NO_DEFAULT_KEY))
    {
#if defined(CONFIG_SSL_USE_DEFAULT_KEY) || defined(CONFIG_SSL_SKELETON_MODE)
 //       static const    /* saves a few more bytes */
//#include "private_key.h"
		
		extern unsigned int default_private_key_len;
		extern unsigned char default_private_key[];
        ssl_obj_memory_load(ssl_ctx, SSL_OBJ_RSA_KEY, default_private_key,
                default_private_key_len, NULL); 
#endif
    }

    /* now load the certificate */
#ifdef CONFIG_SSL_GENERATE_X509_CERT 
    if ((cert_size = ssl_x509_create(ssl_ctx, 0, dn, &cert_data)) < 0)
    {
        ret = cert_size;
        goto error;
    }

    ssl_obj_memory_load(ssl_ctx, SSL_OBJ_X509_CERT, cert_data, cert_size, NULL);
    os_free(cert_data);
#else
    if (os_strlen(CONFIG_SSL_X509_CERT_LOCATION))
    {
        if ((ret = ssl_obj_load(ssl_ctx, SSL_OBJ_X509_CERT, 
                                CONFIG_SSL_X509_CERT_LOCATION, NULL)) < 0)
            goto error;
    }
    else if (!(options & SSL_NO_DEFAULT_KEY))
    {
#if defined(CONFIG_SSL_USE_DEFAULT_KEY) || defined(CONFIG_SSL_SKELETON_MODE)
//        static const    /* saves a few bytes and RAM */
//#include "cert.h"
		extern unsigned char default_certificate[];
		extern unsigned int default_certificate_len;
        ssl_obj_memory_load(ssl_ctx, SSL_OBJ_X509_CERT, 
                    default_certificate, default_certificate_len, NULL);
#endif
    }
#endif

error:
#ifdef CONFIG_SSL_FULL_MODE
    if (ret)
    {
        ssl_printf("Error: Certificate or key not loaded\n"); //TTY_FLUSH();
    }
#endif

    return ret;

}
