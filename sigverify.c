#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/c14n.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/err.h>

#include <string.h>

#include "utils.h"

struct sigverify_ctx {
    xmlDocPtr doc;
    const EVP_MD *digest;
    EVP_PKEY *pkey;
    EVP_MD_CTX *digestCtx;
    int canonicalMode;
    xmlNodePtr canonicalNode;
    xmlNodePtr sigNode;
    xmlNodePtr sigInfoNode;
    xmlNodePtr sigValueNode;
    int status;
    struct decode_buffer dgst;
    struct decode_buffer buffer;
};

static int SigVerifyXMLOutputWriteCallback(void * context, const char * buffer, int len);
static int SigVerifyXMLOutputCloseCallback(void * context);
static int SigVerifyXMLC14NIsVisibleCallback(void* user_data, xmlNodePtr node, xmlNodePtr parent);

static int sigverify_prepare(struct sigverify_ctx *ctx, const unsigned char *data, const unsigned char *pubkey);
static int digest_verify(struct sigverify_ctx *ctx);
static int signature_verify(struct sigverify_ctx *ctx);
static void sigverify_clear(struct sigverify_ctx *ctx);



int sigverify(const unsigned char *data, const unsigned char *pubkey)
{
    struct sigverify_ctx ctx;
    ERR_load_crypto_strings();
    OpenSSL_add_all_algorithms();
    decode_buffer_init(&(ctx.buffer));
    decode_buffer_init(&(ctx.dgst));
    if (-1 == sigverify_prepare(&ctx, data, pubkey)) {
        return -1;
    }
    if (-1 == digest_verify(&ctx)) {
        sigverify_clear(&ctx);
        return -1;
    }
    if (-1 == signature_verify(&ctx)) {
        sigverify_clear(&ctx);
        return -1;
    }
    return 0;
}

static int sigverify_prepare(struct sigverify_ctx *ctx, const unsigned char *data, const unsigned char *pubkey)
{
    xmlDocPtr doc = xmlParseDoc(BAD_CAST data);
    if (!doc) {
        return -1;
    }
    xmlNodePtr rootElement = xmlDocGetRootElement(doc);
    if (!rootElement) {
        xmlFreeDoc(doc);
        return -1;
    }
    xmlNodePtr sigElement = xmlFindChildElement(rootElement, BAD_CAST"Signature");
    if (!sigElement) {
        xmlFreeDoc(doc);
        return -1;
    }
    xmlNodePtr sigInfoElement = xmlFindChildElement(sigElement, BAD_CAST"SignedInfo");
    if (!sigInfoElement) {
        xmlFreeDoc(doc);
        return -1;
    }
    xmlNodePtr referenceElement = xmlFindChildElement(sigInfoElement, BAD_CAST"Reference");
    if (!referenceElement) {
        xmlFreeDoc(doc);
        return -1;
    }
    xmlNodePtr digestValueElement = xmlFindChildElement(referenceElement, BAD_CAST"DigestValue");
    if (!digestValueElement) {
        xmlFreeDoc(doc);
        return -1;
    }
    xmlNodePtr sigValueElement = xmlFindChildElement(sigElement, BAD_CAST"SignatureValue");
    if (!sigValueElement) {
        xmlFreeDoc(doc);
        return -1;
    }
    xmlChar *digestValue = xmlNodeGetContent(digestValueElement);
    if (digestValue) {
        if (-1 == base64_decode(digestValue, xmlStrlen(digestValue), &(ctx->buffer))) {
            xmlFreeDoc(doc);
            xmlFree(digestValue);
            return -1;
        }
        xmlFree(digestValue);
    }
    struct decode_buffer buffer;
    if (-1 == base64_decode(pubkey, strlen((const char *)pubkey), &buffer)) {
        xmlFreeDoc(doc);
        return -1;
    }
    ctx->pkey = d2i_PUBKEY(NULL, (const unsigned char **)&(buffer.data), buffer.len);
    if(ctx->pkey == NULL) {
        xmlFreeDoc(doc);
        return -1;
    }
    ctx->doc = doc;
    ctx->sigNode = sigElement;
    ctx->sigInfoNode = sigInfoElement;
    ctx->sigValueNode = sigValueElement;
    return 0;
}

static void sigverify_clear(struct sigverify_ctx *ctx)
{
    xmlFreeDoc(ctx->doc);
    EVP_PKEY_free(ctx->pkey);
    EVP_MD_CTX_destroy(ctx->digestCtx);
}

static int digest_verify(struct sigverify_ctx *ctx)
{
    xmlOutputBufferPtr buffer;
    ctx->status = 0;
    ctx->canonicalMode = 0;
    ctx->canonicalNode = ctx->sigNode;
    decode_buffer_append(&(ctx->dgst), 64);
    buffer = xmlOutputBufferCreateIO(SigVerifyXMLOutputWriteCallback, SigVerifyXMLOutputCloseCallback, ctx, NULL);
    xmlC14NExecute(ctx->doc, SigVerifyXMLC14NIsVisibleCallback, ctx, XML_C14N_1_1, NULL, 0, buffer);
    xmlOutputBufferClose(buffer);
    if (ctx->dgst.len != ctx->buffer.len) {
        return -1;
    }
    if (memcmp(ctx->buffer.data, ctx->dgst.data, ctx->dgst.len)) {
        return -1;
    }
    decode_buffer_clear(&(ctx->dgst));
    decode_buffer_clear(&(ctx->buffer));
    return 0;
}

static int signature_verify(struct sigverify_ctx *ctx)
{
    xmlOutputBufferPtr buffer;
    int ret = 0;
    xmlChar *sigValue = xmlNodeGetContent(ctx->sigValueNode);
    if (sigValue) {
        if (-1 == base64_decode(sigValue, xmlStrlen(sigValue), &(ctx->dgst))) {
            xmlFree(sigValue);
            return -1;
        }
        xmlFree(sigValue);
    }
    ctx->status = 3;
    ctx->canonicalMode = 1;
    ctx->canonicalNode = ctx->sigInfoNode;
    buffer = xmlOutputBufferCreateIO(SigVerifyXMLOutputWriteCallback, SigVerifyXMLOutputCloseCallback, ctx, NULL);
    xmlC14NExecute(ctx->doc, SigVerifyXMLC14NIsVisibleCallback, ctx, XML_C14N_1_1, NULL, 0, buffer);
    xmlOutputBufferClose(buffer);
    ret = EVP_VerifyFinal(ctx->digestCtx, ctx->dgst.data, ctx->dgst.len, ctx->pkey);
    if (1 == ret) {
        ret = 0;
    }
    else {
        int err;
        do {
            err = ERR_get_error();
            if (err) {
                printf("error is %s\n", ERR_error_string(err, NULL));
                fflush(stdout);
            }
        } while (err != 0);
        ret = -1;
    }
    decode_buffer_clear(&(ctx->dgst));
    return ret;
}

static int SigVerifyXMLOutputWriteCallback(void * context, const char * buffer, int len)
{
    struct sigverify_ctx *ctx = (struct sigverify_ctx *)context;
    if (ctx) {
        switch (ctx->status) {
            case 0:
            {
                ctx->digestCtx = EVP_MD_CTX_create();
                ctx->digest = EVP_sha1();
                EVP_DigestInit(ctx->digestCtx, ctx->digest);
                EVP_DigestUpdate(ctx->digestCtx, buffer, len);
                ctx->status = 1;
                return len;
            }
            break;
            case 1:
            {
                ctx->status = 2;
            }
            break;
            case 3:
            {
                EVP_MD_CTX_destroy(ctx->digestCtx);
                ctx->digestCtx = EVP_MD_CTX_create();
                EVP_VerifyInit(ctx->digestCtx, ctx->digest);
                EVP_VerifyUpdate(ctx->digestCtx, buffer, len);
                ctx->status = 4;
                return len;
            }
            break;
            default:
            break;
        }
    }
    return 0;
}

static int SigVerifyXMLOutputCloseCallback(void * context)
{
    struct sigverify_ctx *ctx = (struct sigverify_ctx *)context;
    if (ctx) {
        switch (ctx->status) {
            case 2:
            {
                EVP_DigestFinal(ctx->digestCtx, ctx->dgst.data, (unsigned int *)&(ctx->dgst.len));
            }
            break;
            default:
            break;
        }
    }
    return 0;
}

static int SigVerifyXMLC14NIsVisibleCallback(void *user_data, xmlNodePtr node, xmlNodePtr parent)
{
    struct sigverify_ctx *ctx = (struct sigverify_ctx *)user_data;
    if (ctx) {
        switch (ctx->canonicalMode) {
            case 0:
            {
                if (node == ctx->canonicalNode) {
                    return 0;
                }
                else {
                    for (xmlNodePtr cur = parent; cur != NULL; cur = cur->parent) {
                        if (cur == ctx->canonicalNode) {
                            return 0;
                        }
                    }
                }
                return 1;
            }
            break;
            case 1:
            {
                if (node == ctx->canonicalNode) {
                    return 1;
                }
                else {
                    for (xmlNodePtr cur = parent; cur != NULL; cur = cur->parent) {
                        if (cur == ctx->canonicalNode) {
                            return 1;
                        }
                    }
                }
                return 0;
            }
            break;
            default:
            break;
        }
    }
    return 1;
}


