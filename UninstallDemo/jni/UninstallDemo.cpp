/* 头文件begin */
#include "uninstalldemo.h"
#include <jni.h>
/* 头文件end */

#ifdef __cplusplus
extern "C" {
#endif

/* 内全局变量begin */
static char TAG[] = "System.out";
static jboolean isCopy = JNI_TRUE;

/* 内全局变量 */

/*
 * Class:     main_activity_UninstalledObserverActivity
 * Method:    init
 * Signature: ()V
 * return: 子进程pid
 */
JNIEXPORT void JNICALL Java_com_fengma_uninstalldemo_MainActivity_init(
		JNIEnv * env, jobject obj, jstring userSerial, jstring appDir,
		jstring appFilesDir, jstring appObservedFile, jstring appLockFile, jstring url) {
	jstring tag = appDir;

	LOG_DEBUG(env->GetStringUTFChars(tag, &isCopy),
			env->GetStringUTFChars(url, &isCopy));

	// fork子进程，执行监控代码
	pid_t pid = fork();
	if (pid < 0) {
		LOG_ERROR(env->GetStringUTFChars(tag, &isCopy),
				env->GetStringUTFChars(env->NewStringUTF("fork failed !!!"),
						&isCopy));

		exit(1);
	} else if (pid == 0) {
		// 若监听文件所在文件夹不存在，创建
		FILE *p_filesDir = fopen(env->GetStringUTFChars(appFilesDir, &isCopy), "r");
		if (p_filesDir == NULL) {
			int filesDirRet = mkdir(env->GetStringUTFChars(appFilesDir, &isCopy), S_IRWXU | S_IRWXG | S_IXOTH);
			if (filesDirRet == -1) {
				LOG_ERROR(env->GetStringUTFChars(tag, &isCopy),
						env->GetStringUTFChars(
								env->NewStringUTF("mkdir failed !!!"),
								&isCopy));

				exit(1);
			}
		}

		// 若被监听文件不存在，创建文件
		FILE *p_observedFile = fopen(env->GetStringUTFChars(appObservedFile, &isCopy), "r");
		if (p_observedFile == NULL) {
			p_observedFile = fopen(env->GetStringUTFChars(appObservedFile, &isCopy), "w");
		}
		fclose(p_observedFile);

		// 创建锁文件，通过检测加锁状态来保证只有一个卸载监听进程
		int lockFileDescriptor = open(env->GetStringUTFChars(appLockFile, &isCopy), O_RDONLY);
		if (lockFileDescriptor == -1) {
			lockFileDescriptor = open(env->GetStringUTFChars(appLockFile, &isCopy), O_CREAT);
		}
		int lockRet = flock(lockFileDescriptor, LOCK_EX | LOCK_NB);
		if (lockRet == -1) {
			LOG_DEBUG(env->GetStringUTFChars(tag, &isCopy),
					env->GetStringUTFChars(
							env->NewStringUTF("observed by another process"),
							&isCopy));

			exit(0);
		}
		LOG_DEBUG(env->GetStringUTFChars(tag, &isCopy),
				env->GetStringUTFChars(
						env->NewStringUTF("observed by child process"),
						&isCopy));

		// 分配空间，以便读取event
		void *p_buf = malloc(sizeof(struct inotify_event));
		if (p_buf == NULL) {
			LOG_ERROR(env->GetStringUTFChars(tag, &isCopy),
					env->GetStringUTFChars(
							env->NewStringUTF("malloc failed !!!"), &isCopy));
			exit(1);
		}

		// 开始监听
		LOG_DEBUG(env->GetStringUTFChars(tag, &isCopy),
				env->GetStringUTFChars(env->NewStringUTF("start observe"),
						&isCopy));

		// 初始化
		int fileDescriptor = inotify_init();
		if (fileDescriptor < 0) {
			free(p_buf);
			LOG_ERROR(env->GetStringUTFChars(tag, &isCopy),
					env->GetStringUTFChars(
							env->NewStringUTF("inotify_init failed !!!"),
							&isCopy));
			exit(1);
		}

		// 添加被监听文件到监听列表
		int watchDescriptor = inotify_add_watch(fileDescriptor,
				env->GetStringUTFChars(appObservedFile, &isCopy), IN_ALL_EVENTS);
		if (watchDescriptor < 0) {
			free(p_buf);
			LOG_ERROR(env->GetStringUTFChars(tag, &isCopy),
					env->GetStringUTFChars(
							env->NewStringUTF("inotify_add_watch failed !!!"),
							&isCopy));
			exit(1);
		}

		while (1) {
			// read会阻塞进程
			size_t readBytes = read(fileDescriptor, p_buf,
					sizeof(struct inotify_event));
			// 若文件被删除，可能是已卸载，还需进一步判断app文件夹是否存在
			if (IN_DELETE_SELF == ((struct inotify_event *) p_buf)->mask) {
				FILE *p_appDir = fopen(env->GetStringUTFChars(appDir, &isCopy), "r");
				// 确认已卸载
				if (p_appDir == NULL) {
					inotify_rm_watch(fileDescriptor, watchDescriptor);

					break;
				}
				// 未卸载，可能用户执行了"清除数据"
				else {
					fclose(p_appDir);

					// 重新创建被监听文件，并重新监听
					FILE *p_observedFile = fopen(env->GetStringUTFChars(appObservedFile, &isCopy), "w");
					fclose(p_observedFile);

					int watchDescriptor = inotify_add_watch(fileDescriptor,
							env->GetStringUTFChars(appObservedFile, &isCopy), IN_ALL_EVENTS);
					if (watchDescriptor < 0) {
						free(p_buf);

						LOG_ERROR(env->GetStringUTFChars(tag, &isCopy),
								env->GetStringUTFChars(
										env->NewStringUTF(
												"inotify_add_watch failed !!!"),
										&isCopy));
						exit(1);
					}
				}
			}
		}
		// 释放资源
		free(p_buf);
		// 停止监听
		LOG_DEBUG(env->GetStringUTFChars(tag, &isCopy),
				env->GetStringUTFChars(env->NewStringUTF("stop observe"),
						&isCopy));
		if (userSerial == NULL) {
			// 执行命令am start -a android.intent.action.VIEW -d $(url)
			execlp("am", "am", "start", "-a", "android.intent.action.VIEW",
					"-d", env->GetStringUTFChars(url, &isCopy), (char *) NULL);
		} else {
			// 执行命令am start --user userSerial -a android.intent.action.VIEW -d $(url)
			execlp("am", "am", "start", "--user",
					env->GetStringUTFChars(userSerial, &isCopy), "-a",
					"android.intent.action.VIEW", "-d", env->GetStringUTFChars(url, &isCopy),
					(char *) NULL);
		}

		// 执行命令失败log
		LOG_ERROR(env->GetStringUTFChars(tag, &isCopy),
				env->GetStringUTFChars(
						env->NewStringUTF("exec AM command failed !!!"),
						&isCopy));
	} else {
		// 父进程直接退出，使子进程被init进程领养，以避免子进程僵死，同时返回子进程pid
		return;
	}
}

#ifdef __cplusplus
}
#endif
