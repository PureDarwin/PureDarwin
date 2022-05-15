/*	UNCUserNotification.c
	Copyright 2000, Apple Computer, Inc. All rights reserved.
*/

#if 0

#include "UNCUserNotification.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <mach/mach.h>
#include <mach/error.h>
#include <servers/bootstrap.h>
#include <limits.h>
#include <errno.h>

#define MAX_STRING_LENGTH PATH_MAX
#define MAX_STRING_COUNT 16
#define MAX_PORT_NAME_LENGTH 63
#define NOTIFICATION_PORT_NAME "com.apple.UNCUserNotification"
#define NOTIFICATION_PORT_NAME_OLD "UNCUserNotification"
#define NOTIFICATION_PORT_NAME_SUFFIX ".session."
#define MESSAGE_TIMEOUT 100

enum {
    kUNCCancelFlag = (1 << 3),
    kUNCUpdateFlag = (1 << 4)
};

/* backward compatibility */
extern const char kUNCTextFieldLabelsKey[];
extern const char kUNCCheckBoxLabelsKey[];

/* forward compatibility */
extern const char kUNCSessionIDKey[];

const char kUNCTokenKey[] = "Token";
const char kUNCTimeoutKey[] = "Timeout";
const char kUNCAlertSourceKey[] = "AlertSource";
const char kUNCIconPathKey[] = "IconPath";
const char kUNCSoundPathKey[] = "SoundPath";
const char kUNCLocalizationPathKey[] = "LocalizationPath";
const char kUNCAlertHeaderKey[] = "AlertHeader";
const char kUNCAlertMessageKey[] = "AlertMessage";
const char kUNCDefaultButtonTitleKey[] = "DefaultButtonTitle";
const char kUNCAlternateButtonTitleKey[] = "AlternateButtonTitle";
const char kUNCOtherButtonTitleKey[] = "OtherButtonTitle";
const char kUNCProgressIndicatorValueKey[] = "ProgressIndicatorValue";
const char kUNCSessionIDKey[] = "SessionID";
const char kUNCPopUpTitlesKey[] = "PopUpTitles";
const char kUNCTextFieldTitlesKey[] = "TextFieldTitles";
const char kUNCTextFieldLabelsKey[] = "TextFieldTitles";
const char kUNCCheckBoxTitlesKey[] = "CheckBoxTitles";
const char kUNCCheckBoxLabelsKey[] = "CheckBoxTitles";
const char kUNCTextFieldValuesKey[] = "TextFieldValues";
const char kUNCPopUpSelectionKey[] = "PopUpSelection";

static const char *kUNCXMLPrologue = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";
static const char *kUNCDoctypePrologue = "<!DOCTYPE plist SYSTEM \"file://localhost/System/Library/DTDs/PropertyList.dtd\">";
static const char *kUNCPlistPrologue = "<plist version=\"0.9\">";
static const char *kUNCDictionaryPrologue = "<dict>";
static const char *kUNCDictionaryEpilogue = "</dict>";
static const char *kUNCArrayPrologue = "<array>";
static const char *kUNCArrayEpilogue = "</array>";
static const char *kUNCKeyPrologue = "<key>";
static const char *kUNCKeyEpilogue = "</key>";
static const char *kUNCStringPrologue = "<string>";
static const char *kUNCStringEpilogue = "</string>";
static const char *kUNCIntegerPrologue = "<integer>";
static const char *kUNCIntegerEpilogue = "</integer>";
static const char *kUNCPlistEpilogue = "</plist>";

struct __UNCUserNotification {
    mach_port_t _replyPort;
    int _token;
    double _timeout;
    unsigned long _requestFlags;
    unsigned long _responseFlags;
    char *_sessionID;
    mach_msg_base_t *_response;
    char **_responseContents;
};

static unsigned long UNCPackContents(char *buffer, const char **contents, int token, int itimeout, char *source) {
    // if buffer is non-null, write XML into it; if buffer is null, return required size
    // should consider escape sequences
    unsigned long keyLen, valLen;
    const char **p = contents, *key = NULL, *val = NULL, *nextKey = NULL, *previousKey = NULL; 
    char *b = buffer, tokenString[64], timeoutString[64];

    snprintf(tokenString, sizeof(tokenString)-1, "%d", token); tokenString[sizeof(tokenString)-1] = '\0';
    snprintf(timeoutString, sizeof(timeoutString)-1, "%d", itimeout); timeoutString[sizeof(timeoutString)-1] = '\0';
#define APPEND(x) {if (buffer) strcpy(b, x); b += strlen(x);}
#define APPENDN(x, n) {if (buffer) strncpy(b, x, n); b += n;}
    APPEND(kUNCXMLPrologue); APPEND("\n");
    APPEND(kUNCDoctypePrologue); APPEND("\n");
    APPEND(kUNCPlistPrologue); APPEND("\n");
    APPEND(kUNCDictionaryPrologue); APPEND("\n");
    APPEND("\t"); APPEND(kUNCKeyPrologue); APPEND(kUNCTokenKey); APPEND(kUNCKeyEpilogue); APPEND("\n");
    APPEND("\t"); APPEND(kUNCIntegerPrologue); APPEND(tokenString); APPEND(kUNCIntegerEpilogue); APPEND("\n");
    APPEND("\t"); APPEND(kUNCKeyPrologue); APPEND(kUNCTimeoutKey); APPEND(kUNCKeyEpilogue); APPEND("\n");
    APPEND("\t"); APPEND(kUNCIntegerPrologue); APPEND(timeoutString); APPEND(kUNCIntegerEpilogue); APPEND("\n");
    APPEND("\t"); APPEND(kUNCKeyPrologue); APPEND(kUNCAlertSourceKey); APPEND(kUNCKeyEpilogue); APPEND("\n");
    APPEND("\t"); APPEND(kUNCStringPrologue); APPEND(source); APPEND(kUNCStringEpilogue); APPEND("\n");
    
    while (p && *p) {
        key = *p++; val = *p++;
        if (val) {
            nextKey = *p;
            keyLen = strlen(key); if (keyLen > MAX_STRING_LENGTH) keyLen = MAX_STRING_LENGTH;
            valLen = strlen(val); if (valLen > MAX_STRING_LENGTH) valLen = MAX_STRING_LENGTH;
            if (key != previousKey) {
                APPEND("\t"); APPEND(kUNCKeyPrologue);
                APPENDN(key, keyLen);
                APPEND(kUNCKeyEpilogue); APPEND("\n");
            }
            if ((key != previousKey) && (key == nextKey)) {
                APPEND("\t"); APPEND(kUNCArrayPrologue); APPEND("\n");
            }
            if ((key == previousKey) || (key == nextKey)) APPEND("\t");
            APPEND("\t"); APPEND(kUNCStringPrologue);
            APPENDN(val, valLen);
            APPEND(kUNCStringEpilogue); APPEND("\n");
            if ((key == previousKey) && (key != nextKey)) {
                APPEND("\t"); APPEND(kUNCArrayEpilogue); APPEND("\n");
            }
            previousKey = key;
        }
    }
    
    APPEND(kUNCDictionaryEpilogue); APPEND("\n");
    APPEND(kUNCPlistEpilogue); APPEND("\n");
#undef APPEND
#undef APPENDN
    
    return b - buffer;
}

static void convertEscapes(char *str) {
    char *p = str, *q = str;
    for (p = q = str; 0 != (*q = *p); p++, q++) {
        if ('&' == *p) {
            if ('g' == *(p+1) && 't' == *(p+2) && ';' == *(p+3)) {
                *q = '>';
                p += 3;
            } else if ('l' == *(p+1) && 't' == *(p+2) && ';' == *(p+3)) {
                *q = '<';
                p += 3;
            } else if ('a' == *(p+1) && 'm' == *(p+2) && 'p' == *(p+3) && ';' == *(p+4)) {
                *q = '&';
                p += 4;
            }
        }
    }
}

static unsigned long UNCUnpackContents(char *buffer, char **contents) {
    // if contents is non-null, unpack XML buffer into it; if contents is null, return required size
    // if contents is non-null, as side effect, insert null string terminators in buffer and convert some escapes in place
    char **p = contents, *key = NULL, *keyEnd = NULL, *previousKey = NULL, *value = NULL, *valueEnd = NULL, *b = buffer;
    
    while (b) {
        key = strstr(b, kUNCKeyPrologue); if (key) key += strlen(kUNCKeyPrologue);
        value = strstr(b, kUNCStringPrologue); if (value) value += strlen(kUNCStringPrologue);
        b = NULL;
        if (key || (previousKey && value)) {
            if (contents) *p = NULL;
            if (0 != key && key < value) {
                b = key;
                keyEnd = strstr(b, kUNCKeyEpilogue);
                if (keyEnd) {
                    if (contents) {
                        *p = key;
                        *keyEnd = '\0';
                    }
                    previousKey = key;
                    b = keyEnd + strlen(kUNCKeyEpilogue);
                    value = strstr(b, kUNCStringPrologue); if (value) value += strlen(kUNCStringPrologue);
                }
            } else {
                if (contents) *p = previousKey;
            }
            p++;

            if (contents) *p = NULL;
            if (0 != value) {
                b = value;
                valueEnd = strstr(b, kUNCStringEpilogue);
                if (valueEnd) {
                    if (contents) {
                        *p = value;
                        *valueEnd = '\0';
                        convertEscapes(value);
                    }
                    b = valueEnd + strlen(kUNCStringEpilogue);
                }
            }
            p++;
        }
    }
    if (p > contents) {
        if (contents) *p = NULL;
        p++;
    }
    
    return p - contents;
}

static const char *UNCSessionIDForContents(const char **contents) {
    const char **p = contents, *retval = NULL; 
    
    while (!retval && p && *p) {
        if (0 == strcmp(*p++, kUNCSessionIDKey)) retval = *p;
        p++;
    }
    return retval;
}

extern char ***_NSGetArgv(void);

static int UNCSendRequest(const char *sessionID, mach_port_t replyPort, int token, double timeout, unsigned long flags, const char **contents) {
    int retval = ERR_SUCCESS, itimeout = (timeout > 0.0 && timeout < INT_MAX) ? (int)timeout : 0;
    mach_msg_base_t *msg = NULL;
    mach_port_t bootstrapPort = MACH_PORT_NULL, serverPort = MACH_PORT_NULL;
    unsigned long size;
    char namebuffer[MAX_PORT_NAME_LENGTH + 1], oldnamebuffer[MAX_PORT_NAME_LENGTH + 1], *source = (*_NSGetArgv())[0], *p = source;
    
    strcpy(namebuffer, NOTIFICATION_PORT_NAME);
    strcpy(oldnamebuffer, NOTIFICATION_PORT_NAME_OLD);
    if (sessionID) {
        strcat(namebuffer, NOTIFICATION_PORT_NAME_SUFFIX);
        strncat(namebuffer, sessionID, MAX_PORT_NAME_LENGTH - sizeof(NOTIFICATION_PORT_NAME) - sizeof(NOTIFICATION_PORT_NAME_SUFFIX));
        namebuffer[MAX_PORT_NAME_LENGTH] = '\0';
        
        strcat(oldnamebuffer, NOTIFICATION_PORT_NAME_SUFFIX);
        strncat(oldnamebuffer, sessionID, MAX_PORT_NAME_LENGTH - sizeof(NOTIFICATION_PORT_NAME_OLD) - sizeof(NOTIFICATION_PORT_NAME_SUFFIX));
        oldnamebuffer[MAX_PORT_NAME_LENGTH] = '\0';
    }

    retval = task_get_bootstrap_port(mach_task_self(), &bootstrapPort);
    if (ERR_SUCCESS == retval && MACH_PORT_NULL != bootstrapPort) retval = bootstrap_look_up(bootstrapPort, namebuffer, &serverPort);
    if (ERR_SUCCESS != retval || MACH_PORT_NULL == serverPort) retval = bootstrap_look_up(bootstrapPort, oldnamebuffer, &serverPort);
    if (ERR_SUCCESS == retval && MACH_PORT_NULL != serverPort) {
        while (*p) if ('/' == *p++) source = p;
        size = sizeof(mach_msg_base_t) + ((UNCPackContents(NULL, contents, token, itimeout, source) + 3) & (~0x3));
        msg = (mach_msg_base_t *)malloc(size);
        if (msg) {
            bzero(msg, size);
            msg->header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, MACH_MSG_TYPE_MAKE_SEND_ONCE);
            msg->header.msgh_size = size;
            msg->header.msgh_remote_port = serverPort;
            msg->header.msgh_local_port = replyPort;
            msg->header.msgh_id = flags;
            msg->body.msgh_descriptor_count = 0;
            UNCPackContents((char *)msg + sizeof(mach_msg_base_t), contents, token, itimeout, source);
            retval = mach_msg((mach_msg_header_t *)msg, MACH_SEND_MSG|MACH_SEND_TIMEOUT, size, 0, MACH_PORT_NULL, MESSAGE_TIMEOUT, MACH_PORT_NULL);
            free(msg);
        } else {
            retval = unix_err(ENOMEM);
        }
    }
    return retval;
}

extern UNCUserNotificationRef UNCUserNotificationCreate(double timeout, unsigned long flags, int *error, const char **contents) {
    UNCUserNotificationRef userNotification = NULL;
    int retval = ERR_SUCCESS;
    static unsigned short tokenCounter = 0;
    int token = ((getpid()<<16) | (tokenCounter++));
    const char *sessionID = UNCSessionIDForContents(contents);
    mach_port_t replyPort = MACH_PORT_NULL;
    size_t idlen;

    retval = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &replyPort);
    if (ERR_SUCCESS == retval && MACH_PORT_NULL != replyPort) retval = UNCSendRequest(sessionID, replyPort, token, timeout, flags, contents);
    if (ERR_SUCCESS == retval) {
        userNotification = (UNCUserNotificationRef)malloc(sizeof(struct __UNCUserNotification));
        if (userNotification) {
            bzero(userNotification, sizeof(struct __UNCUserNotification));
            userNotification->_replyPort = replyPort;
            userNotification->_token = token;
            userNotification->_timeout = timeout;
            userNotification->_requestFlags = flags;
            userNotification->_responseFlags = 0;
            userNotification->_sessionID = NULL;
            userNotification->_response = NULL;
            userNotification->_responseContents = NULL;
            if (sessionID) {
                idlen = strlen(sessionID);
                if (idlen > MAX_PORT_NAME_LENGTH) idlen = MAX_PORT_NAME_LENGTH;
                userNotification->_sessionID = (char *)malloc(idlen + 1);
                strncpy(userNotification->_sessionID, sessionID, idlen);
                userNotification->_sessionID[idlen] = '\0';
            }
        } else {
            retval = unix_err(ENOMEM);
        }
    }
    if (ERR_SUCCESS != retval && MACH_PORT_NULL != replyPort) mach_port_deallocate(mach_task_self(), replyPort);
    if (error) *error = retval;
    return userNotification;
}

extern int UNCUserNotificationReceiveResponse(UNCUserNotificationRef userNotification, double timeout, unsigned long *responseFlags) {
    int retval = ERR_SUCCESS;
    mach_msg_timeout_t msgtime = (timeout > 0.0 && 1000.0 * timeout < INT_MAX) ? (mach_msg_timeout_t)(1000.0 * timeout) : 0;
    mach_msg_base_t *msg = NULL;
    unsigned long size = MAX_STRING_COUNT * MAX_STRING_LENGTH, contentSize = 0;

    if (userNotification && MACH_PORT_NULL != userNotification->_replyPort) {
        msg = (mach_msg_base_t *)malloc(size);
        if (msg) {
            bzero(msg, size);
            msg->header.msgh_size = size;
            if (msgtime > 0) {
                retval = mach_msg((mach_msg_header_t *)msg, MACH_RCV_MSG|MACH_RCV_TIMEOUT, 0, size, userNotification->_replyPort, msgtime, MACH_PORT_NULL);
            } else {
                retval = mach_msg((mach_msg_header_t *)msg, MACH_RCV_MSG, 0, size, userNotification->_replyPort, 0, MACH_PORT_NULL);
            }
            if (ERR_SUCCESS == retval) {
                if (responseFlags) *responseFlags = msg->header.msgh_id;
                userNotification->_response = msg;
                contentSize = UNCUnpackContents((char *)msg + sizeof(mach_msg_base_t), NULL);
                if (0 < contentSize) {
                    userNotification->_responseContents = (char **)malloc(contentSize * sizeof(char **));
                    if (userNotification->_responseContents) {
                        UNCUnpackContents((char *)msg + sizeof(mach_msg_base_t), userNotification->_responseContents);
                    }
                }
                mach_port_deallocate(mach_task_self(), userNotification->_replyPort);
                userNotification->_replyPort = MACH_PORT_NULL;
            } else {
                free(msg);
            }
        } else {
            retval = unix_err(ENOMEM);
        }
    }
    return retval;
}

extern const char *UNCUserNotificationGetResponseValue(UNCUserNotificationRef userNotification, const char *key, unsigned long index) {
    char **p, *retval = NULL;
    if (userNotification && userNotification->_responseContents && key) {
        p = userNotification->_responseContents;
        while (!retval && *p) {
            if (0 == strcmp(*p++, key) && 0 == index--) retval = *p;
            p++;
        }
    }
    return retval;
}

extern const char **UNCUserNotificationGetResponseContents(UNCUserNotificationRef userNotification) {
    return userNotification ? (const char **)(userNotification->_responseContents) : NULL;
}

extern int UNCUserNotificationUpdate(UNCUserNotificationRef userNotification, double timeout, unsigned long flags, const char **contents) {
    int retval = ERR_SUCCESS;
    if (userNotification && MACH_PORT_NULL != userNotification->_replyPort) {
        retval = UNCSendRequest(userNotification->_sessionID, userNotification->_replyPort, userNotification->_token, timeout, flags|kUNCUpdateFlag, contents);
    }
    return retval;
}

extern int UNCUserNotificationCancel(UNCUserNotificationRef userNotification) {
    int retval = ERR_SUCCESS;
    if (userNotification && MACH_PORT_NULL != userNotification->_replyPort) {
        retval = UNCSendRequest(userNotification->_sessionID, userNotification->_replyPort, userNotification->_token, 0, kUNCCancelFlag, NULL);
    }
    return retval;
}

extern void UNCUserNotificationFree(UNCUserNotificationRef userNotification) {
    if (userNotification) {
        if (MACH_PORT_NULL != userNotification->_replyPort) mach_port_deallocate(mach_task_self(), userNotification->_replyPort);
        if (userNotification->_sessionID) free(userNotification->_sessionID);
        if (userNotification->_responseContents) free(userNotification->_responseContents);
        if (userNotification->_response) free(userNotification->_response);
        free(userNotification);
    }
}

extern int UNCDisplayNotice(double timeout, unsigned long flags, const char *iconPath, const char *soundPath, const char *localizationPath, const char *alertHeader, const char *alertMessage, const char *defaultButtonTitle) {
    UNCUserNotificationRef userNotification;
    int retval = ERR_SUCCESS;
    const char *contents[13];
    unsigned long i = 0;
    if (iconPath) {contents[i++] = kUNCIconPathKey; contents[i++] = iconPath;}
    if (soundPath) {contents[i++] = kUNCSoundPathKey; contents[i++] = soundPath;}
    if (localizationPath) {contents[i++] = kUNCLocalizationPathKey; contents[i++] = localizationPath;}
    if (alertHeader) {contents[i++] = kUNCAlertHeaderKey; contents[i++] = alertHeader;}
    if (alertMessage) {contents[i++] = kUNCAlertMessageKey; contents[i++] = alertMessage;}
    if (defaultButtonTitle) {contents[i++] = kUNCDefaultButtonTitleKey; contents[i++] = defaultButtonTitle;}
    contents[i++] = NULL;
    userNotification = UNCUserNotificationCreate(timeout, flags, &retval, contents);
    if (userNotification) UNCUserNotificationFree(userNotification);
    return retval;
}

extern int UNCDisplayAlert(double timeout, unsigned long flags, const char *iconPath, const char *soundPath, const char *localizationPath, const char *alertHeader, const char *alertMessage, const char *defaultButtonTitle, const char *alternateButtonTitle, const char *otherButtonTitle, unsigned long *responseFlags) {
    UNCUserNotificationRef userNotification;
    int retval = ERR_SUCCESS;
    const char *contents[17];
    unsigned long i = 0;
    if (iconPath) {contents[i++] = kUNCIconPathKey; contents[i++] = iconPath;}
    if (soundPath) {contents[i++] = kUNCSoundPathKey; contents[i++] = soundPath;}
    if (localizationPath) {contents[i++] = kUNCLocalizationPathKey; contents[i++] = localizationPath;}
    if (alertHeader) {contents[i++] = kUNCAlertHeaderKey; contents[i++] = alertHeader;}
    if (alertMessage) {contents[i++] = kUNCAlertMessageKey; contents[i++] = alertMessage;}
    if (defaultButtonTitle) {contents[i++] = kUNCDefaultButtonTitleKey; contents[i++] = defaultButtonTitle;}
    if (alternateButtonTitle) {contents[i++] = kUNCAlternateButtonTitleKey; contents[i++] = alternateButtonTitle;}
    if (otherButtonTitle) {contents[i++] = kUNCOtherButtonTitleKey; contents[i++] = otherButtonTitle;}
    contents[i++] = NULL;
    userNotification = UNCUserNotificationCreate(timeout, flags, &retval, contents);
    if (userNotification) {
        retval = UNCUserNotificationReceiveResponse(userNotification, timeout, responseFlags);
        UNCUserNotificationFree(userNotification);
    }
    return retval;
}

#endif

