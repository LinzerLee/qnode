/*
 * process.h
 *
 *  Created on: 2017年10月21日
 *      Author: linzer
 */

#ifndef __QNODE_PROCESS_H__
#define __QNODE_PROCESS_H__

#include <timestamp.h>

#define EXE_PATH_BUF_SIZE	1024
#define PROC_STAT_BUF_SIZE	64


typedef struct {
	double user_sec;
	double sys_sec;
} cpu_time;

pid_t proc_get_pid();
uid_t proc_get_uid();
uid_t proc_get_euid();
timestamp proc_get_starttime();
int proc_get_ticks_per_sec();
int proc_get_page_size();
int proc_get_ofd_num();
int proc_get_mfd_num();
void proc_get_status(stringpiece *output);
void proc_get_stat(stringpiece *output);
int proc_get_thread_num();
void proc_get_thread(ARRAY arr);
void proc_get_username(stringpiece *output);
void proc_get_hostname(stringpiece *output);
void proc_get_procname(stringpiece *output);
void proc_get_exe_path(stringpiece *output);
cpu_time proc_get_cputime();
int proc_check_debugmode();

#endif /* __QNODE_PROCESS_H__ */
