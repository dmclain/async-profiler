/*
 * Copyright 2016 Andrei Pangin
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _VMENTRY_H
#define _VMENTRY_H

#include <jvmti.h>


#if __GNUC__ == 4
#  undef JNIEXPORT
#  define JNIEXPORT __attribute__((visibility("default")))
#endif


// Denotes ASGCT_CallFrame where method_id has special meaning (not jmethodID)
enum ASGCT_CallFrameType {
    BCI_NATIVE_FRAME        = -10,  // native function name (char*)
    BCI_ALLOC               = -11,  // name of the allocated class
    BCI_ALLOC_OUTSIDE_TLAB  = -12,  // name of the class allocated outside TLAB
    BCI_LOCK                = -13,  // class name of the locked object
    BCI_PARK                = -14,  // class name of the park() blocker
    BCI_THREAD_ID           = -15,  // method_id designates a thread
    BCI_ERROR               = -16,  // method_id is an error string
    BCI_INSTRUMENT          = -17,  // synthetic method_id that should not appear in the call stack
};

// See hotspot/src/share/vm/prims/forte.cpp
enum ASGCT_Failure {
    ticks_no_Java_frame         =  0,
    ticks_no_class_load         = -1,
    ticks_GC_active             = -2,
    ticks_unknown_not_Java      = -3,
    ticks_not_walkable_not_Java = -4,
    ticks_unknown_Java          = -5,
    ticks_not_walkable_Java     = -6,
    ticks_unknown_state         = -7,
    ticks_thread_exit           = -8,
    ticks_deopt                 = -9,
    ticks_safepoint             = -10,
    ticks_skipped               = -11,
    ASGCT_FAILURE_TYPES         = 12
};

typedef struct {
    jint bci;
    jmethodID method_id;
} ASGCT_CallFrame;

typedef struct {
    JNIEnv* env;
    jint num_frames;
    ASGCT_CallFrame* frames;
} ASGCT_CallTrace;

typedef void (*AsyncGetCallTrace)(ASGCT_CallTrace*, jint, void*);

typedef struct {
    void* unused[38];
    jstring (JNICALL *ExecuteDiagnosticCommand)(JNIEnv*, jstring);
} VMManagement;

typedef VMManagement* (*JVM_GetManagement)(jint);

typedef struct {
    void* unused1[86];
    jvmtiError (JNICALL *RedefineClasses)(jvmtiEnv*, jint, const jvmtiClassDefinition*);
    void* unused2[35];
    jvmtiError (JNICALL *GenerateEvents)(jvmtiEnv*, jvmtiEvent);
    void* unused3[28];
    jvmtiError (JNICALL *RetransformClasses)(jvmtiEnv*, jint, const jclass*);
} JVMTIFunctions;


class VM {
  private:
    static JavaVM* _vm;
    static jvmtiEnv* _jvmti;
    static JVM_GetManagement _getManagement;
    static jvmtiError (JNICALL *_orig_RedefineClasses)(jvmtiEnv*, jint, const jvmtiClassDefinition*);
    static jvmtiError (JNICALL *_orig_RetransformClasses)(jvmtiEnv*, jint, const jclass* classes);
    static jvmtiError (JNICALL *_orig_GenerateEvents)(jvmtiEnv* jvmti, jvmtiEvent event_type);
    static volatile int _in_redefine_classes;
    static int _hotspot_version;
    static bool _zero_vm;

    static void ready();
    static void* getLibraryHandle(const char* name);
    static void loadMethodIDs(jvmtiEnv* jvmti, JNIEnv* jni, jclass klass);
    static void loadAllMethodIDs(jvmtiEnv* jvmti, JNIEnv* jni);

  public:
    static void* _libjvm;
    static void* _libjava;
    static AsyncGetCallTrace _asyncGetCallTrace;

    static bool init(JavaVM* vm, bool attach);

    static void restartProfiler(void* timer_arg);

    static jvmtiEnv* jvmti() {
        return _jvmti;
    }

    static JNIEnv* jni() {
        JNIEnv* jni;
        return _vm->GetEnv((void**)&jni, JNI_VERSION_1_6) == 0 ? jni : NULL;
    }

    static JNIEnv* attachThread(const char* name) {
        JNIEnv* jni;
        JavaVMAttachArgs args = {JNI_VERSION_1_6, (char*)name, NULL};
        return _vm->AttachCurrentThreadAsDaemon((void**)&jni, &args) == 0 ? jni : NULL;
    }

    static void detachThread() {
        _vm->DetachCurrentThread();
    }

    static VMManagement* management() {
        return _getManagement != NULL ? _getManagement(0x20030000) : NULL;
    }

    static int hotspot_version() {
        return _hotspot_version;
    }

    static bool inRedefineClasses() {
        return _in_redefine_classes > 0;
    }

    static void JNICALL VMInit(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread);
    static void JNICALL VMDeath(jvmtiEnv* jvmti, JNIEnv* jni);

    static void JNICALL ClassLoad(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread, jclass klass) {
        // Needed only for AsyncGetCallTrace support
    }

    static void JNICALL ClassPrepare(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread, jclass klass) {
        loadMethodIDs(jvmti, jni, klass);
    }

    static jvmtiError JNICALL RedefineClassesHook(jvmtiEnv* jvmti, jint class_count, const jvmtiClassDefinition* class_definitions);
    static jvmtiError JNICALL RetransformClassesHook(jvmtiEnv* jvmti, jint class_count, const jclass* classes);
    static jvmtiError JNICALL GenerateEventsHook(jvmtiEnv* jvmti, jvmtiEvent event_type);
};

#endif // _VMENTRY_H
