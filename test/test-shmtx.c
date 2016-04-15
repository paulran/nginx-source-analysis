#include "task.h"
#include <stdio.h>
#include <stdlib.h>


#define TEST_LOCK_PATH  "logs/test.lock"

static ngx_shmtx_t shmtx;
static int process_id = 0;
static int process_max = 10;
static int *shared_data = 0;
static const int shared_data_num = 100;

static void worker_process(void *data) {
	int i, j, loop_num;
	int get_data = 0;

	while (1) {
		if (ngx_shmtx_trylock(&shmtx)) {
			log_debug("start process[%d][%d].", process_id, ngx_pid);
			ngx_shmtx_unlock(&shmtx);
			break;
		}
	}

	loop_num = (shared_data_num / process_max) * 2;

	for (i = 0; i < loop_num;) {
		if (ngx_shmtx_trylock(&shmtx)) {
			get_data = 0;
			for (j = 0; j < shared_data_num; ++j) {
				if (*(shared_data + j) == 1) {
					usleep(50);

					get_data = 1;
					*(shared_data + j) = 0;

					log_debug("process[%d][%d] get data %d.", process_id,
							ngx_pid, j);
					break;
				}
			}

			ngx_shmtx_unlock(&shmtx);

			++i;
			if (!get_data) {
				break;
			}
		}

		usleep(100);
	}
}

ngx_pid_t spawn_process() {
	ngx_pid_t pid;

	pid = fork();

	switch (pid) {

	case -1:
		log_error("fork() failed while fork");
		return NGX_INVALID_PID;

	case 0:
		process_id++;
		ngx_pid = ngx_getpid();
		worker_process(NULL);
		exit(0);

	default:
		break;
	}

	process_id++;
	return pid;
}

static void process_wait(void) {
	int status;
	ngx_pid_t pid;
	ngx_err_t err;
	ngx_uint_t one;

	one = 0;

	for (;;) {
		pid = waitpid(-1, &status, WUNTRACED);

		if (pid == 0) {
			return;
		}

		if (pid == -1) {
			err = ngx_errno;

			if (err == EINTR) {
				continue;
			}

			if (err == ECHILD && one) {
				log_debug("all sub processes had exited.");
				return;
			}

			log_error("waitpid() failed");
			return;
		}

		one = 1;

		if (WTERMSIG(status)) {
			log_error("sub process %d exited on signal %d%s.", pid,
					WTERMSIG(status),
					WCOREDUMP(status) ? " (core dumped)" : "");

		} else {
			log_error("sub process %d exited with code %d.", pid,
					WEXITSTATUS(status));
		}
	}
}

HELPER_IMPL(shmtx) {

	u_char *shared;
	ngx_shm_t shm;
	u_char name[] = TEST_LOCK_PATH;
	int i;

	shm.size = sizeof(ngx_shmtx_sh_t) + sizeof(int) * shared_data_num;
	shm.name.len = sizeof("nginx_shared_zone") - 1;
	shm.name.data = (u_char *) "nginx_shared_zone";
	shm.log = global_log;

	if (ngx_shm_alloc(&shm) != NGX_OK) {
		return NGX_ERROR;
	}

	shared = shm.addr;

	if (ngx_shmtx_create(&shmtx, (ngx_shmtx_sh_t *) shared, name) != NGX_OK) {
		return NGX_ERROR;
	}

	shared_data = (int*) (shm.addr + sizeof(ngx_shmtx_sh_t));
	for (i = 0; i < shared_data_num; ++i) {
		*(shared_data + i) = 1;
	}

	ngx_shmtx_trylock(&shmtx);

	for (i = 0; i < process_max; ++i) {
		spawn_process();
	}

	ngx_shmtx_unlock(&shmtx);

	process_wait();


	for (i = 0; i < shared_data_num; ++i) {
		ASSERT(0 == *(shared_data + i));
	}

	ngx_shmtx_destroy(&shmtx);
	ngx_shm_free(shm);
	return 0;
}

