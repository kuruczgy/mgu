#ifndef MGU_JNI_HELPER_H
#define MGU_JNI_HELPER_H

// JNI convenience macros
#define jni_check if ((*env)->ExceptionCheck(env) == JNI_TRUE) goto jni_error
#define jni_find_class(name, path) jclass class_ ## name = \
	(*env)->FindClass(env, path); jni_check
#define jni_find_method(class, name, sig) jmethodID mid_ ## name = \
	(*env)->GetMethodID(env, class_ ## class, #name, sig); jni_check
#define jni_find_static_method(class, name, sig) jmethodID mid_ ## name = \
	(*env)->GetStaticMethodID(env, class_ ## class, #name, sig); jni_check
#define jni_find_ctor(class, sig) jmethodID ctor_ ## class = \
	(*env)->GetMethodID(env, class_ ## class, "<init>", sig); jni_check
#define jni_find_field(class, name, sig) jfieldID fid_ ## name = \
	(*env)->GetFieldID(env, class_ ## class, #name, sig); jni_check
#define jni_find_static_field(class, name, sig) jfieldID fid_ ## name = \
	(*env)->GetStaticFieldID(env, class_ ## class, #name, sig); jni_check

#endif
