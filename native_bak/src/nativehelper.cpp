#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <jvmti.h>
#include "nativehelper.h"
jvmtiEnv *jvmti=NULL;

jclass nativeHelperClass=NULL;;

jmethodID transformMethodId=NULL;

jmethodID classLoadedMethodId=NULL;

struct ObjectReferrer{
	jlong count;
	ObjectReferrer * next;
	jlong* tagPtr;
};

struct ObjectReferrerList{  
	jint maxNum;
	jint num;
  	ObjectReferrer * root;
};

void * allocate( size_t bytecount) {
	void * resultBuffer = NULL;
	jvmtiError error = jvmti->Allocate(bytecount,(unsigned char**) &resultBuffer);
	if ( error != JVMTI_ERROR_NONE ) {
		resultBuffer = NULL;
	}
	return resultBuffer;
}

void deallocate( void * buffer) {
	jvmti->Deallocate((unsigned char*)buffer);
}

void addObjectReferrer(ObjectReferrerList * list,jlong count){
	ObjectReferrer ** pBefore = &list->root;
	ObjectReferrer * after = NULL;
	
	while(true){
		
		ObjectReferrer * current=*pBefore;
		
		if(current == NULL){
			break;			
		}
		if(count < current->count){
			pBefore = &(current->next);
		}
		else{
			after=current;
			break;
		}
	}
	
	if(list->num >= list->maxNum){

		if(pBefore == &list->root){
			return;
		}
		else{
			ObjectReferrer * referrer = NULL;
			referrer = (ObjectReferrer *) allocate(sizeof(ObjectReferrer));
			referrer->count=count;
			referrer->next = after;
			*pBefore=referrer;
			/// 
			list->root = list->root->next;
		}		
	}
	else{
		list->num++;
		ObjectReferrer * referrer = NULL;
		referrer = (ObjectReferrer *) allocate(sizeof(ObjectReferrer));
		referrer->count=count;
		referrer->next = after;
		*pBefore=referrer;
	}
}

void throwException(JNIEnv * env,char * clazz, char * message)
{
  	jclass exceptionClass = env->FindClass(clazz);
  	if (exceptionClass==NULL) 
  	{
		exceptionClass = env->FindClass("java/lang/RuntimeException");
     		if (exceptionClass==NULL) 
     		{
			fprintf (stderr,"Couldn't throw exception %s - %s\n",clazz,message);
     		}
 	}
  	env->ThrowNew(exceptionClass,message);
	fprintf (stderr,"exception found %s - %s\n",clazz,message);
}


void initJvmti(JNIEnv * env){

	if(jvmti == NULL){
		JavaVM *jvm = 0;
		int res;
		res = env->GetJavaVM(&jvm);
		if (res < 0 || jvm == 0) {
			throwException(env,"java/lang/RuntimeException","GetJavaVM fail");
		}
		res = jvm->GetEnv((void **)&jvmti, JVMTI_VERSION_1_1);
		if (res != JNI_OK || jvmti == 0) {
			throwException(env,"java/lang/RuntimeException","GetEnv fail");
		}
		jvmtiError error;
	  	jvmtiCapabilities   capabilities;
		//jvmtiCapabilities capabilities = { 1 };
	  	error = jvmti->GetCapabilities(&capabilities);
	  	capabilities.can_tag_objects = 1;
	  	capabilities.can_generate_garbage_collection_events = 1;
		capabilities.can_retransform_classes = 1;
		capabilities.can_retransform_any_class = 1;
		capabilities.can_redefine_classes = 1;
		capabilities.can_redefine_any_class = 1;
		capabilities.can_get_source_file_name = 1;
		//capabilities.can_generate_method_entry_events = 1;
		/////////////////////
	  	//capabilities.can_tag_objects = 1;
	  	//capabilities.can_generate_field_modification_events = 1;
	  	//capabilities.can_generate_field_access_events = 1;
	  	//capabilities.can_get_bytecodes = 1;
	  	//capabilities.can_get_synthetic_attribute = 1;
	  	//capabilities.can_get_owned_monitor_info = 1;
	  	//capabilities.can_get_current_contended_monitor = 1;
	  	//capabilities.can_get_monitor_info = 1;
	  	//capabilities.can_pop_frame = 1;
	  	//capabilities.can_redefine_classes = 1;
	  	//capabilities.can_signal_thread = 1;
	  	//capabilities.can_get_source_file_name = 1;
 	  	//capabilities.can_get_line_numbers = 1;
	  	//capabilities.can_get_source_debug_extension = 1;
	  	//capabilities.can_access_local_variables = 1;
	  	//capabilities.can_maintain_original_method_order = 1;
	  	//capabilities.can_generate_single_step_events = 1;
	  	//capabilities.can_generate_exception_events = 1;
	  	//capabilities.can_generate_frame_pop_events = 1;
	  	//capabilities.can_generate_breakpoint_events = 1;
	  	//capabilities.can_suspend = 1;
	  	//capabilities.can_redefine_any_class = 1;
	  	//capabilities.can_get_current_thread_cpu_time = 1;
	  	//capabilities.can_get_thread_cpu_time = 1;
	  	//capabilities.can_generate_method_entry_events = 1;
	  	//capabilities.can_generate_method_exit_events = 1;
	  	//capabilities.can_generate_all_class_hook_events = 1;
	  	//capabilities.can_generate_compiled_method_load_events = 1;
	  	//capabilities.can_generate_monitor_events = 1;
	  	//capabilities.can_generate_vm_object_alloc_events = 1;
	  	//capabilities.can_generate_native_method_bind_events = 1;
	  	//capabilities.can_generate_garbage_collection_events = 1;
	  	//capabilities.can_generate_object_free_events = 1;
	  	//capabilities.can_force_early_return = 1;
	  	//capabilities.can_get_owned_monitor_stack_depth_info = 1;
	  	//capabilities.can_get_constant_pool = 1;
	  	//capabilities.can_set_native_method_prefix = 1;
	  	//capabilities.can_retransform_classes = 1;
	  	//capabilities.can_retransform_any_class = 1;
	  	//capabilities.can_generate_resource_exhaustion_heap_events = 1;
 	  	//capabilities.can_generate_resource_exhaustion_threads_events = 1;


	  	error= jvmti->AddCapabilities(&capabilities);


		if (error != JNI_OK) {
			throwException(env,"java/lang/RuntimeException","AddCapabilities fail");
		}
	}	
}

void initClassInfo(JNIEnv * env){
	nativeHelperClass = env->FindClass("com/chenjw/knife/agent/utils/NativeHelper");
	if(env->ExceptionCheck()){
		env->ExceptionDescribe();	
	}
	transformMethodId = env->GetStaticMethodID(nativeHelperClass,"transform","(Ljava/lang/ClassLoader;Ljava/lang/String;Ljava/lang/Class;Ljava/security/ProtectionDomain;[B)[B");
}


void initClassLoadedInfo(JNIEnv * env){
	nativeHelperClass = env->FindClass("com/chenjw/knife/agent/utils/NativeHelper");
	if(env->ExceptionCheck()){
		env->ExceptionDescribe();	
	}
	classLoadedMethodId = env->GetStaticMethodID(nativeHelperClass,"classLoaded","(Ljava/lang/Class;)V");
}




jvmtiIterationControl JNICALL iterate_markTag
    (jlong class_tag, jlong size, jlong* tag_ptr, void* user_data) 
{
	if(tag_ptr!=NULL){
		*tag_ptr=1;
	}
    	
    	return JVMTI_ITERATION_IGNORE;
}

jint JNICALL iterate_ref_markReferrer
    (jvmtiHeapReferenceKind reference_kind, const jvmtiHeapReferenceInfo* reference_info, jlong class_tag, jlong referrer_class_tag, jlong size, jlong* tag_ptr, jlong* referrer_tag_ptr, jint length, void* user_data) 
{
	if(reference_kind!=JVMTI_HEAP_REFERENCE_FIELD && reference_kind!=JVMTI_HEAP_REFERENCE_ARRAY_ELEMENT){
		return JVMTI_VISIT_OBJECTS;
	}		
	if(referrer_tag_ptr!=NULL && tag_ptr!=NULL && *tag_ptr==2 && referrer_tag_ptr!=tag_ptr){
		*referrer_tag_ptr=1;
	}	
    	return JVMTI_VISIT_OBJECTS;
}

jint JNICALL iterate_ref_markReferree
    (jvmtiHeapReferenceKind reference_kind, const jvmtiHeapReferenceInfo* reference_info, jlong class_tag, jlong referrer_class_tag, jlong size, jlong* tag_ptr, jlong* referrer_tag_ptr, jint length, void* user_data) 
{
	if(reference_kind!=JVMTI_HEAP_REFERENCE_FIELD && reference_kind!=JVMTI_HEAP_REFERENCE_ARRAY_ELEMENT){
		return JVMTI_VISIT_OBJECTS;
	}		
	if(referrer_tag_ptr!=NULL && tag_ptr!=NULL && *referrer_tag_ptr==2 && referrer_tag_ptr!=tag_ptr){
		*tag_ptr=1;
	}	
    	return JVMTI_VISIT_OBJECTS;
}

jint JNICALL iterate_ref_markCountReference
    (jvmtiHeapReferenceKind reference_kind, const jvmtiHeapReferenceInfo* reference_info, jlong class_tag, jlong referrer_class_tag, jlong size, jlong* tag_ptr, jlong* referrer_tag_ptr, jint length, void* user_data) 
{
	if(reference_kind!=JVMTI_HEAP_REFERENCE_FIELD && reference_kind!=JVMTI_HEAP_REFERENCE_ARRAY_ELEMENT){
		return JVMTI_VISIT_OBJECTS;
	}	
	if(referrer_tag_ptr!=NULL && tag_ptr!=NULL){
		(*referrer_tag_ptr)--;
		//printf("ccc%d\n",*referrer_tag_ptr);
	}	
    	return JVMTI_VISIT_OBJECTS;
}

	
jvmtiIterationControl JNICALL iterate_countReference
    (jlong class_tag, jlong size, jlong* tag_ptr, void* user_data)
{

	//printf("bbbb\n");
	if(tag_ptr!=NULL && *tag_ptr<0){
		ObjectReferrerList * list=(ObjectReferrerList *)user_data;
		//printf("before tagPtr=%d,count=%d\n",tag_ptr,*tag_ptr);
		addObjectReferrer(list,*tag_ptr);
		*tag_ptr=-(*tag_ptr);	
		//printf("after tagPtr=%d,count=%d\n",tag_ptr,*tag_ptr);
	}
   	return JVMTI_ITERATION_IGNORE;   
}


jvmtiIterationControl JNICALL iterate_cleanTag
    (jlong class_tag, jlong size, jlong* tag_ptr, void* user_data)
{
	*tag_ptr=0;
   	return JVMTI_ITERATION_IGNORE;   
}


void releaseTags()
{
  	jvmti->IterateOverHeap( JVMTI_HEAP_OBJECT_TAGGED,
                          &iterate_cleanTag, NULL);
}

jobject getBooleanField(JNIEnv * env,jobject obj,jfieldID fieldId){
	jboolean fieldValue=env->GetBooleanField(obj,fieldId);
	jclass clazz=env->FindClass("java/lang/Boolean");
	jobject r=env->NewObject(clazz,env->GetMethodID(clazz,"<init>","(Z)V"),fieldValue);
	return r;
}

jobject getByteField(JNIEnv * env,jobject obj,jfieldID fieldId){
	jbyte fieldValue=env->GetByteField(obj,fieldId);
	jclass clazz=env->FindClass("java/lang/Byte");
	jobject r=env->NewObject(clazz,env->GetMethodID(clazz,"<init>","(B)V"),fieldValue);
	return r;
}

jobject getCharField(JNIEnv * env,jobject obj,jfieldID fieldId){
	jchar fieldValue=env->GetCharField(obj,fieldId);
	jclass clazz=env->FindClass("java/lang/Character");
	jobject r=env->NewObject(clazz,env->GetMethodID(clazz,"<init>","(C)V"),fieldValue);
	return r;
}

jobject getShortField(JNIEnv * env,jobject obj,jfieldID fieldId){
	jshort fieldValue=env->GetShortField(obj,fieldId);
	jclass clazz=env->FindClass("java/lang/Short");
	jobject r=env->NewObject(clazz,env->GetMethodID(clazz,"<init>","(S)V"),fieldValue);
	return r;
}

jobject getIntField(JNIEnv * env,jobject obj,jfieldID fieldId){
	jint fieldValue=env->GetIntField(obj,fieldId);
	jclass clazz=env->FindClass("java/lang/Integer");
	jobject r=env->NewObject(clazz,env->GetMethodID(clazz,"<init>","(I)V"),fieldValue);
	return r;
}

jobject getLongField(JNIEnv * env,jobject obj,jfieldID fieldId){
	jlong fieldValue=env->GetLongField(obj,fieldId);
	jclass clazz=env->FindClass("java/lang/Long");
	jobject r=env->NewObject(clazz,env->GetMethodID(clazz,"<init>","(J)V"),fieldValue);
	return r;
}

jobject getFloatField(JNIEnv * env,jobject obj,jfieldID fieldId){
	jfloat fieldValue=env->GetFloatField(obj,fieldId);
	jclass clazz=env->FindClass("java/lang/Float");
	jobject r=env->NewObject(clazz,env->GetMethodID(clazz,"<init>","(F)V"),fieldValue);
	return r;
}

jobject getDoubleField(JNIEnv * env,jobject obj,jfieldID fieldId){
	jdouble fieldValue=env->GetDoubleField(obj,fieldId);
	jclass clazz=env->FindClass("java/lang/Double");
	jobject r=env->NewObject(clazz,env->GetMethodID(clazz,"<init>","(D)V"),fieldValue);
	return r;
}


jobject getObjectField(JNIEnv * env,jobject obj,jfieldID fieldId){
	jobject fieldValue=env->GetObjectField(obj,fieldId);
	return fieldValue;
}

jobject getFieldValue(JNIEnv * env,jobject obj,jclass fieldClass,jfieldID fieldId)
{
	char* signature;
	jvmti->GetClassSignature(fieldClass,&signature,NULL);
	//printf("bbb%s\n",signature);
	if(strcmp(signature,"Z")==0){
		return getBooleanField(env,obj,fieldId);
	}
	else if(strcmp(signature,"B")==0){
		return getByteField(env,obj,fieldId);
	}
	else if(strcmp(signature,"C")==0){
		return getCharField(env,obj,fieldId);
	}
	else if(strcmp(signature,"S")==0){
		return getShortField(env,obj,fieldId);
	}
	else if(strcmp(signature,"I")==0){
		return getIntField(env,obj,fieldId);
	}
	else if(strcmp(signature,"J")==0){
		return getLongField(env,obj,fieldId);
	}
	else if(strcmp(signature,"F")==0){
		return getFloatField(env,obj,fieldId);
	}
	else if(strcmp(signature,"D")==0){
		return getDoubleField(env,obj,fieldId);
	}
	else{
		return getObjectField(env,obj,fieldId);
	}
}

void JNICALL
eventHandlerClassFileLoadHook(  jvmtiEnv *              jvmti,
                                JNIEnv *                env,
                                jclass                  classBeingRedefined,
                                jobject                 loader,
                                const char*             name,
                                jobject                 protectionDomain,
                                jint                    classDataLen,
                                const unsigned char*    classData,
                                jint*                   newClassDataLen,
                                unsigned char**         newClassData) {
    	unsigned char * resultBuffer = NULL;
        jstring classNameStringObject = env->NewStringUTF(name);
 	jbyteArray classFileBufferObject = env->NewByteArray(classDataLen);
	jbyte * typedBuffer = (jbyte *) classData;          
 	env->SetByteArrayRegion(classFileBufferObject,0,classDataLen,typedBuffer);
     	jbyteArray transformedBufferObject = (jbyteArray)env->CallStaticObjectMethod(nativeHelperClass,transformMethodId,loader,classNameStringObject,classBeingRedefined,protectionDomain,classFileBufferObject);
	if (transformedBufferObject != NULL) {
		jsize transformedBufferSize = env->GetArrayLength(transformedBufferObject);
		env->GetByteArrayRegion(transformedBufferObject,0,transformedBufferSize,(jbyte *)resultBuffer);
		*newClassDataLen = (transformedBufferSize);
   		*newClassData     = resultBuffer;
	}
	return;
}


void JNICALL
eventHandlerClassLoadedHook(jvmtiEnv * jvmti,
            JNIEnv* env,
            jthread thread,
            jclass klass) {
	env->CallStaticObjectMethod(nativeHelperClass,classLoadedMethodId,klass);
}


/*
 * Class:     com_chenjw_knife_agent_utils_NativeHelper
 * Method:    startClassFileLoadHook0
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_com_chenjw_knife_agent_utils_NativeHelper_startClassFileLoadHook0
  (JNIEnv * env, jclass thisClass){
	initJvmti(env);
	initClassInfo(env);
	jvmtiEventCallbacks callbacks;
        memset(&callbacks, 0, sizeof(callbacks));
        callbacks.ClassFileLoadHook = &eventHandlerClassFileLoadHook;
        jvmti->SetEventCallbacks(&callbacks,sizeof(callbacks));
 	jvmti->SetEventNotificationMode(JVMTI_ENABLE,JVMTI_EVENT_CLASS_FILE_LOAD_HOOK,NULL);

}

/*
 * Class:     com_chenjw_knife_agent_utils_NativeHelper
 * Method:    stopClassFileLoadHook0
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_com_chenjw_knife_agent_utils_NativeHelper_stopClassFileLoadHook0
  (JNIEnv * env, jclass thisClass){
	initJvmti(env);
	jvmti->SetEventNotificationMode(JVMTI_DISABLE,JVMTI_EVENT_CLASS_FILE_LOAD_HOOK,NULL);
}

/*
 * Class:     com_chenjw_knife_agent_utils_NativeHelper
 * Method:    startClassFileLoadHook0
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_com_chenjw_knife_agent_utils_NativeHelper_startClassLoadHook0
  (JNIEnv * env, jclass thisClass){
	initJvmti(env);
	initClassLoadedInfo(env);
	jvmtiEventCallbacks callbacks;
        memset(&callbacks, 0, sizeof(callbacks));
        callbacks.ClassLoad = &eventHandlerClassLoadedHook;
        jvmti->SetEventCallbacks(&callbacks,sizeof(callbacks));
 	jvmti->SetEventNotificationMode(JVMTI_ENABLE,JVMTI_EVENT_CLASS_LOAD,NULL);

}

/*
 * Class:     com_chenjw_knife_agent_utils_NativeHelper
 * Method:    stopClassFileLoadHook0
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_com_chenjw_knife_agent_utils_NativeHelper_stopClassLoadHook0
  (JNIEnv * env, jclass thisClass){
	initJvmti(env);
	jvmti->SetEventNotificationMode(JVMTI_DISABLE,JVMTI_EVENT_CLASS_LOAD,NULL);
}

/*
 * Class:     com_chenjw_knife_agent_utils_NativeHelper
 * Method:    retransformClasses
 * Signature: ([Ljava/lang/Class;)V
 */
JNIEXPORT void JNICALL Java_com_chenjw_knife_agent_utils_NativeHelper_retransformClasses0
  (JNIEnv * env, jclass thisClass, jobjectArray classes){
	initJvmti(env);
    	jsize       numClasses           = 0;
    	jclass *    classArray           = NULL;
	numClasses = env->GetArrayLength(classes);
        classArray = (jclass *) allocate(numClasses * sizeof(jclass));
        jint index;
        for (index = 0; index < numClasses; index++) {
      		classArray[index] = (jclass)env->GetObjectArrayElement(classes, index);
        }
 	jvmti->RetransformClasses(numClasses, classArray);
	if (classArray != NULL) {
		deallocate((void*)classArray);
    	}
}


JNIEXPORT void JNICALL Java_com_chenjw_knife_agent_utils_NativeHelper_countReferree0
  (JNIEnv * env, jclass thisClass, jint maxNum,jlongArray countArray,jobjectArray objArray)
{

	initJvmti(env);
	jclass loadedObject = env->FindClass("java/lang/Object");

	jvmtiHeapCallbacks * callbacks;
        callbacks=(jvmtiHeapCallbacks *)allocate(sizeof(jvmtiHeapCallbacks));
        callbacks->heap_reference_callback = &iterate_ref_markCountReference;
	callbacks->primitive_field_callback =NULL;
	callbacks->array_primitive_value_callback =NULL;
	callbacks->string_primitive_value_callback =NULL;
	jvmti->FollowReferences(0,NULL,NULL,callbacks,NULL);
	jint countObjts=0;
  	jobject * objs;
  	jlong * tagResults;
//printf("~~~1\n");
	ObjectReferrerList * list=NULL;
 	list = (ObjectReferrerList *) allocate( sizeof(ObjectReferrerList));
	list->maxNum=maxNum;
	list->num=0;
	list->root=NULL;
	jvmti->IterateOverHeap(JVMTI_HEAP_OBJECT_TAGGED,iterate_countReference,list);
	jlong* idToQuery;
//printf("~~~2\n");
	idToQuery = (jlong*) allocate(list->num*sizeof(jlong));
	ObjectReferrer* referrer=list->root;
	jint i=0;
	while(referrer!=NULL){
		jlong count=-(referrer->count);
		bool exist=false;
		for(jint j=0;j<i;j++){
			if(idToQuery[j]==count){
				exist=true;
				break;
			}
		}
		if(!exist){
			idToQuery[i]=count;
			i++;
			//printf("~~~%d\n",count);
		}
		referrer=referrer->next;
		
	}
//printf("~~~3\n");
	jvmti->GetObjectsWithTags(list->num,idToQuery,&countObjts,&objs,&tagResults);
  	// Set the object array
  	for (jint i=0;i<maxNum;i++) {
		jint n=-1;
		jlong nn=-1;
		for(jint j=0;j<countObjts;j++){
			if(tagResults[j]>nn){
				n=j;
				nn=tagResults[j];
			}
		}
		env->SetLongArrayRegion(countArray,i,1, &tagResults[n]);
		env->SetObjectArrayElement(objArray,i, objs[n]);
		
		tagResults[n]=-1;
		

  	} 

	deallocate((void *)tagResults);  
  	deallocate((void *)objs);  
	deallocate((void *)callbacks);
	deallocate((void *)idToQuery);
	deallocate((void *)list);
 	releaseTags();       
}

JNIEXPORT jobjectArray JNICALL Java_com_chenjw_knife_agent_utils_NativeHelper_findReferrerByObject0
  (JNIEnv * env, jclass thisClass, jobject obj)
{
	initJvmti(env);
	jclass loadedObject = env->FindClass("java/lang/Object");

	jvmti->SetTag(obj,2);
	jvmtiHeapCallbacks callbacks;
        memset(&callbacks, 0, sizeof(callbacks));
        callbacks.heap_reference_callback = &iterate_ref_markReferrer;
	jvmtiError e=jvmti->FollowReferences(0,NULL,NULL,&callbacks,NULL);
	jint countObjts=0;
  	jobject * objs;
  	jlong * tagResults;
  	jlong idToQuery=1;  
   
  	jvmti->GetObjectsWithTags(1,&idToQuery,&countObjts,&objs,&tagResults);
  	// Set the object array
  	jobjectArray arrayReturn = env->NewObjectArray(countObjts,loadedObject,0);
  	for (jint i=0;i<countObjts;i++) {
		//printf("~~~%d\n",&objs[i]);
     		env->SetObjectArrayElement(arrayReturn,i, objs[i]);
  	} 
	jvmti->Deallocate((unsigned char *)tagResults);  
  	jvmti->Deallocate((unsigned char *)objs);  
  	releaseTags();         
  	return arrayReturn;
}

JNIEXPORT jobjectArray JNICALL Java_com_chenjw_knife_agent_utils_NativeHelper_findReferreeByObject0
  (JNIEnv * env, jclass thisClass, jobject obj)
{

	initJvmti(env);
	jclass loadedObject = env->FindClass("java/lang/Object");

	jvmti->SetTag(obj,2);

	jvmtiHeapCallbacks callbacks;

        memset(&callbacks, 0, sizeof(callbacks));

        callbacks.heap_reference_callback = &iterate_ref_markReferree;

	jvmtiError e=jvmti->FollowReferences(0,NULL,NULL,&callbacks,NULL);
	jint countObjts=0;
  	jobject * objs;
  	jlong * tagResults;
  	jlong idToQuery=1;  
 	
  	jvmti->GetObjectsWithTags(1,&idToQuery,&countObjts,&objs,&tagResults);
  	// Set the object array
  	jobjectArray arrayReturn = env->NewObjectArray(countObjts,loadedObject,0);
  	for (jint i=0;i<countObjts;i++) {
		//printf("~~~%d\n",&objs[i]);
     		env->SetObjectArrayElement(arrayReturn,i, objs[i]);
  	} 
	jvmti->Deallocate((unsigned char *)tagResults);  
  	jvmti->Deallocate((unsigned char *)objs);  
  	releaseTags();         
  	return arrayReturn;
}


JNIEXPORT jobjectArray JNICALL Java_com_chenjw_knife_agent_utils_NativeHelper_findInstancesByClass0
  (JNIEnv * env, jclass thisClass, jclass klass)
{ 
	initJvmti(env);
	jclass loadedObject = env->FindClass("java/lang/Object");
  	jvmtiError e=jvmti->IterateOverInstancesOfClass(klass,JVMTI_HEAP_OBJECT_EITHER,iterate_markTag,NULL);
	
  	jint countObjts=0;
  	jobject * objs;
  	jlong * tagResults;
  	jlong idToQuery=1;  
   	
  	jvmti->GetObjectsWithTags(1,&idToQuery,&countObjts,&objs,&tagResults);
  	// Set the object array
  	jobjectArray arrayReturn = env->NewObjectArray(countObjts,loadedObject,0);
  	for (jint i=0;i<countObjts;i++) {
     		env->SetObjectArrayElement(arrayReturn,i, objs[i]);
  	}
 printf("aaa%d\n",tagResults);
 printf("b%d\n",objs); 
	jvmti->Deallocate((unsigned char *)tagResults); 

  	jvmti->Deallocate((unsigned char *)objs);  
 printf("c");
  	releaseTags();         

  	return arrayReturn;
}

/////////////////////////////////////////////////
// get field
/////////////////////////////////////////////////

JNIEXPORT jobject JNICALL Java_com_chenjw_knife_agent_utils_NativeHelper_getFieldValue0
  (JNIEnv * env, jclass thisClass, jobject obj,jclass fieldClass,jstring fieldName,jclass fieldType)
{
	
	initJvmti(env);
	char* fieldNameChars=(char*)env->GetStringUTFChars(fieldName,0);	
	//printf("123%s\n",fieldNameChars);	
	
	//jclass klass=env->GetObjectClass(obj);
	jclass klass=fieldClass;
	//jfieldID fieldId=env->GetFieldID(klass,fieldNameChars,fieldNameChars);
	jint count=0;
	jfieldID* fieldIds;
	jvmti->GetClassFields(klass,&count,&fieldIds);
	for(int i=0;i<count;i++){
		char* tFieldName;
		jvmti->GetFieldName(klass,fieldIds[i],&tFieldName,0,0);
		
		//printf("%s\n",fieldNameChars);
		if(strcmp(tFieldName,fieldNameChars)==0){
			//printf("123%s\n",tFieldName);
			//printf("aaa\n");
			jobject result=getFieldValue(env,obj,fieldType,fieldIds[i]);
			jvmti->Deallocate((unsigned char *)tFieldName);
			jvmti->Deallocate((unsigned char *)fieldIds);
			env->ReleaseStringUTFChars(fieldName,fieldNameChars);  	
			return result;
		}
	}
	//char* errorStr=strcat("field not found ",fieldNameChars);
	throwException(env,NULL,"field not found");
	jvmti->Deallocate((unsigned char *)fieldIds);  
	env->ReleaseStringUTFChars(fieldName,fieldNameChars);
	return NULL;
}


JNIEXPORT void JNICALL Java_com_chenjw_knife_agent_utils_NativeHelper_redefineClass0
  (JNIEnv * env, jclass thisClass, jclass klass, jbyteArray byteArray)
{
	initJvmti(env);
    	jvmtiClassDefinition * classDef = NULL;
	jvmti->Allocate(sizeof(jvmtiClassDefinition),(unsigned char**)&classDef);
  	classDef->klass = klass;
	classDef->class_bytes = (unsigned char*)env->GetByteArrayElements(byteArray, NULL);
	classDef->class_byte_count = env->GetArrayLength(byteArray);
	jvmti->RedefineClasses(1, classDef);
	jvmti->Deallocate((unsigned char *)classDef);
}

/////////////////////////////////////////////////
// set field
/////////////////////////////////////////////////

JNIEXPORT void JNICALL Java_com_chenjw_knife_agent_utils_NativeHelper_setObjectFieldValue0 (JNIEnv *env, jclass thisClass, jobject obj, jobject field, jobject newValue){
	jfieldID fieldId=env->FromReflectedField(field);
	env->SetObjectField(obj,fieldId,newValue);
}

JNIEXPORT void JNICALL Java_com_chenjw_knife_agent_utils_NativeHelper_setBooleanFieldValue0 (JNIEnv *env, jclass thisClass, jobject obj, jobject field, jboolean newValue){
	jfieldID fieldId=env->FromReflectedField(field);
	env->SetBooleanField(obj,fieldId,newValue);
}

JNIEXPORT void JNICALL Java_com_chenjw_knife_agent_utils_NativeHelper_setByteFieldValue0 (JNIEnv *env, jclass thisClass, jobject obj, jobject field, jbyte newValue){
	jfieldID fieldId=env->FromReflectedField(field);
	env->SetByteField(obj,fieldId,newValue);
}

JNIEXPORT void JNICALL Java_com_chenjw_knife_agent_utils_NativeHelper_setCharFieldValue0 (JNIEnv *env, jclass thisClass, jobject obj, jobject field, jchar newValue){
	jfieldID fieldId=env->FromReflectedField(field);
	env->SetCharField(obj,fieldId,newValue);
}

JNIEXPORT void JNICALL Java_com_chenjw_knife_agent_utils_NativeHelper_setShortFieldValue0 (JNIEnv *env, jclass thisClass, jobject obj, jobject field, jshort newValue){
	jfieldID fieldId=env->FromReflectedField(field);
	env->SetShortField(obj,fieldId,newValue);
}

JNIEXPORT void JNICALL Java_com_chenjw_knife_agent_utils_NativeHelper_setIntFieldValue0 (JNIEnv *env, jclass thisClass, jobject obj, jobject field, jint newValue){
	jfieldID fieldId=env->FromReflectedField(field);
	env->SetIntField(obj,fieldId,newValue);
}

JNIEXPORT void JNICALL Java_com_chenjw_knife_agent_utils_NativeHelper_setLongFieldValue0 (JNIEnv *env, jclass thisClass, jobject obj, jobject field, jlong newValue){
	jfieldID fieldId=env->FromReflectedField(field);
	env->SetLongField(obj,fieldId,newValue);
}

JNIEXPORT void JNICALL Java_com_chenjw_knife_agent_utils_NativeHelper_setFloatFieldValue0 (JNIEnv *env, jclass thisClass, jobject obj, jobject field, jfloat newValue){
	jfieldID fieldId=env->FromReflectedField(field);
	env->SetFloatField(obj,fieldId,newValue);
}

JNIEXPORT void JNICALL Java_com_chenjw_knife_agent_utils_NativeHelper_setDoubleFieldValue0 (JNIEnv *env, jclass thisClass, jobject obj, jobject field, jdouble newValue){
	jfieldID fieldId=env->FromReflectedField(field);
	env->SetDoubleField(obj,fieldId,newValue);
}

/////////////////////////////////////////////////
// set static field
/////////////////////////////////////////////////

JNIEXPORT void JNICALL Java_com_chenjw_knife_agent_utils_NativeHelper_setStaticObjectFieldValue0 (JNIEnv *env, jclass thisClass, jclass clazz, jobject field, jobject newValue){
	jfieldID fieldId=env->FromReflectedField(field);
	env->SetStaticObjectField(clazz,fieldId,newValue);
}

JNIEXPORT void JNICALL Java_com_chenjw_knife_agent_utils_NativeHelper_setStaticBooleanFieldValue0 (JNIEnv *env, jclass thisClass, jclass clazz, jobject field, jboolean newValue){
	jfieldID fieldId=env->FromReflectedField(field);
	env->SetStaticBooleanField(clazz,fieldId,newValue);
}

JNIEXPORT void JNICALL Java_com_chenjw_knife_agent_utils_NativeHelper_setStaticByteFieldValue0 (JNIEnv *env, jclass thisClass, jclass clazz, jobject field, jbyte newValue){
	jfieldID fieldId=env->FromReflectedField(field);
	env->SetStaticByteField(clazz,fieldId,newValue);
}

JNIEXPORT void JNICALL Java_com_chenjw_knife_agent_utils_NativeHelper_setStaticCharFieldValue0 (JNIEnv *env, jclass thisClass, jclass clazz, jobject field, jchar newValue){
	jfieldID fieldId=env->FromReflectedField(field);
	env->SetStaticCharField(clazz,fieldId,newValue);
}

JNIEXPORT void JNICALL Java_com_chenjw_knife_agent_utils_NativeHelper_setStaticShortFieldValue0 (JNIEnv *env, jclass thisClass, jclass clazz, jobject field, jshort newValue){
	jfieldID fieldId=env->FromReflectedField(field);
	env->SetStaticShortField(clazz,fieldId,newValue);
}

JNIEXPORT void JNICALL Java_com_chenjw_knife_agent_utils_NativeHelper_setStaticIntFieldValue0 (JNIEnv *env, jclass thisClass, jclass clazz, jobject field, jint newValue){
	jfieldID fieldId=env->FromReflectedField(field);
	env->SetStaticIntField(clazz,fieldId,newValue);
}

JNIEXPORT void JNICALL Java_com_chenjw_knife_agent_utils_NativeHelper_setStaticLongFieldValue0 (JNIEnv *env, jclass thisClass, jclass clazz, jobject field, jlong newValue){
	jfieldID fieldId=env->FromReflectedField(field);
	env->SetStaticLongField(clazz,fieldId,newValue);
}

JNIEXPORT void JNICALL Java_com_chenjw_knife_agent_utils_NativeHelper_setStaticFloatFieldValue0 (JNIEnv *env, jclass thisClass, jclass clazz, jobject field, jfloat newValue){
	jfieldID fieldId=env->FromReflectedField(field);
	env->SetStaticFloatField(clazz,fieldId,newValue);
}

JNIEXPORT void JNICALL Java_com_chenjw_knife_agent_utils_NativeHelper_setStaticDoubleFieldValue0 (JNIEnv *env, jclass thisClass, jclass clazz, jobject field, jdouble newValue){
	jfieldID fieldId=env->FromReflectedField(field);
	env->SetStaticDoubleField(clazz,fieldId,newValue);
}

/////////////////////////////////////////////////
// get field
/////////////////////////////////////////////////


JNIEXPORT jobject JNICALL Java_com_chenjw_knife_agent_utils_NativeHelper_getObjectFieldValue0
  (JNIEnv *env, jclass thisClass, jobject obj, jobject field){
	jfieldID fieldId=env->FromReflectedField(field);
	return env->GetObjectField(obj,fieldId);
}

JNIEXPORT jboolean JNICALL Java_com_chenjw_knife_agent_utils_NativeHelper_getBooleanFieldValue0
  (JNIEnv *env, jclass thisClass, jobject obj, jobject field){
	jfieldID fieldId=env->FromReflectedField(field);
	return env->GetBooleanField(obj,fieldId);
}

JNIEXPORT jbyte JNICALL Java_com_chenjw_knife_agent_utils_NativeHelper_getByteFieldValue0
  (JNIEnv *env, jclass thisClass, jobject obj, jobject field){
	jfieldID fieldId=env->FromReflectedField(field);
	return env->GetByteField(obj,fieldId);
}

JNIEXPORT jchar JNICALL Java_com_chenjw_knife_agent_utils_NativeHelper_getCharFieldValue0
  (JNIEnv *env, jclass thisClass, jobject obj, jobject field){
	jfieldID fieldId=env->FromReflectedField(field);
	return env->GetCharField(obj,fieldId);
}

JNIEXPORT jshort JNICALL Java_com_chenjw_knife_agent_utils_NativeHelper_getShortFieldValue0
  (JNIEnv *env, jclass thisClass, jobject obj, jobject field){
	jfieldID fieldId=env->FromReflectedField(field);
	return env->GetShortField(obj,fieldId);
}

JNIEXPORT jint JNICALL Java_com_chenjw_knife_agent_utils_NativeHelper_getIntFieldValue0
  (JNIEnv *env, jclass thisClass, jobject obj, jobject field){
	jfieldID fieldId=env->FromReflectedField(field);
	return env->GetIntField(obj,fieldId);
}

JNIEXPORT jlong JNICALL Java_com_chenjw_knife_agent_utils_NativeHelper_getLongFieldValue0
  (JNIEnv *env, jclass thisClass, jobject obj, jobject field){
	jfieldID fieldId=env->FromReflectedField(field);
	return env->GetLongField(obj,fieldId);
}

JNIEXPORT jfloat JNICALL Java_com_chenjw_knife_agent_utils_NativeHelper_getFloatFieldValue0
  (JNIEnv *env, jclass thisClass, jobject obj, jobject field){
	jfieldID fieldId=env->FromReflectedField(field);
	return env->GetFloatField(obj,fieldId);
}

JNIEXPORT jdouble JNICALL Java_com_chenjw_knife_agent_utils_NativeHelper_getDoubleFieldValue0
  (JNIEnv *env, jclass thisClass, jobject obj, jobject field){
	jfieldID fieldId=env->FromReflectedField(field);
	return env->GetDoubleField(obj,fieldId);
}

/////////////////////////////////////////////////
// get static field
/////////////////////////////////////////////////

JNIEXPORT jobject JNICALL Java_com_chenjw_knife_agent_utils_NativeHelper_getStaticObjectFieldValue0
  (JNIEnv *env, jclass thisClass, jclass clazz, jobject field){
	jfieldID fieldId=env->FromReflectedField(field);
	return env->GetStaticObjectField(clazz,fieldId);
}

JNIEXPORT jboolean JNICALL Java_com_chenjw_knife_agent_utils_NativeHelper_getStaticBooleanFieldValue0
  (JNIEnv *env, jclass thisClass, jclass clazz, jobject field){
	jfieldID fieldId=env->FromReflectedField(field);
	return env->GetStaticBooleanField(clazz,fieldId);
}

JNIEXPORT jbyte JNICALL Java_com_chenjw_knife_agent_utils_NativeHelper_getStaticByteFieldValue0
  (JNIEnv *env, jclass thisClass, jclass clazz, jobject field){
	jfieldID fieldId=env->FromReflectedField(field);
	return env->GetStaticByteField(clazz,fieldId);
}


JNIEXPORT jchar JNICALL Java_com_chenjw_knife_agent_utils_NativeHelper_getStaticCharFieldValue0
  (JNIEnv *env, jclass thisClass, jclass clazz, jobject field){
	jfieldID fieldId=env->FromReflectedField(field);
	return env->GetStaticCharField(clazz,fieldId);
}

JNIEXPORT jshort JNICALL Java_com_chenjw_knife_agent_utils_NativeHelper_getStaticShortFieldValue0
  (JNIEnv *env, jclass thisClass, jclass clazz, jobject field){
	jfieldID fieldId=env->FromReflectedField(field);
	return env->GetStaticShortField(clazz,fieldId);
}

JNIEXPORT jint JNICALL Java_com_chenjw_knife_agent_utils_NativeHelper_getStaticIntFieldValue0
  (JNIEnv *env, jclass thisClass, jclass clazz, jobject field){
	jfieldID fieldId=env->FromReflectedField(field);
	return env->GetStaticIntField(clazz,fieldId);
}

JNIEXPORT jlong JNICALL Java_com_chenjw_knife_agent_utils_NativeHelper_getStaticLongFieldValue0
  (JNIEnv *env, jclass thisClass, jclass clazz, jobject field){
	jfieldID fieldId=env->FromReflectedField(field);
	return env->GetStaticLongField(clazz,fieldId);
}


JNIEXPORT jfloat JNICALL Java_com_chenjw_knife_agent_utils_NativeHelper_getStaticFloatFieldValue0
  (JNIEnv *env, jclass thisClass, jclass clazz, jobject field){
	jfieldID fieldId=env->FromReflectedField(field);
	return env->GetStaticFloatField(clazz,fieldId);
}


JNIEXPORT jdouble JNICALL Java_com_chenjw_knife_agent_utils_NativeHelper_getStaticDoubleFieldValue0
  (JNIEnv *env, jclass thisClass, jclass clazz, jobject field){
	jfieldID fieldId=env->FromReflectedField(field);
	return env->GetStaticDoubleField(clazz,fieldId);
}


JNIEXPORT jstring JNICALL Java_com_chenjw_knife_agent_utils_NativeHelper_getClassSourceFileName0
  (JNIEnv *env, jclass thisClass, jclass clazz){
	initJvmti(env);
	char* source_name_ptr;
	jvmti->GetSourceFileName(clazz,&source_name_ptr);
	jstring result=env->NewStringUTF(source_name_ptr);
	return result;
}

JNIEXPORT jobject JNICALL Java_com_chenjw_knife_agent_utils_NativeHelper_getCallerClassLoader0
  (JNIEnv *env, jclass thisClass){
	//jclass caller = JVM_GetCallerClass(env,2);
	//return caller != 0 ? JVM_GetClassLoader(env,caller) : 0;

	// not implemented
	return NULL;
}

jmethodID methodEnterMethodId=NULL;

void initMethodEnterInfo(JNIEnv * env){
	nativeHelperClass = env->FindClass("com/chenjw/knife/agent/utils/NativeHelper");
	if(env->ExceptionCheck()){
		env->ExceptionDescribe();	
	}
	methodEnterMethodId = env->GetStaticMethodID(nativeHelperClass,"methodEnter","(Ljava/lang/reflect/Method;)V");
	printf("ccc%d\n",methodEnterMethodId);
}

void JNICALL
eventHandlerMethodEnter(jvmtiEnv * jvmti,
            JNIEnv* env,
            jthread thread,
            jmethodID method) {
	printf("bbb\n");
	jclass* clazz;
	jvmti->GetMethodDeclaringClass(method,clazz);
	jobject obj=env->ToReflectedMethod(*clazz,method,true);
	
	env->CallStaticObjectMethod(nativeHelperClass,methodEnterMethodId,obj);
}


JNIEXPORT void JNICALL Java_com_chenjw_knife_agent_utils_NativeHelper_startMethodTrace0
  (JNIEnv * env, jclass thisClass){
	initJvmti(env);
	initMethodEnterInfo(env);
	jvmtiEventCallbacks callbacks;
        memset(&callbacks, 0, sizeof(callbacks));
        callbacks.MethodEntry = &eventHandlerMethodEnter;
        jvmti->SetEventCallbacks(&callbacks,sizeof(callbacks));
 	jvmti->SetEventNotificationMode(JVMTI_ENABLE,JVMTI_EVENT_METHOD_ENTRY,NULL);
	printf("aaa\n");

}

JNIEXPORT void JNICALL Java_com_chenjw_knife_agent_utils_NativeHelper_stopMethodTrace0
  (JNIEnv * env, jclass thisClass){
	initJvmti(env);
	jvmti->SetEventNotificationMode(JVMTI_DISABLE,JVMTI_EVENT_METHOD_ENTRY,NULL);
	printf("ddd\n");
}
