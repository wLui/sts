#include "call_sts.h"

int call_sts_program(const char* in_file_name, const char* out_file_name, int in_file_size) {
	int num_procs = in_file_size / 131072;
	if (num_procs > 32) {
		num_procs = 32;
	}

	if (num_procs == 0) {
		printf("[-] not enough bytes to analyze! (min: %d, given: %d)\n",131072,in_file_size);
	}

	char cmd[2048];
	snprintf(cmd,2048,"salloc -N %d -n %d -m cyclic mpirun -n %d ./mpi_sts %s %s",num_procs,num_procs,num_procs,in_file_name,out_file_name);

	return system(cmd);

}

float get_results_from_file(const char* results_file) {

	FILE* fd = fopen(results_file,"r");

        if (fd == NULL) {
                printf("[-] Error opening file %s\n",results_file);
                printf("[-] Error: %s\n",strerror(errno));
                return -1;
        }

	char str[2048];
	//int num_tokens;

	// lines 17 and 18 contain the test data
	// skip first 16
	for (int i = 0; i < 10; i++) {
		fscanf(fd,"%2048[^\n]\n",str);
	}

	int pass,total;
	
	fscanf(fd,"%d/%d%s\n",&pass,&total,str);

	return (float) pass / (float) total;

}
