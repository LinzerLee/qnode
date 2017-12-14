/*
 * process.c
 *
 *  Created on: 2017年10月21日
 *      Author: linzer
 */
#include <assert.h>
#include <dirent.h>
#include <pwd.h>
#include <stdio.h> // snprintf
#include <stdlib.h>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/times.h>
#include <ctype.h>
#ifdef __APPLE__
#include <sys/proc_info.h>
#include <libproc.h>
#endif

#include <define.h>
#include <stringpiece.h>
#include <array.h>
#include <util.h>
#include <thread.h>
#include <process.h>

__thread static int t_ofd_num = 0;
__thread static ARRAY t_pid_array = NULL;

static int fd_filter(const struct dirent* d) {
	if(isdigit(d->d_name[0])) {
		++ t_ofd_num;
	}

	return 0;
}

static int task_filter(const struct dirent* d) {
	if(isdigit(d->d_name[0])) {
		if(t_pid_array) {
			ARRAY_PUSH_BACK(t_pid_array, pid_t, atoi(d->d_name));
		}
	}

  return 0;
}

typedef int (* filter_cb)(const struct dirent *);

static int scan_dir(const char *dirpath, filter_cb callback) {
	struct dirent **namelist = NULL;
	int result = scandir(dirpath, &namelist, callback, alphasort);
	assert(namelist == NULL);

	return result;
}

pid_t proc_get_pid() {
	return getpid();
}

uid_t proc_get_uid() {
	return getuid();
}

uid_t proc_get_euid() {
	return geteuid();
}

timestamp proc_get_starttime() {
	static timestamp g_starttime = { 0 };
	if(unlikely(!g_starttime.us)) {
		g_starttime = timestamp_now();
	}

	return g_starttime;
}

int proc_get_ticks_per_sec() {
	static int g_clockticks = 0;
	if(unlikely(!g_clockticks)) {
		g_clockticks = (int)sysconf(_SC_CLK_TCK);
	}

	return g_clockticks;
}

int proc_get_page_size() {
	static int g_pagesize = 0;
	if(unlikely(!g_pagesize)) {
		g_pagesize = (int)sysconf(_SC_PAGE_SIZE);
	}

	return g_pagesize;
}

int proc_get_ofd_num() {
	t_ofd_num = 0;
	scan_dir("/proc/self/fd", fd_filter);

	return t_ofd_num;
}

int proc_get_mfd_num() {
	struct rlimit rl;
	if (getrlimit(RLIMIT_NOFILE, &rl)) {
		return proc_get_ofd_num();
	} else {
		return (int)rl.rlim_cur;
	}
}

void proc_get_status(stringpiece *output) {
	assert(output != NULL);
	smallfile *sf = smallfile_open("/proc/self/status");
	smallfile_read_tostring(sf, output);
	smallfile_close(sf);
}

void proc_get_stat(stringpiece *output) {
	assert(output != NULL);
	char buf[PROC_STAT_BUF_SIZE];
	snprintf(buf, sizeof buf, "/proc/self/task/%d/stat", thread_current_id());
	smallfile *sf = smallfile_open(buf);
	smallfile_read_tostring(sf, output);
	smallfile_close(sf);
}

int proc_get_thread_num() {
	int result = 0;
	stringpiece status;
	stringpiece_init(&status);
	proc_get_status(&status);
	size_t pos = stringpiece_find_str(&status, "Threads:");
	if(pos != STRINGPIECE_NOPOS) {
		result = atoi(stringpiece_to_cstring(&status) + pos + 8);
	}
	stringpiece_release(&status);

	return result;
}

static int pid_cmp_fn(const void *a, const void *b) {
	return *(int *)a - *(int *)b;
}

void proc_get_thread(ARRAY arr) {
	assert(arr != NULL);
	t_pid_array = arr;
	scan_dir("/proc/self/task", task_filter);
	t_pid_array = NULL;
	ARRAY_QSORT(arr, int, pid_cmp_fn);
}

void proc_get_username(stringpiece *output) {
	assert(output != NULL);
	struct passwd pwd;
	struct passwd* result = NULL;
	char buf[8192];
	const char* name = "unknownuser";

	getpwuid_r(proc_get_uid(), &pwd, buf, sizeof buf, &result);
	if (result) {
		name = pwd.pw_name;
	}

	stringpiece_assgin(output, name);
}

void proc_get_hostname(stringpiece *output) {
	// HOST_NAME_MAX 64
	// _POSIX_HOST_NAME_MAX 255
	assert(output != NULL);
	char buf[256];
	if (gethostname(buf, sizeof buf) == 0) {
		buf[sizeof(buf) - 1] = '\0';
		stringpiece_assgin(output, buf);
	} else {
		stringpiece_assgin(output, "unknownhost");
	}
}

void proc_get_procname(stringpiece *output) {
	assert(output != NULL);
#ifdef __APPLE__
	char pathBuffer[PROC_PIDPATHINFO_MAXSIZE];
	bzero(pathBuffer, PROC_PIDPATHINFO_MAXSIZE);
	proc_pidpath(getpid(), pathBuffer, sizeof(pathBuffer));
	if (strlen(pathBuffer) > 0) {
		const char *cur = strrchr(pathBuffer, '/');
		if(!TEST_VAILD_PTR(cur)) {
			cur = pathBuffer;
		} else {
			++ cur;
		}
		stringpiece_assgin(output, cur);
	} else {
		stringpiece_clear(output);
	}
#elif defined __linux__
	proc_get_stat(output);
	int lp = stringpiece_find_char(output, '(');
	int rp = stringpiece_rfind_char(output, ')');

	if (lp != STRINGPIECE_NOPOS && rp != STRINGPIECE_NOPOS && lp < rp) {
		stringpiece tmp = stringpiece_substr(output, lp + 1, rp - lp - 1);
		stringpiece_swap(output, &tmp);
		stringpiece_release(&tmp);
	} else {
		stringpiece_clear(output);
	}
#endif
}

void proc_get_exe_path(stringpiece *output) {
	assert(output != NULL);
	char buf[EXE_PATH_BUF_SIZE];
	ssize_t n = readlink("/proc/self/exe", buf, sizeof buf);
	stringpiece_clear(output);
	if(n > 0) {
		if(n == EXE_PATH_BUF_SIZE)
			-- n;
		buf[n] = '\0';
		stringpiece_assgin(output, buf);
	}
}

cpu_time proc_get_cputime() {
	cpu_time ct = { .0 };
	struct tms tms;
	if (likely(times(&tms) >= 0)) {
		const double hz = (double)proc_get_ticks_per_sec();
		ct.user_sec = (double)tms.tms_utime / hz;
		ct.sys_sec = (double)tms.tms_stime / hz;
	}

	return ct;
}

int proc_check_debugmode() {
#ifdef NDEBUG
	return false;
#else
	return true;
#endif
}

