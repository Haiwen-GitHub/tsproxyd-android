//
// Created by ts on 2022/1/15.
//

#include <unistd.h>
#include <poll.h>
#include <pthread.h>
#include <sched.h>

#include <android/log.h>

#include "android_native_app_glue.h"
#include "adapter.h"

#define TAG "tsproxyd"
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__))
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__))

/* For debug builds, always enable the debug traces in this library */
#ifndef NDEBUG
#  define LOGV(...)  ((void)__android_log_print(ANDROID_LOG_VERBOSE, TAG, __VA_ARGS__))
#else
#  define LOGV(...)  ((void)0)
#endif

void work_thread(void* agr) {
    int argcc = 3;
    const char* argvv[3] = {"tsproxyd", "-c", "/sdcard/tsproxyd.conf"};
    LOGE("work_thread start \n");
    int ret = main(argcc, argvv);
    //int ret = init_daemon(argcc, argvv);
    if (ret != 0) {
        LOGE("main excute failed !ret:%d\n", ret);
        return;
    }
    LOGE("work_thread end \n");
    return;
}

int android_main(struct android_app* app) {
    LOGI("android_main start");
    pthread_t thread = 0;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&thread, &attr, work_thread, NULL);
    LOGI("thread :%d", thread);
    if (thread != 0) {
        return 1;
    }
    //work_thread(NULL);
    return 0;
    LOGI("android_main end");
}
