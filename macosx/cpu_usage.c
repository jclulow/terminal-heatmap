

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <mach/mach.h>
#include <mach/processor_info.h>
#include <mach/mach_host.h>
#include <time.h>

typedef struct cpu_load {
	int cl_system;
	int cl_user;
	int cl_nice;
	int cl_idle;
} cpu_load_t;

int cpu_count = -1;
cpu_load_t *cpu_load = NULL;

void
update_cpu_load()
{
	kern_return_t error;
	natural_t nmpu;
	processor_info_array_t info;
	mach_msg_type_number_t cnt;
	int infosz;
	int i;
	int firstrun = 0;

	error = host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO,
	    &nmpu, &info, &cnt);
	if (error != KERN_SUCCESS) {
		mach_error("update_cpu_load1", error);
		exit(1);
	}

	if (cpu_load == NULL) {
		cpu_count = nmpu;
		cpu_load = calloc(nmpu, sizeof (cpu_load_t));
		firstrun = 1;
	}

	infosz = cnt / nmpu;

	for (i = 0; i < nmpu; i++) {
		cpu_load_t newload;
		newload.cl_system = info[CPU_STATE_SYSTEM + i * infosz];
		newload.cl_user = info[CPU_STATE_USER + i * infosz];
		newload.cl_nice = info[CPU_STATE_NICE + i * infosz];
		newload.cl_idle = info[CPU_STATE_IDLE + i * infosz];
		if (!firstrun) {
			int delta_system = newload.cl_system -
				cpu_load[i].cl_system;
			int delta_user = newload.cl_user -
				cpu_load[i].cl_user;
			int delta_nice = newload.cl_nice -
				cpu_load[i].cl_nice;
			int delta_idle = newload.cl_idle -
				cpu_load[i].cl_idle;

			int used = delta_system + delta_user + delta_nice;
			int percent = 100 * used / (used + delta_idle);

			fprintf(stdout, "%d ", percent);
		}
		memcpy(&cpu_load[i], &newload, sizeof (cpu_load_t));
	}
	if (!firstrun)
		fprintf(stdout, "\n");

	vm_deallocate(mach_task_self(), (vm_address_t)info, cnt);
}

int
main(int argc, char **argv)
{
	setvbuf(stdout, NULL, _IOLBF, 0);

	for (;;) {
		update_cpu_load();
		sleep(1);
	}
}
