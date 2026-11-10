// pdtlscheck - connect to a TLS server and judge its certificate as SecTrust does
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// the handshake breaks on server authentication as Qt does, then prints the verdict, protocol, cipher
// and the server's first HTTP response line
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include "../apple/OSX/libsecurity_ssl/lib/SecureTransport.h"
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

extern void trustd_init(CFURLRef home_path);

static void printError(const char *what, CFErrorRef error);

// the trust engine's own view of the system store, reachable with -i where it runs in this process
typedef struct _OpaqueSecOTAPKI *SecOTAPKIRef;
extern SecOTAPKIRef SecOTAPKICopyCurrentOTAPKIRef(void);
extern CFDictionaryRef SecOTAPKICopyAnchorLookupTable(SecOTAPKIRef otapkiRef);
extern CFDataRef SecCertificateGetNormalizedIssuerContent(SecCertificateRef certificate);
extern unsigned char *CC_SHA1(const void *data, uint32_t len, unsigned char *md);

// how many root names the store indexes, and whether each certificate's issuer is among them
static void inspectStore(SecTrustRef trust)
{
    SecOTAPKIRef otapki = SecOTAPKICopyCurrentOTAPKIRef();
    CFDictionaryRef table = otapki ? SecOTAPKICopyAnchorLookupTable(otapki) : NULL;

    if (table == NULL) {
        static const char *const resources[][2] = {
            { "Blocked", "plist" }, { "GrayListedKeys", "plist" }, { "EVRoots", "plist" },
            { "AppleESCertificates", "plist" }, { "AssetVersion", "plist" }, { "certsIndex", "data" },
            { "certsTable", "data" },
        };
        CFURLRef url = CFURLCreateWithFileSystemPath(NULL, CFSTR("/System/Library/Security/Certificates.bundle"),
            kCFURLPOSIXPathStyle, true);
        CFBundleRef bundle = CFBundleCreate(NULL, url);

        // the same lookups trustd makes, to name the one that fails
        printf("store: not loaded, bundle %s\n", bundle ? "opens" : "does not open");
        for (unsigned i = 0; bundle && i < sizeof(resources) / sizeof(resources[0]); i++) {
            CFStringRef name = CFStringCreateWithCString(NULL, resources[i][0], kCFStringEncodingUTF8);
            CFStringRef type = CFStringCreateWithCString(NULL, resources[i][1], kCFStringEncodingUTF8);
            CFURLRef resource = CFBundleCopyResourceURL(bundle, name, type, NULL);

            printf("  %s.%s %s", resources[i][0], resources[i][1], resource ? "found" : "missing");
            // plists are parsed as trustd parses them, a parse failure there leaves the store unloaded
            if (resource && strcmp(resources[i][1], "plist") == 0) {
                CFReadStreamRef stream = CFReadStreamCreateWithFile(NULL, resource);
                CFErrorRef error = NULL;
                CFPropertyListRef plist = NULL;

                if (stream && CFReadStreamOpen(stream))
                    plist = CFPropertyListCreateWithStream(NULL, stream, 0, kCFPropertyListImmutable, NULL, &error);
                if (plist)
                    printf(", parses as %s", CFGetTypeID(plist) == CFArrayGetTypeID() ? "array" :
                        CFGetTypeID(plist) == CFDictionaryGetTypeID() ? "dictionary" : "other");
                else
                    printError(", does not parse", error);
                if (plist)
                    CFRelease(plist);
                if (error)
                    CFRelease(error);
                if (stream) {
                    CFReadStreamClose(stream);
                    CFRelease(stream);
                }
            }
            printf("\n");
            if (resource)
                CFRelease(resource);
            CFRelease(name);
            CFRelease(type);
        }
        if (bundle)
            CFRelease(bundle);
        CFRelease(url);
        return;
    }
    printf("store: %ld root names\n", (long)CFDictionaryGetCount(table));
    for (CFIndex i = 0; i < SecTrustGetCertificateCount(trust); i++) {
        SecCertificateRef certificate = SecTrustGetCertificateAtIndex(trust, i);
        CFDataRef issuer = SecCertificateGetNormalizedIssuerContent(certificate);
        unsigned char digest[20];
        CFDataRef key;

        if (issuer == NULL)
            continue;
        CC_SHA1(CFDataGetBytePtr(issuer), (uint32_t)CFDataGetLength(issuer), digest);
        key = CFDataCreate(NULL, digest, sizeof(digest));
        printf("cert %ld issuer ", (long)i);
        for (int b = 0; b < 20; b++)
            printf("%02x", digest[b]);
        printf(" %s\n", CFDictionaryContainsKey(table, key) ? "in store" : "not in store");
        CFRelease(key);
    }
    CFRelease(table);
    CFRelease(otapki);
}

static OSStatus readSocket(SSLConnectionRef connection, void *data, size_t *length)
{
    int fd = *(const int *)connection;
    size_t wanted = *length, got = 0;

    while (got < wanted) {
        ssize_t n = read(fd, (char *)data + got, wanted - got);

        if (n == 0) {
            *length = got;
            return errSSLClosedGraceful;
        }
        if (n < 0) {
            if (errno == EINTR)
                continue;
            *length = got;
            return errSecIO;
        }
        got += n;
    }
    *length = got;
    return noErr;
}

static OSStatus writeSocket(SSLConnectionRef connection, const void *data, size_t *length)
{
    int fd = *(const int *)connection;
    size_t wanted = *length, sent = 0;

    while (sent < wanted) {
        ssize_t n = write(fd, (const char *)data + sent, wanted - sent);

        if (n < 0) {
            if (errno == EINTR)
                continue;
            *length = sent;
            return errSecIO;
        }
        sent += n;
    }
    *length = sent;
    return noErr;
}

static int connectTo(const char *host, const char *port)
{
    struct addrinfo hints = { .ai_socktype = SOCK_STREAM }, *list, *ai;
    int fd = -1, error = getaddrinfo(host, port, &hints, &list);

    if (error) {
        fprintf(stderr, "pdtlscheck: %s: %s\n", host, gai_strerror(error));
        return -1;
    }
    for (ai = list; ai; ai = ai->ai_next) {
        fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd >= 0 && connect(fd, ai->ai_addr, ai->ai_addrlen) == 0)
            break;
        if (fd >= 0)
            close(fd);
        fd = -1;
    }
    freeaddrinfo(list);
    if (fd < 0)
        fprintf(stderr, "pdtlscheck: cannot connect to %s:%s\n", host, port);
    return fd;
}

static void printError(const char *what, CFErrorRef error)
{
    CFStringRef description = error ? CFErrorCopyDescription(error) : NULL;
    char text[1024] = "";

    if (description) {
        CFStringGetCString(description, text, sizeof(text), kCFStringEncodingUTF8);
        CFRelease(description);
    }
    printf("%s: %s\n", what, text);
}

int main(int argc, char *argv[])
{
    int inProcess = argc > 1 && strcmp(argv[1], "-i") == 0;
    const char *host = argc > 1 + inProcess ? argv[1 + inProcess] : NULL;
    const char *port = argc > 2 + inProcess ? argv[2 + inProcess] : "443";
    SSLContextRef context;
    OSStatus status;
    int fd, trusted = 0;

    if (host == NULL) {
        fprintf(stderr, "usage: pdtlscheck [-i] host [port]\n");
        return 2;
    }
    if (inProcess)
        trustd_init(NULL);
    fd = connectTo(host, port);
    if (fd < 0)
        return 1;
    context = SSLCreateContext(NULL, kSSLClientSide, kSSLStreamType);
    SSLSetIOFuncs(context, readSocket, writeSocket);
    SSLSetConnection(context, &fd);
    SSLSetPeerDomainName(context, host, strlen(host));
    SSLSetSessionOption(context, kSSLSessionOptionBreakOnServerAuth, true);

    while ((status = SSLHandshake(context)) == errSSLPeerAuthCompleted || status == errSSLWouldBlock) {
        SecTrustRef trust = NULL;
        CFErrorRef error = NULL;

        if (status == errSSLWouldBlock)
            continue;
        SSLCopyPeerTrust(context, &trust);
        if (trust == NULL) {
            printf("trust: no peer certificates\n");
            break;
        }
        printf("chain: %ld certificates\n", (long)SecTrustGetCertificateCount(trust));
        if (inProcess)
            inspectStore(trust);
        trusted = SecTrustEvaluateWithError(trust, &error);
        if (trusted)
            printf("trust: trusted\n");
        else
            printError("trust: not trusted", error);
        if (error)
            CFRelease(error);
        CFRelease(trust);
        if (!trusted)
            break;
    }
    if (trusted && status == noErr) {
        SSLProtocol protocol = kSSLProtocolUnknown;
        SSLCipherSuite cipher = 0;
        char request[512], response[512];
        size_t moved = 0;

        SSLGetNegotiatedProtocolVersion(context, &protocol);
        SSLGetNegotiatedCipher(context, &cipher);
        printf("protocol %d cipher 0x%04x\n", protocol, cipher);
        snprintf(request, sizeof(request), "HEAD / HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", host);
        SSLWrite(context, request, strlen(request), &moved);
        moved = 0;
        if (SSLRead(context, response, sizeof(response) - 1, &moved) == noErr || moved > 0) {
            response[moved] = 0;
            response[strcspn(response, "\r\n")] = 0;
            printf("response: %s\n", response);
        }
        SSLClose(context);
    } else if (status != noErr && status != errSSLPeerAuthCompleted)
        printf("handshake: %d\n", (int)status);
    CFRelease(context);
    close(fd);
    return trusted && status == noErr ? 0 : 1;
}
