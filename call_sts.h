#include <stdio.h>
#include <stddef.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#ifndef CALL_STS
#define CALL_STS

int call_sts_program(const char* in_file_name, const char* out_file_name, int in_file_size); 

float get_results_from_file(const char* results_file);

#endif
