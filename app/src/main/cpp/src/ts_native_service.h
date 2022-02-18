//
// Created by CA on 2022/2/16.
//
/**
 * @file ts_native_service.h
 */
#ifndef TSPROXYD_TS_NATIVE_SERVICE_H
#define TSPROXYD_TS_NATIVE_SERVICE_H

#include <stdint.h>
#include <sys/types.h>
#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * {@link TsNativeServiceCallbacks}
 */
struct TsNativeServiceCallbacks;

/**
 * This structure defines the native side of an TsNativeService.
 * It is created by the jni code call in native, and handed to the service's native
 * code as it is being launched.
 */
typedef struct _TsNativeService {
    /**
     * Pointer to the callback function table of the native service.
     * You can set the functions here to your own callbacks.  The callbacks
     * pointer itself here should not be changed; it is allocated and managed
     * for you by the jni code.
     */
    struct TsNativeServiceCallbacks *callbacks;

    /**
     * The global handle on the process's Java VM.
     */
    JavaVM *vm;

    /**
     * JNI context for the main thread of the app.  Note that this field
     * can ONLY be used from the main thread of the process; that is, the
     * thread that calls into the TsNativeServiceCallbacks.
     */
    JNIEnv *env;

    /**
     * The TsproxydService object handle.
     */
    jobject tsproxydService;

    /**
     * Path to this application's internal data directory.
     */
    const char *internalDataPath;

    /**
     * Path to config file data directory.
     */
    const char *configFilePath;

    /**
     * The platform's SDK version code.
     */
    int32_t sdkVersion;

    /**
     * tsproxyd lib handler.
     */
    void *dlhandle;

    /**
     * hv lib handler.
     */
    void *dlhandle_hv;

    /**
     * entry function name in tsproxyd lib.
     */
    int (*mainFunc)(struct _TsNativeService *service);
} TsNativeService;

/**
 * These are the callbacks the framework makes into a native application.
 * All of these callbacks happen on the main thread of the application.
 * By default, all callbacks are NULL; set to a pointer to your own function
 * to have it called.
 */
typedef struct TsNativeServiceCallbacks {
    /**
     * NativeService is being destroyed
     */
    void (*onCreate)(TsNativeService *service);

    /**
     * NativeService has started
     */
    void (*onStart)(TsNativeService *service);

    /**
     * NativeService has stopped.
     */
    void (*onStop)(TsNativeService *service);

    /**
     * NativeService is being destroyed
     */
    void (*onDestroy)(TsNativeService *service);

} TsNativeServiceCallbacks;

/**
 * This is the function that must be in the native code to instantiate the
 * application's native service.  It is called with the service instance (see
 * above);
 */
typedef int TsNativeService_mainFunc_Type(TsNativeService *service);

/**
 * The name of the function that NativeInstance looks for when launching its
 * native code.  This is the default function that is used, you can specify
 * "com.ts.tsproxyd.func_name" string meta-data in your manifest to use a different
 * function.
 */
extern TsNativeService_mainFunc_Type android_main_func;

#ifdef __cplusplus
}
#endif

#endif //TSPROXYD_TS_NATIVE_SERVICE_H
