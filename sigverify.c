#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/x509.h>

static const char *pubkey = "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAzZOYAJEwa/ODoV9QtDg+1F5uzlH6KHidPYRXu+6dHPmk8iHmx5AbeMwEAOtoutse2OOQ+UkI51dsFhicNOLBwu1EIHDmbtQKDNzRIYuNnhqUAypBxQeFQklzxVAxRjf1ij77pSp5VVQGPCWWqWdohZYsfnVL5hIRiI39xf/hzY56f3RO8j+/BlnvVoCq9q4Re+MytQZbSC3OxO70Z3CK4zNF/GFVv9uWxMid86XYvqcJJlbjqRZkenigo447Lr4e49d2/3Qzkj54mybMyJniT6jDiHnfet+VlKs/Cc2YQy0IfStRB/voK20u2kX6nYGNAVaXV/6HOb0yzPwDyoL5tQIDAQAB";
static const char *applist = "<appList xml:id=\"mlServerAppList\"><app><appID>0x80000000</appID><name>DAPServer</name><allowedProfileIDs>0</allowedProfileIDs><remotingInfo><protocolID>DAP</protocolID><direction>bi</direction></remotingInfo><appInfo><appCategory>0xF0000001</appCategory></appInfo></app><Signature xmlns=\"http://www.w3.org/2000/09/xmldsig#\"><SignedInfo xmlns=\"http://www.w3.org/2000/09/xmldsig#\"><CanonicalizationMethod Algorithm=\"http://www.w3.org/2006/12/xml-c14n11\"></CanonicalizationMethod><SignatureMethod Algorithm=\"http://www.w3.org/2000/09/xmldsig#rsa-sha1\"></SignatureMethod><Reference URI=\"#mlServerAppList\"><Transforms><Transform Algorithm=\"http://www.w3.org/2000/09/xmldsig#enveloped-signature\"></Transform><Transform Algorithm=\"http://www.w3.org/2006/12/xml-c14n11\"></Transform></Transforms><DigestMethod Algorithm=\"http://www.w3.org/2000/09/xmldsig#sha1\"></DigestMethod><DigestValue>8FLxBkpA/v0BVQsdqCMmHtcMfmQ=</DigestValue></Reference></SignedInfo><SignatureValue>mxWcCnXeYNQN1rh68G3H0aq9XCseCHF3Pv5OEqGQs3PaQPYw1mq/ggSrAY/57md0b4TVQKlAOvkriEEfrLjzzIa5gF9F+K8DFTIysft1OvIv2OgtqTosy1sP15Oc1BSUhifOYE3opDx1XTgsNFVg+Tkgyza1vW97tTH0Hz8SQICoSPmLH43S9x4jSoOGuL5uUnK9y/xuSaTB+Capfl/Dlj+6mCGJcaL7ZBDRzD48aZrW0KIbL6hlaSpWDmLIElZfer4uWiEP5lqzrgThsag8gbarDDd2RsNwa+fLPANfBH3cDkeFDT50LUSv4Rv1vP+Nv1DPbS7EtjZ3C7235vJL3w==</SignatureValue></Signature></appList>";

void base64_decode(const char *in, size_t ilen, uint8_t **out, size_t *olen)
{
	BIO *buff, *b64f;
	b64f = BIO_new(BIO_f_base64());
	buff = BIO_new_mem_buf((void *)in, ilen);
	buff = BIO_push(b64f, buff);
	*out = (uint8_t *)malloc(ilen * sizeof(uint8_t));
	BIO_set_flags(buff, BIO_FLAGS_BASE64_NO_NL);
	BIO_set_close(buff, BIO_CLOSE);
	*olen = BIO_read(buff, *out, ilen);
	*out = (uint8_t *)realloc((void *)(*out), (*olen + 1) * sizeof(uint8_t));
	(*out)[*olen] = '\0';
	BIO_free_all(buff);
}

xmlNodePtr xmlFindChildElement(xmlNodePtr parent, xmlChar *name)
{
    xmlNodePtr cur = xmlFirstElementChild(parent);
    for (; cur; cur = cur->next) {
        if (cur->type != XML_ELEMENT_NODE) {
            continue;
        }
        if (!xmlStrcmp(cur->name, name)) {
            return cur;
        }
    }
    return NULL;
}

int get_signature_value(const char *list, char **out)
{
	xmlDocPtr doc = xmlParseDoc(list);
	if (!doc) {
		return -1;
	}
	xmlNodePtr rootElement = xmlDocGetRootElement(doc);
	if (!rootElement) {
		xmlFreeDoc(doc);
		return -1;
	}
	xmlNodePtr signatureElement = xmlFindChildElement(rootElement, "Signature");
	if (!signatureElement) {
		xmlFreeDoc(doc);
		return -1;
	}
	xmlNodePtr signatureValueElement = xmlFindChildElement(signatureElement, "SignatureValue");
	if (!signatureValueElement) {
		xmlFreeDoc(doc);
	}
	xmlChar *ret = xmlNodeGetContent(signatureValueElement);
	if (ret) {
		*out = xmlStrdup(ret);
		xmlFree(ret);
		xmlFreeDoc(doc);
		return 0;
	}
	xmlFreeDoc(doc);
	return -1;
}

int main(int argc, char *argv[])
{
	uint8_t *keyout = NULL;
	size_t keylen = 0;
	char *sigvalue = NULL;
	uint8_t *sigout = NULL;
	size_t siglen = 0;
	EVP_PKEY *pKey = NULL;
	base64_decode(pubkey, strlen(pubkey), &keyout, &keylen);
	if (get_signature_value(applist, &sigvalue)) {
		free(keyout);
		printf("get signature value failed\n");
		return -1;
	}
	base64_decode(sigvalue, strlen(sigvalue), &sigout, &siglen);
	pKey = d2i_PUBKEY(&pKey, (const uint8_t **)&keyout, keylen);
	EVP_PKEY_free(pKey);
	free(sigvalue);
	free(sigout);
	return 0;
}
