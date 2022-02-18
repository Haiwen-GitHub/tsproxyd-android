//
// Created by ts on 2022/1/15.
//

#include <unistd.h>
#include <poll.h>
#include <pthread.h>
#include <sched.h>
#include <android/log.h>
#include <stdbool.h>

#include "android_native_app_glue.h"
#include "adapter.h"
#include "ts_native_service.h"

#define TAG "tsproxyd"
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__))
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__))
#define LOGV(...)  ((void)__android_log_print(ANDROID_LOG_VERBOSE, TAG, __VA_ARGS__))

static bool g_main_runing = false;

static void work_thread(void* arg) {
    int argcc = 3;
    const char* argvv[3] = {"tsproxyd", "-c", "/sdcard/Android/data/com.ts.tsproxyd/files/tsproxyd.conf"};
    if (arg != NULL) {
        argvv[2] = (const char *) arg;
        LOGE("work_thread argvv[2]:%s\n", argvv[2]);
    }
    LOGE("work_thread start \n");
    g_main_runing = true;
    int ret = main(argcc, (char **) argvv);
    if (ret != 0) {
        LOGE("main excute failed !ret:%d\n", ret);
        return;
    }
    g_main_runing = false;
    LOGE("work_thread end \n");
}

int android_main(struct android_app* app) {
    if (g_main_runing) {
        return 0;
    }
    LOGI("android_main start");
    pthread_t thread = 0;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&thread, &attr, (void *(*)(void *)) work_thread, NULL);
    LOGI("android_main end thread :%ld", thread);
    return 0;
}

int android_main_func(TsNativeService *service) {
    if (g_main_runing) {
        return 0;
    }
    LOGI("android_main_func start");
    pthread_t thread = 0;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&thread, &attr, (void *(*)(void *)) work_thread, (void*)service->configFilePath);
    LOGI("android_main_func end thread :%ld", thread);
    return 0;
}