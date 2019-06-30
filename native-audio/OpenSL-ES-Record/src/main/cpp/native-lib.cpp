#include <jni.h>
#include <string>
#include "AndroidLog.h"
#include "RecordBuffer.h"
#include "unistd.h"

extern "C"
{
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
}

//引擎接口
static SLObjectItf engineObject = NULL;
//引擎对象
static SLEngineItf engineEngine;

//录音器接口
static SLObjectItf recorderObject = NULL;
//录音器对象
static SLRecordItf recorderRecord;
//缓冲队列
static SLAndroidSimpleBufferQueueItf recorderBufferQueue;

//录制大小设为4096
#define RECORDER_FRAMES (2048)
static unsigned recorderSize = RECORDER_FRAMES * 2;

//PCM文件
FILE *pcmFile;
//录音buffer
RecordBuffer *recordBuffer;

bool finished = false;


#include <string.h>
#include <inttypes.h>
#include <jni.h>
#include <android/log.h>
#include <assert.h>

typedef struct tick_context {
    JavaVM  *javaVM;
    jclass   jniHelperClz;
    jobject  jniHelperObj;
    jclass   mainActivityClz;
    jobject  mainActivityObj;
    pthread_mutex_t  lock;
    int      done;
    short* buffer;
} TickContext;
TickContext g_ctx;

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved){
    LOGD("JNI_OnLoad");
    JNIEnv* env;
    memset(&g_ctx, 0, sizeof(g_ctx));

    g_ctx.javaVM = vm;
    if (vm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR; // JNI version not supported.
    }

    jclass  clz = env->FindClass("com/example/hellojnicallback/JniHandler");
    g_ctx.jniHelperClz = (jclass)env->NewGlobalRef(clz);

    jmethodID  jniHelperCtor = env->GetMethodID(g_ctx.jniHelperClz,"<init>", "()V");
    jobject    handler = env->NewObject(g_ctx.jniHelperClz,jniHelperCtor);
    g_ctx.jniHelperObj = env->NewGlobalRef(handler);

    g_ctx.done = 0;
    g_ctx.mainActivityObj = NULL;
    return  JNI_VERSION_1_6;
}


void sendJavaMsg(JNIEnv *env, jobject instance,
                 jmethodID func,const char* msg){
    jstring javaMsg = env->NewStringUTF(msg);
    env->CallVoidMethod(instance, func, javaMsg);
    env->DeleteLocalRef(javaMsg);
}

void bqRecorderCallback(SLAndroidSimpleBufferQueueItf bq, void *context)
{
    // for streaming recording, here we would call Enqueue to give recorder the next buffer to fill
    // but instead, this is a one-time buffer so we stop recording
    LOGD("bqRecorderCallback record size is %d", recorderSize);

//    fwrite(recordBuffer->getNowBuffer(), 1, recorderSize, pcmFile);
    int write = fwrite(g_ctx.buffer, 1, recorderSize, pcmFile);
    LOGD("fwrite:%d", write);

    if(finished)
    {
        (*recorderRecord)->SetRecordState(recorderRecord, SL_RECORDSTATE_STOPPED);
        fclose(pcmFile);
        LOGD("停止录音");
    } else{

        JavaVM *javaVM = g_ctx.javaVM;
        JNIEnv *env;
        jint res = javaVM->GetEnv((void**)&env, JNI_VERSION_1_6);
        if (res != JNI_OK) {
            res = javaVM->AttachCurrentThread(&env, NULL);
            if (JNI_OK != res) {
                LOGE("Failed to AttachCurrentThread, ErrorCode = %d", res);
                return;
            }
        }
        jmethodID statusId = env->GetMethodID(g_ctx.jniHelperClz,
                                                 "updateStatus",
                                                 "(Ljava/lang/String;)V");
        sendJavaMsg(env, g_ctx.jniHelperObj, statusId,
                    "草你妈隔壁....");

        (*recorderBufferQueue)->Enqueue(recorderBufferQueue,g_ctx.buffer,recorderSize);
//        (*recorderBufferQueue)->Enqueue(recorderBufferQueue, recordBuffer->getRecordBuffer(),recorderSize);
    }
}


extern "C"
JNIEXPORT void JNICALL
Java_ywl5320_com_openslrecord_MainActivity_rdSound(JNIEnv *env, jobject instance, jstring path_, jshortArray buffer_) {
    const char *path = env->GetStringUTFChars(path_, 0);
    jshort *buffer = env->GetShortArrayElements(buffer_, NULL);

    /**
     * PCM文件
     */
    pcmFile = fopen(path, "w");
    /**
     * PCMbuffer队列
     */
    recordBuffer = new RecordBuffer(RECORDER_FRAMES * 2);
    SLresult result;
    /**
     * 创建引擎对象
     */
    result = slCreateEngine(&engineObject, 0, NULL, 0, NULL, NULL);
    result = (*engineObject)->Realize(engineObject, SL_BOOLEAN_FALSE);
    result = (*engineObject)->GetInterface(engineObject, SL_IID_ENGINE, &engineEngine);

    /**
     * 设置IO设备（麦克风）
     */
    SLDataLocator_IODevice loc_dev = {SL_DATALOCATOR_IODEVICE, SL_IODEVICE_AUDIOINPUT,
                                      SL_DEFAULTDEVICEID_AUDIOINPUT, NULL};
    SLDataSource audioSrc = {&loc_dev, NULL};
    /**
     * 设置buffer队列
     */
    SLDataLocator_AndroidSimpleBufferQueue loc_bq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};
    /**
     * 设置录制规格：PCM、2声道、44100HZ、16bit
     */
    SLDataFormat_PCM format_pcm = {SL_DATAFORMAT_PCM, 1, SL_SAMPLINGRATE_44_1,
                                   SL_PCMSAMPLEFORMAT_FIXED_16, SL_PCMSAMPLEFORMAT_FIXED_16,
                                   SL_SPEAKER_FRONT_CENTER, SL_BYTEORDER_LITTLEENDIAN};
//                                   SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT, SL_BYTEORDER_LITTLEENDIAN};
    SLDataSink audioSnk = {&loc_bq, &format_pcm};

    const SLInterfaceID id[1] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE};
    const SLboolean req[1] = {SL_BOOLEAN_TRUE};

    /**
     * 创建录制器
     */
    result = (*engineEngine)->CreateAudioRecorder(engineEngine, &recorderObject, &audioSrc,
                                                  &audioSnk, 1, id, req);
    if (SL_RESULT_SUCCESS != result) {
        return;
    }
    result = (*recorderObject)->Realize(recorderObject, SL_BOOLEAN_FALSE);
    if (SL_RESULT_SUCCESS != result) {
        return;
    }
    result = (*recorderObject)->GetInterface(recorderObject, SL_IID_RECORD, &recorderRecord);
    result = (*recorderObject)->GetInterface(recorderObject, SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
                                             &recorderBufferQueue);
    finished = false;
    g_ctx.buffer = buffer;
    result = (*recorderBufferQueue)->Enqueue(recorderBufferQueue, g_ctx.buffer,recorderSize);
//    result = (*recorderBufferQueue)->Enqueue(recorderBufferQueue, recordBuffer->getRecordBuffer(),recorderSize);
    result = (*recorderBufferQueue)->RegisterCallback(recorderBufferQueue, bqRecorderCallback, NULL);
    LOGD("开始录音");
    /**
     * 开始录音
     */
    (*recorderRecord)->SetRecordState(recorderRecord, SL_RECORDSTATE_RECORDING);
    env->ReleaseStringUTFChars(path_, path);
    env->ReleaseShortArrayElements(buffer_, buffer, 0);

}

extern "C"
JNIEXPORT void JNICALL
Java_ywl5320_com_openslrecord_MainActivity_rdStop(JNIEnv *env, jobject instance) {

    // TODO
    if(recorderRecord != NULL)
    {
        finished = true;
    }
}



