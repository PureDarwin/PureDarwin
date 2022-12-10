
#include <jni.h>

#include <stddef.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>

#include <android/log.h>

#include "testenv.h"

#define TAG "CommonCrypto"

static int pipeFDs[2];
static pthread_t stdio_redirect_thread;

static void *stdio_redirect_thread_func(void *context) {
    ssize_t bytesRead;

    char readBuf[4000];
    while ((bytesRead = read(pipeFDs[0], readBuf, sizeof readBuf - 1)) > 0) {

        if (readBuf[bytesRead - 1] == '\n') {
            --bytesRead;
        }

        readBuf[bytesRead] = 0;

        __android_log_write(ANDROID_LOG_DEBUG, TAG, readBuf);
    }

    return 0;
}

static int start_stdio_redirect_thread() {
    setvbuf(stdout, 0, _IOLBF, 0);
    setvbuf(stderr, 0, _IONBF, 0);

    pipe(pipeFDs);
    dup2(pipeFDs[1], 1);
    dup2(pipeFDs[1], 2);

    if (pthread_create(&stdio_redirect_thread, 0, stdio_redirect_thread_func, 0) == -1) {
        return -1;
    }

    pthread_detach(stdio_redirect_thread);

    return 0;
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *jvm, void *reserved) {
    if (start_stdio_redirect_thread() != 0) {
        return JNI_ERR;
    }

    return JNI_VERSION_1_6;
}


JNIEXPORT jint JNICALL Java_com_apple_commoncrypto_CommonCryptoTestShim_startTest
        (JNIEnv *env, jobject obj) {
    return tests_begin(0, NULL);
}

