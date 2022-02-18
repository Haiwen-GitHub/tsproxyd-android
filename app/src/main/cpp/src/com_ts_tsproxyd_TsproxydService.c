
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <poll.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <jni.h>

#include <android/log.h>

#ifndef _Included_com_ts_tsproxyd_TsproxydService
#define _Included_com_ts_tsproxyd_TsproxydService
#ifdef __cplusplus
extern "C" {
#endif

#include "com_ts_tsproxyd_TsproxydService.h"
#include "ts_native_service.h"

#define TAG "TsNativeService"
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__))
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__))
#define LOGV(...)  ((void)__android_log_print(ANDROID_LOG_VERBOSE, TAG, __VA_ARGS__))

static void dummyCallback(TsNativeService *tsNativeService) {
    LOGI("dummyCallback\n");
}

// make a native service state
static TsNativeService *createNativeService(TsNativeService_mainFunc_Type mainFunc, TsNativeServiceCallbacks *callbacks) {
    TsNativeService *nativeService = malloc(sizeof(TsNativeService));
    if (nativeService == NULL) {
        LOGE("malloc TsNativeService failed\n");
        return NULL;
    }
    memset((TsNativeService *) nativeService, 0, sizeof(TsNativeService));

    nativeService->mainFunc = mainFunc;

    if (callbacks) {
        nativeService->callbacks = callbacks;
    } else {
        nativeService->callbacks = malloc(sizeof(TsNativeServiceCallbacks));
        if (nativeService->callbacks == NULL) {
            LOGE("malloc TsNativeServiceCallbacks failed\n");
            free(nativeService);
            return NULL;
        }
        memset(nativeService->callbacks, 0, sizeof(TsNativeServiceCallbacks));
        nativeService->callbacks->onCreate = dummyCallback;
        nativeService->callbacks->onDestroy = dummyCallback;
        nativeService->callbacks->onStart = dummyCallback;
        nativeService->callbacks->onStop = dummyCallback;
    }
    return nativeService;
}

static void releaseNativeService(TsNativeService *nativeService) {
    if (nativeService->callbacks) {
        free(nativeService->callbacks);
        nativeService->callbacks = NULL;
    }
    free(nativeService);
    nativeService = NULL;
}

/*
 * Class:     com_ts_tsproxyd_TsproxydService
 * Method:    loadNativeService
 * Signature: (Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)J
 */
JNIEXPORT jlong JNICALL Java_com_ts_tsproxyd_TsproxydService_loadNativeService
        (JNIEnv *env, jobject serviceObj, jstring path, jstring path_hv, jstring funcName, jstring config_path, jstring internalDataDir) {

    TsNativeService *nativeService = NULL;
    LOGI("loadNativeService start");

    // load libhv libs first
    const char *pathStr = (*env)->GetStringUTFChars(env, path_hv, NULL);
    void *handle_hv = dlopen(pathStr, RTLD_LAZY);
    if (handle_hv == NULL) {
        LOGE("dlopen failed pathStr:%s", pathStr);
        return 0;
    }
    (*env)->ReleaseStringUTFChars(env, path, pathStr);

    // load tsporxyd libs
    pathStr = (*env)->GetStringUTFChars(env, path, NULL);
    void *handle = dlopen(pathStr, RTLD_LAZY);
    if (handle == NULL) {
        LOGE("dlopen failed pathStr:%s", pathStr);
        return 0;
    }
    (*env)->ReleaseStringUTFChars(env, path, pathStr);

    if (handle_hv != NULL && handle != NULL) {
        const char *funcStr = (*env)->GetStringUTFChars(env, funcName, NULL);
        void *funcPtr = dlsym(handle, funcStr);
        nativeService = createNativeService((TsNativeService_mainFunc_Type *) funcPtr, NULL);
        (*env)->ReleaseStringUTFChars(env, funcName, funcStr);

        nativeService->dlhandle = handle;
        nativeService->dlhandle_hv = handle_hv;

        if ((*env)->GetJavaVM(env, &nativeService->vm) < 0) {
            LOGE("GetJavaVM failed");
            releaseNativeService(nativeService);
            return 0;
        }

        nativeService->env = env;
        nativeService->tsproxydService = (*env)->NewGlobalRef(env, serviceObj);

        const char *dirStr = (*env)->GetStringUTFChars(env, internalDataDir, NULL);
        unsigned long length = strlen(dirStr) + 1;
        if (dirStr != NULL && length > 1) {
            nativeService->internalDataPath = malloc(length);
            memset((void *) nativeService->internalDataPath, 0, length);
            memcpy((void *) nativeService->internalDataPath, dirStr, length);
        } else {
            nativeService->internalDataPath = NULL;
        }
        (*env)->ReleaseStringUTFChars(env, internalDataDir, dirStr);

        const char *configStr = (*env)->GetStringUTFChars(env, config_path, NULL);
        length = strlen(configStr) + 1;
        if (configStr != NULL && length > 1) {
            nativeService->configFilePath = malloc(length);
            memset((void *) nativeService->configFilePath, 0, length);
            memcpy((void *) nativeService->configFilePath, configStr, length);
        } else {
            nativeService->configFilePath = NULL;
        }
        (*env)->ReleaseStringUTFChars(env, config_path, configStr);
    }

    return (jlong) nativeService;
}

/*
 * Class:     com_ts_tsproxyd_TsproxydService
 * Method:    unloadNativeService
 * Signature: (J)V
 */
JNIEXPORT void JNICALL Java_com_ts_tsproxyd_TsproxydService_unloadNativeService
        (JNIEnv *env, jobject service, jlong handle) {
    LOGI("unloadNativeService");
    if (handle != 0) {
        TsNativeService *tsNativeService = (TsNativeService *) handle;
        free((void *) tsNativeService->internalDataPath);
        tsNativeService->internalDataPath = NULL;
        free((void *) tsNativeService->configFilePath);
        tsNativeService->configFilePath = NULL;
        releaseNativeService(tsNativeService);
        tsNativeService = NULL;
    }
}

/*
 * Class:     com_ts_tsproxyd_TsproxydService
 * Method:    onStartNativeService
 * Signature: (J)I
 */
JNIEXPORT jint JNICALL Java_com_ts_tsproxyd_TsproxydService_onStartNativeService
        (JNIEnv *env, jobject service, jlong handle) {
    LOGI("onStartNativeService");
    if (handle != 0) {
        TsNativeService *tns = (TsNativeService *) handle;
        if (tns->mainFunc != NULL) {
            tns->mainFunc(tns);
        } else {
            // call default
            android_main_func(tns);
        }

        // call back?
        if (tns->callbacks->onStart != NULL) {
            tns->callbacks->onStart(tns);
        }
    }
    return 0;
}

/*
 * Class:     com_ts_tsproxyd_TsproxydService
 * Method:    onStopNativeService
 * Signature: (J)I
 */
JNIEXPORT jint JNICALL Java_com_ts_tsproxyd_TsproxydService_onStopNativeService
        (JNIEnv *env, jobject service, jlong handle) {
    LOGI("onStopNativeService");
    if (handle != 0) {
        TsNativeService *tns = (TsNativeService *) handle;
        if (tns->callbacks->onStop != NULL) {
            tns->callbacks->onStop(tns);
        }
    }
    return 0;
}

#ifdef __cplusplus
}
#endif
#endif
