/*****************************************************************************
		    C U M U L A T I V E	  S U M S   T E S T
 *****************************************************************************/

/*
 * This code has been heavily modified by the following people:
 *
 *      Landon Curt Noll
 *      Tom Gilgan
 *      Riccardo Paccagnella
 *
 * See the README.md and the initial comment in sts.c for more information.
 *
 * WE (THOSE LISTED ABOVE WHO HEAVILY MODIFIED THIS CODE) DISCLAIM ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL WE (THOSE LISTED ABOVE
 * WHO HEAVILY MODIFIED THIS CODE) BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
 * USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * chongo (Landon Curt Noll, http://www.isthe.com/chongo/index.html) /\oo/\
 *
 * Share and enjoy! :-)
 */


// Exit codes: 30 thru 39

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <errno.h>
#include "../utils/externs.h"
#include "../utils/cephes.h"
#include "../utils/utilities.h"
#include "../utils/debug.h"


/*
 * Private stats - stats.txt information for this test
 */
struct CumulativeSums_private_stats {
	bool success_forward;	// Success or failure for forward test
	bool success_backward;	// Success or failure for backward test
	long int z_forward;	// Maximum forward partial sum
	long int z_backward;	// Maximum backward partial sum
};


/*
 * Static const variables declarations
 */
static const enum test test_num = TEST_CUSUM;	// This test number


/*
 * Forward static function declarations
 */
static double compute_pi_value(struct state *state, long int z);
static bool CumulativeSums_print_stat(FILE * stream, struct state *state, struct CumulativeSums_private_stats *stat,
				      double p_value, double rev_p_value);
static bool CumulativeSums_print_p_value(FILE * stream, double p_value);
static void CumulativeSums_metric_print(struct state *state, long int sampleCount, long int toolow,
					long int *freqPerBin, int index);


/*
 * CumulativeSums_init - initialize the Cumulative Sums test
 *
 * given:
 *      state           // run state to test under
 *
 * This function is called for each and every iteration noted in state->tp.numOfBitStreams.
 *
 * NOTE: The initialize function must be called first.
 */
void
CumulativeSums_init(struct state *state)
{
	long int n;		// Length of a single bit stream

	/*
	 * Check preconditions (firewall)
	 */
	if (state == NULL) {
		err(30, __func__, "state arg is NULL");
	}
	if (state->testVector[test_num] != true) {
		dbg(DBG_LOW, "init driver interface for %s[%d] called when test vector was false", state->testNames[test_num],
		    test_num);
		return;
	}
	if (state->cSetup != true) {
		err(30, __func__, "test constants not setup prior to calling %s for %s[%d]",
		    __func__, state->testNames[test_num], test_num);
	}

	/*
	 * Collect parameters from state
	 */
	n = state->tp.n;

	/*
	 * Disable test if conditions do not permit this test from being run
	 */
	if (n < MIN_LENGTH_CUSUM) {
		warn(__func__, "disabling test %s[%d]: requires bitcount(n): %ld >= %d",
		     state->testNames[test_num], test_num, n, MIN_LENGTH_CUSUM);
		state->testVector[test_num] = false;
		return;
	}

	/*
	 * Create working sub-directory if forming files such as results.txt and stats.txt
	 */
	if (state->resultstxtFlag == true) {
		state->subDir[test_num] = precheckSubdir(state, state->testNames[test_num]);
		dbg(DBG_HIGH, "test %s[%d] will use subdir: %s", state->testNames[test_num], test_num, state->subDir[test_num]);
	}

	/*
	 * Allocate dynamic arrays
	 */
	if (state->resultstxtFlag == true) {
		state->stats[test_num] = create_dyn_array(sizeof(struct CumulativeSums_private_stats),
							  DEFAULT_CHUNK, state->tp.numOfBitStreams, false);	// stats.txt
	}
	state->p_val[test_num] = create_dyn_array(sizeof(double),
						  DEFAULT_CHUNK, 2 * state->tp.numOfBitStreams, false);	// results.txt

	/*
	 * Determine format of data*.txt filenames based on state->partitionCount[test_num]
	 * NOTE: If we are not partitioning the p_values, no data*.txt filenames are needed
	 */
	if (state->partitionCount[test_num] > 1) {
		state->datatxt_fmt[test_num] = data_filename_format(state->partitionCount[test_num]);
		dbg(DBG_HIGH, "%s[%d] will form data*.txt filenames with the following format: %s",
		    state->testNames[test_num], test_num, state->datatxt_fmt[test_num]);
	} else {
		state->datatxt_fmt[test_num] = NULL;
		dbg(DBG_HIGH, "%s[%d] not partitioning, no data*.txt filename format", state->testNames[test_num], test_num);
	}

	return;
}


/*
 * CumulativeSums_iterate - iterate one bit stream for Cumulative Sums test
 *
 * given:
 *      state           // run state to test under
 *
 * This function is called for each and every iteration noted in state->tp.numOfBitStreams.
 *
 * NOTE: The initialize function must be called first.
 */
void
CumulativeSums_iterate(struct thread_state *thread_state)
{
	struct CumulativeSums_private_stats stat;	// Stats for this iteration
	long int n;			// Length of a single bit stream
	long int S;			// Variable used to store the forward partial sums
	long int S_max;			// Maximum forward partial sum
	long int S_min;			// Minimum forward partial sum
	double p_value_forward;		// p_value for forward test
	double p_value_backward;	// p_value for backward test
	long int k;

	/*
	 * Check preconditions (firewall)
	 */
	if (thread_state == NULL) {
		err(31, __func__, "thread_state arg is NULL");
	}
	struct state *state = thread_state->global_state;
	if (state == NULL) {
		err(31, __func__, "state arg is NULL");
	}
	if (state->testVector[test_num] != true) {
		dbg(DBG_LOW, "iterate function[%d] %s called when test vector was false", test_num, __func__);
		return;
	}
	if (state->epsilon == NULL) {
		err(31, __func__, "state->epsilon is NULL");
	}
	if (state->epsilon[thread_state->thread_id] == NULL) {
		err(31, __func__, "state->epsilon[%ld] is NULL", thread_state->thread_id);
	}
	if (state->cSetup != true) {
		err(31, __func__, "test constants not setup prior to calling %s for %s[%d]",
		    __func__, state->testNames[test_num], test_num);
	}

	/*
	 * Collect parameters from state
	 */
	n = state->tp.n;

	/*
	 * Zeroize stats before performing the test
	 */
	memset(&stat, 0, sizeof(stat));

	/*
	 * Step 2a: find the maximum and the minimum values of the forward partial sums.
	 *
	 * It can be proven that the maximum and the minimum values of the
	 * backward partial sums are complementary to the ones of the
	 * forward partial sums in relation to the total final sum.
	 *
	 * In other words, if S_max and S_min are the maximum and the minimum forward
	 * partial sums and S is the final total sum of the adjusted values of epsilon,
	 * the maximum and the minimum backwards partial sums will be respectively
	 * (S - S_min) and (S - S_max).
	 */
	S = 0;
	S_max = 0;
	S_min = 0;
	for (k = 0; k < n; k++) {
		(state->epsilon[thread_state->thread_id][k] != 0) ? S++ : S--;
		S_max = MAX(S, S_max);
		S_min = MIN(S, S_min);
	}

	/*
	 * Step 3: compute the test statistics
	 * We are applying the test both in forward and backward mode,
	 * so we have two separate test statistics.
	 */
	stat.z_backward = (S_max - S > S - S_min) ? S_max - S : S - S_min;
	stat.z_forward = (S_max > -S_min) ? S_max : -S_min;

	/*
	 * Step 4: compute test p-values
	 */
	p_value_forward = compute_pi_value(state, stat.z_forward);
	p_value_backward = compute_pi_value(state, stat.z_backward);

	/*
	 * Lock mutex before making changes to the shared state
	 */
	if (thread_state->mutex != NULL) {
		pthread_mutex_lock(thread_state->mutex);
	}

	/*
	 * Record success or failure for this iteration (forward test)
	 */
	state->count[test_num]++;	// Count this iteration
	state->valid[test_num]++;	// Count this valid iteration
	if (isNegative(p_value_forward)) {
		state->failure[test_num]++;	// Bogus p_value < 0.0 treated as a failure
		stat.success_forward = false;	// FAILURE
		warn(__func__, "iteration %ld of test %s[%d] produced bogus p_value: %f < 0.0\n",
		     thread_state->iteration_being_done + 1, state->testNames[test_num], test_num, p_value_forward);
	} else if (isGreaterThanOne(p_value_forward)) {
		state->failure[test_num]++;	// Bogus p_value > 1.0 treated as a failure
		stat.success_forward = false;	// FAILURE
		warn(__func__, "iteration %ld of test %s[%d] produced bogus p_value: %f > 1.0\n",
		     thread_state->iteration_being_done + 1, state->testNames[test_num], test_num, p_value_forward);
	} else if (p_value_forward < state->tp.alpha) {
		state->valid_p_val[test_num]++;	// Valid p_value in [0.0, 1.0] range
		state->failure[test_num]++;	// Valid p_value but too low is a failure
		stat.success_forward = false;	// FAILURE
	} else {
		state->valid_p_val[test_num]++;	// Valid p_value in [0.0, 1.0] range
		state->success[test_num]++;	// Valid p_value not too low is a success
		stat.success_forward = true;	// SUCCESS
	}

	/*
	 * Record success or failure for this iteration (backward test)
	 */
	state->count[test_num]++;	// Count this iteration
	state->valid[test_num]++;	// Count this valid iteration
	if (isNegative(p_value_backward)) {
		state->failure[test_num]++;	// Bogus backward p_value < 0.0 treated as a failure
		stat.success_backward = false;	// FAILURE
		warn(__func__, "iteration %ld of backward test %s[%d] produced bogus p_value: %f < 0.0\n",
		     thread_state->iteration_being_done + 1, state->testNames[test_num], test_num, p_value_backward);
	} else if (isGreaterThanOne(p_value_backward)) {
		state->failure[test_num]++;	// Bogus backward p_value > 1.0 treated as a failure
		stat.success_backward = false;	// FAILURE
		warn(__func__, "iteration %ld of backward test %s[%d] produced bogus p_value: %f > 1.0\n",
		     thread_state->iteration_being_done + 1, state->testNames[test_num], test_num, p_value_backward);
	} else if (p_value_backward < state->tp.alpha) {
		state->valid_p_val[test_num]++;	// Valid backward p_value in [0.0, 1.0] range
		state->failure[test_num]++;	// Valid backward p_value but too low is a failure
		stat.success_backward = false;	// FAILURE
	} else {
		state->valid_p_val[test_num]++;	// Valid backward p_value in [0.0, 1.0] range
		state->success[test_num]++;	// Valid backward p_value not too low is a success
		stat.success_backward = true;	// SUCCESS
	}

	/*
	 * Record values computed during this iteration
	 */
	if (state->resultstxtFlag == true) {
		append_value(state->stats[test_num], &stat);
	}
	append_value(state->p_val[test_num], &p_value_forward);
	append_value(state->p_val[test_num], &p_value_backward);

	/*
	 * Unlock mutex after making changes to the shared state
	 */
	if (thread_state->mutex != NULL) {
		pthread_mutex_unlock(thread_state->mutex);
	}

	return;
}


/*
 * compute_pi_value - compute pi-value for the given test statistic
 *
 * given:
 *      state           // run state to test under
 *      z		// test statistic (either the mode 0 or 1 one)
 *
 * This auxiliary function computes the pi-value for the Cumulative Sums test.
 */
static double
compute_pi_value(struct state *state, long int z)
{
	long int n;		// Length of a single bit stream
	double sum1;		// First summation of the p-value formula
	double sum2;		// Second summation of the p-value formula
	long int k;

	/*
	 * Check preconditions (firewall)
	 */
	if (state == NULL) {
		err(38, __func__, "state arg is NULL");
	}
	if (z < 0) {
		err(38, __func__, "z is negative: requires z: %ld >= 0", z);
	}

	/*
	 * Collect parameters from state
	 */
	n = state->tp.n;

	/*
	 * Step 4a: compute terms needed for the test p-value
	 */
	sum1 = 0.0;
	for (k = (-n / z + 1) / 4; k <= (n / z - 1) / 4; k++) {
		sum1 += cephes_normal(((4 * k + 1) * z) / state->c.sqrtn);
		sum1 -= cephes_normal(((4 * k - 1) * z) / state->c.sqrtn);
	}
	sum2 = 0.0;
	for (k = (-n / z - 3) / 4; k <= (n / z - 1) / 4; k++) {
		sum2 += cephes_normal(((4 * k + 3) * z) / state->c.sqrtn);
		sum2 -= cephes_normal(((4 * k + 1) * z) / state->c.sqrtn);
	}

	/*
	 * Step 4b: compute the test p-value
	 */
	return 1.0 - sum1 + sum2;
}


/*
 * CumulativeSums_print_stat - print private_stats information to the end of an open file
 *
 * given:
 *      stream          // open writable FILE stream
 *      state           // run state to test under
 *      stat            // struct CumulativeSums_private_stats for format and print
 *      p_value         // p_value iteration test result(s) - forward direction
 *      rev_p_value     // p_value iteration test result(s) - backward direction
 *
 * returns:
 *      true --> no errors
 *      false --> an I/O error occurred
 */
static bool
CumulativeSums_print_stat(FILE * stream, struct state *state, struct CumulativeSums_private_stats *stat, double p_value,
			  double rev_p_value)
{
	int io_ret;		// I/O return status

	/*
	 * Check preconditions (firewall)
	 */
	if (stream == NULL) {
		err(32, __func__, "stream arg is NULL");
	}
	if (state == NULL) {
		err(32, __func__, "state arg is NULL");
	}
	if (stat == NULL) {
		err(32, __func__, "stat arg is NULL");
	}
	if (p_value == NON_P_VALUE && stat->success_forward == true) {
		err(32, __func__, "p_value was set to NON_P_VALUE but stat->success_forward == true");
	}
	if (rev_p_value == NON_P_VALUE && stat->success_backward == true) {
		err(32, __func__, "rev_p_value was set to NON_P_VALUE but stat->success_backward == true");
	}

	/*
	 * Print stat to a file for the forward test
	 */
	if (state->legacy_output == true) {
		io_ret = fprintf(stream, "\t\t      CUMULATIVE SUMS (FORWARD) TEST\n");
		if (io_ret <= 0) {
			return false;
		}
		io_ret = fprintf(stream, "\t\t-------------------------------------------\n");
		if (io_ret <= 0) {
			return false;
		}
		io_ret = fprintf(stream, "\t\tCOMPUTATIONAL INFORMATION:\n");
		if (io_ret <= 0) {
			return false;
		}
	} else {
		io_ret = fprintf(stream, "\t\t      Cumulative sums forward test\n");
		if (io_ret <= 0) {
			return false;
		}
	}
	io_ret = fprintf(stream, "\t\t-------------------------------------------\n");
	if (io_ret <= 0) {
		return false;
	}
	io_ret = fprintf(stream, "\t\t(a) The maximum partial sum = %ld\n", stat->z_forward);
	if (io_ret <= 0) {
		return false;
	}
	io_ret = fprintf(stream, "\t\t-------------------------------------------\n");
	if (io_ret <= 0) {
		return false;
	}
	if (stat->success_forward == true) {
		io_ret = fprintf(stream, "SUCCESS\t\tp_value = %f\n\n", p_value);
		if (io_ret <= 0) {
			return false;
		}
	} else if (p_value == NON_P_VALUE) {
		io_ret = fprintf(stream, "FAILURE\t\tp_value = __INVALID__\n\n");
		if (io_ret <= 0) {
			return false;
		}
	} else {
		io_ret = fprintf(stream, "FAILURE\t\tp_value = %f\n\n", p_value);
		if (io_ret <= 0) {
			return false;
		}
	}

	/*
	 * Print stat to a file for the backward test
	 */
	if (state->legacy_output == true) {
		io_ret = fprintf(stream, "\t\t      CUMULATIVE SUMS (BACKWARD) TEST\n");
		if (io_ret <= 0) {
			return false;
		}
		io_ret = fprintf(stream, "\t\t-------------------------------------------\n");
		if (io_ret <= 0) {
			return false;
		}
		io_ret = fprintf(stream, "\t\tCOMPUTATIONAL INFORMATION:\n");
		if (io_ret <= 0) {
			return false;
		}
	} else {
		io_ret = fprintf(stream, "\t\t      Cumulative sums backward test\n");
		if (io_ret <= 0) {
			return false;
		}
	}
	io_ret = fprintf(stream, "\t\t-------------------------------------------\n");
	if (io_ret <= 0) {
		return false;
	}
	io_ret = fprintf(stream, "\t\t(a) The maximum partial sum = %ld\n", stat->z_backward);
	if (io_ret <= 0) {
		return false;
	}
	io_ret = fprintf(stream, "\t\t-------------------------------------------\n");
	if (io_ret <= 0) {
		return false;
	}
	if (stat->success_backward == true) {
		io_ret = fprintf(stream, "SUCCESS\t\tp_value = %f\n\n", rev_p_value);
		if (io_ret <= 0) {
			return false;
		}
	} else if (rev_p_value == NON_P_VALUE) {
		io_ret = fprintf(stream, "FAILURE\t\tp_value = __INVALID__\n\n");
		if (io_ret <= 0) {
			return false;
		}
	} else {
		io_ret = fprintf(stream, "FAILURE\t\tp_value = %f\n\n", rev_p_value);
		if (io_ret <= 0) {
			return false;
		}
	}

	/*
	 * All printing successful
	 */
	return true;
}


/*
 * CumulativeSums_print_p_value - print p_value information to the end of an open file
 *
 * given:
 *      stream          // open writable FILE stream
 *      stat            // struct CumulativeSums_private_stats for format and print
 *      p_value         // p_value iteration test result(s)
 *
 * returns:
 *      true --> no errors
 *      false --> an I/O error occurred
 */
static bool
CumulativeSums_print_p_value(FILE * stream, double p_value)
{
	int io_ret;		// I/O return status

	/*
	 * Check preconditions (firewall)
	 */
	if (stream == NULL) {
		err(33, __func__, "stream arg is NULL");
	}

	/*
	 * Print p_value to a file
	 */
	if (p_value == NON_P_VALUE) {
		io_ret = fprintf(stream, "__INVALID__\n");
		if (io_ret <= 0) {
			return false;
		}
	} else {
		io_ret = fprintf(stream, "%f\n", p_value);
		if (io_ret <= 0) {
			return false;
		}
	}

	/*
	 * All printing successful
	 */
	return true;
}


/*
 * CumulativeSums_print - print to results.txt, data*.txt, stats.txt for all iterations
 *
 * given:
 *      state           // run state to test under
 *
 * This function is called for once to print dynamic arrays into
 * results.txt, data*.txt, stats.txt.
 *
 * NOTE: The initialize and iterate functions must be called before this function is called.
 */
void
CumulativeSums_print(struct state *state)
{
	struct CumulativeSums_private_stats *stat;	// Pointer to statistics of an iteration
	double p_value;			// p_value iteration test result(s) - forward direction
	double rev_p_value;		// p_value iteration test result(s) - backward direction
	FILE *stats = NULL;		// Open stats.txt file
	FILE *results = NULL;		// Open results.txt file
	FILE *data = NULL;		// Open data*.txt file
	char *stats_txt = NULL;		// Pathname for stats.txt
	char *results_txt = NULL;	// Pathname for results.txt
	char *data_txt = NULL;		// Pathname for data*.txt
	char data_filename[BUFSIZ + 1];	// Basename for a given data*.txt pathname
	bool ok;			// true -> I/O was OK
	int snprintf_ret;		// snprintf return value
	int io_ret;			// I/O return status
	long int i;
	long int j;

	/*
	 * Check preconditions (firewall)
	 */
	if (state == NULL) {
		err(34, __func__, "state arg is NULL");
	}
	if (state->testVector[test_num] != true) {
		dbg(DBG_HIGH, "Print driver interface for %s[%d] called when test vector was false", state->testNames[test_num],
		    test_num);
		return;
	}
	if (state->resultstxtFlag == false) {
		dbg(DBG_HIGH, "Print driver interface for %s[%d] was not enabled with -s", state->testNames[test_num], test_num);
		return;
	}
	if (state->partitionCount[test_num] < 1) {
		err(34, __func__,
		    "print driver interface for %s[%d] called with state.partitionCount: %d < 0",
		    state->testNames[test_num], test_num, state->partitionCount[test_num]);
	}
	if (state->p_val[test_num]->count != (state->tp.numOfBitStreams * state->partitionCount[test_num])) {
		err(34, __func__,
		    "print driver interface for %s[%d] called with p_val count: %ld != %ld*%d=%ld",
		    state->testNames[test_num], test_num, state->p_val[test_num]->count,
		    state->tp.numOfBitStreams, state->partitionCount[test_num],
		    state->tp.numOfBitStreams * state->partitionCount[test_num]);
	}
	if (state->datatxt_fmt[test_num] == NULL) {
		err(34, __func__, "format for data0*.txt filename is NULL");
	}

	/*
	 * Open stats.txt file
	 */
	stats_txt = filePathName(state->subDir[test_num], "stats.txt");
	dbg(DBG_HIGH, "about to open/truncate: %s", stats_txt);
	stats = openTruncate(stats_txt);

	/*
	 * Open results.txt file
	 */
	results_txt = filePathName(state->subDir[test_num], "results.txt");
	dbg(DBG_HIGH, "about to open/truncate: %s", results_txt);
	results = openTruncate(results_txt);

	/*
	 * Write results.txt and stats.txt files
	 */
	for (i = 0; i < state->stats[test_num]->count; ++i) {

		/*
		 * Locate stat for this iteration
		 */
		stat = addr_value(state->stats[test_num], struct CumulativeSums_private_stats, i);

		/*
		 * Get p_value pair (forward and backward) for this iteration
		 */
		p_value = get_value(state->p_val[test_num], double, i * 2);
		rev_p_value = get_value(state->p_val[test_num], double, (i * 2) + 1);

		/*
		 * Print stat to stats.txt
		 */
		errno = 0;	// paranoia
		ok = CumulativeSums_print_stat(stats, state, stat, p_value, rev_p_value);
		if (ok == false) {
			errp(34, __func__, "error in writing to %s", stats_txt);
		}

		/*
		 * Print p_value and rev_p_value to results.txt
		 */
		errno = 0;	// paranoia
		ok = CumulativeSums_print_p_value(results, p_value);
		if (ok == false) {
			errp(34, __func__, "error in writing to %s", results_txt);
		}
		errno = 0;	// paranoia
		ok = CumulativeSums_print_p_value(results, rev_p_value);
		if (ok == false) {
			errp(34, __func__, "error in writing to %s", results_txt);
		}
	}

	/*
	 * Flush and close stats.txt, free pathname
	 */
	errno = 0;		// paranoia
	io_ret = fflush(stats);
	if (io_ret != 0) {
		errp(34, __func__, "error flushing to: %s", stats_txt);
	}
	errno = 0;		// paranoia
	io_ret = fclose(stats);
	if (io_ret != 0) {
		errp(34, __func__, "error closing: %s", stats_txt);
	}
	free(stats_txt);
	stats_txt = NULL;

	/*
	 * Flush and close results.txt, free pathname
	 */
	errno = 0;		// paranoia
	io_ret = fflush(results);
	if (io_ret != 0) {
		errp(34, __func__, "error flushing to: %s", results_txt);
	}
	errno = 0;		// paranoia
	io_ret = fclose(results);
	if (io_ret != 0) {
		errp(34, __func__, "error closing: %s", results_txt);
	}
	free(results_txt);
	results_txt = NULL;

	/*
	 * Write data*.txt for each data file if we need to partition results
	 */
	if (state->partitionCount[test_num] > 1) {
		for (j = 0; j < state->partitionCount[test_num]; ++j) {

			/*
			 * Form the data*.txt basename
			 */
			errno = 0;	// paranoia
			snprintf_ret = snprintf(data_filename, BUFSIZ, state->datatxt_fmt[test_num], j + 1);
			data_filename[BUFSIZ] = '\0';	// paranoia
			if (snprintf_ret <= 0 || snprintf_ret >= BUFSIZ || errno != 0) {
				errp(34, __func__, "snprintf failed for %d bytes for data%03ld.txt, returned: %d", BUFSIZ,
				     j + 1, snprintf_ret);
			}

			/*
			 * Form the data*.txt filename
			 */
			data_txt = filePathName(state->subDir[test_num], data_filename);
			dbg(DBG_HIGH, "about to open/truncate: %s", data_txt);
			data = openTruncate(data_txt);

			/*
			 * Write this particular data*.txt filename
			 */
			if (j < state->p_val[test_num]->count) {
				for (i = j; i < state->p_val[test_num]->count; i += state->partitionCount[test_num]) {

					/*
					 * Get p_value for an iteration belonging to this data*.txt filename
					 */
					p_value = get_value(state->p_val[test_num], double, i);

					/*
					 * Print p_value to results.txt
					 */
					errno = 0;	// paranoia
					ok = CumulativeSums_print_p_value(data, p_value);
					if (ok == false) {
						errp(34, __func__, "error in writing to %s", data_txt);
					}
				}
			}

			/*
			 * Flush and close data*.txt, free pathname
			 */
			errno = 0;	// paranoia
			io_ret = fflush(data);
			if (io_ret != 0) {
				errp(34, __func__, "error flushing to: %s", data_txt);
			}
			errno = 0;	// paranoia
			io_ret = fclose(data);
			if (io_ret != 0) {
				errp(34, __func__, "error closing: %s", data_txt);
			}
			free(data_txt);
			data_txt = NULL;
		}
	}

	return;
}


/*
 * CumulativeSums_metric_print - print uniformity and proportional information for a tallied count
 *
 * given:
 *      state           	// run state to test under
 *      sampleCount             // Number of bitstreams in which we counted p_values
 *      toolow                  // p_values that were below alpha
 *      freqPerBin              // uniformity frequency bins
 */
static void
CumulativeSums_metric_print(struct state *state, long int sampleCount, long int toolow, long int *freqPerBin, int index)
{
	long int passCount;			// p_values that pass
	double p_hat;				// 1 - alpha
	double proportion_threshold_max;	// When passCount is too high
	double proportion_threshold_min;	// When passCount is too low
	double chi2;				// Sum of chi^2 for each tenth
	double uniformity;			// Uniformity of frequency bins
	double expCount;			// Sample size divided by frequency bin count
	int io_ret;				// I/O return status
	long int i;

	/*
	 * Check preconditions (firewall)
	 */
	if (state == NULL) {
		err(35, __func__, "state arg is NULL");
	}
	if (freqPerBin == NULL) {
		err(35, __func__, "freqPerBin arg is NULL");
	}

	/*
	 * Determine the number tests that passed
	 */
	if ((sampleCount <= 0) || (sampleCount < toolow)) {
		passCount = 0;
	} else {
		passCount = sampleCount - toolow;
	}

	/*
	 * Determine proportion thresholds
	 */
	p_hat = 1.0 - state->tp.alpha;
	proportion_threshold_max = (p_hat + 3.0 * sqrt((p_hat * state->tp.alpha) / sampleCount)) * sampleCount;
	proportion_threshold_min = (p_hat - 3.0 * sqrt((p_hat * state->tp.alpha) / sampleCount)) * sampleCount;

	/*
	 * Compute uniformity p-value
	 */
	chi2 = 0.0;
	expCount = (double)sampleCount / state->tp.uniformity_bins;
	if (expCount <= 0.0) {
		// Not enough samples for uniformity check
		uniformity = 0.0;
	} else {
		// Sum chi squared of the frequency bins
		for (i = 0; i < state->tp.uniformity_bins; ++i) {
			chi2 += (freqPerBin[i] - expCount) * (freqPerBin[i] - expCount) / expCount;
		}
		// Uniformity threshold level
		uniformity = cephes_igamc((state->tp.uniformity_bins - 1.0) / 2.0, chi2 / 2.0);
	}

	/*
	 * Save or print results
	 */
	if (state->legacy_output == true) {

		/*
		 * Output uniformity results in traditional format to finalAnalysisReport.txt
		 */
		for (i = 0; i < state->tp.uniformity_bins; ++i) {
			fprintf(state->finalRept, "%3ld ", freqPerBin[i]);
		}
		if (expCount <= 0.0) {
			// Not enough samples for uniformity check
			fprintf(state->finalRept, "    ----    ");
			dbg(DBG_HIGH, "too few iterations for uniformity check on %s", state->testNames[test_num]);
		} else if (uniformity < state->tp.uniformity_level) {
			// Uniformity failure (the uniformity p-value is smaller than the minimum uniformity_level (default 0.0001)
			fprintf(state->finalRept, " %8.6f * ", uniformity);
			dbg(DBG_HIGH, "metrics detected uniformity failure for %s", state->testNames[test_num]);
		} else {
			// Uniformity success
			fprintf(state->finalRept, " %8.6f   ", uniformity);
			dbg(DBG_HIGH, "metrics detected uniformity success for %s", state->testNames[test_num]);
		}

		/*
		 * Output proportional results in traditional format to finalAnalysisReport.txt
		 */
		if (sampleCount == 0) {
			// Not enough samples for proportional check
			fprintf(state->finalRept, " ------     %s\n", state->testNames[test_num]);
			dbg(DBG_HIGH, "too few samples for proportional check on %s", state->testNames[test_num]);
		} else if ((passCount < proportion_threshold_min) || (passCount > proportion_threshold_max)) {
			// Proportional failure
			fprintf(state->finalRept, "%4ld/%-4ld *	 %s\n", passCount, sampleCount, state->testNames[test_num]);
			dbg(DBG_HIGH, "metrics detected proportional failure for %s", state->testNames[test_num]);
		} else {
			// Proportional success
			fprintf(state->finalRept, "%4ld/%-4ld	 %s\n", passCount, sampleCount, state->testNames[test_num]);
			dbg(DBG_HIGH, "metrics detected proportional success for %s", state->testNames[test_num]);
		}

		/*
		 * Flush the output file buffer
		 */
		errno = 0;                // paranoia
		io_ret = fflush(state->finalRept);
		if (io_ret != 0) {
			errp(35, __func__, "error flushing to: %s", state->finalReptPath);
		}

	} else {
		bool uniformity_passed = true;
		bool proportion_passed = true;

		/*
		 * Check uniformity results
		 */
		if (expCount <= 0.0 || uniformity < state->tp.uniformity_level) {
			// Uniformity failure or not enough samples for uniformity check
			uniformity_passed = false;
			dbg(DBG_HIGH, "metrics detected uniformity failure for %s", state->testNames[test_num]);
		}

		/*
		 * Check proportional results
		 */
		if (sampleCount == 0 || (passCount < proportion_threshold_min) || (passCount > proportion_threshold_max)) {
			// Proportional failure or not enough samples for proportional check
			proportion_passed = false;
			dbg(DBG_HIGH, "metrics detected proportional failure for %s", state->testNames[test_num]);
		}

		if (proportion_passed == false && uniformity_passed == false) {
			state->metric_results.cusum[index] = FAILED_BOTH;
		} else if (proportion_passed == false) {
			state->metric_results.cusum[index] = FAILED_PROPORTION;
		} else if (uniformity_passed == false) {
			state->metric_results.cusum[index] = FAILED_UNIFORMITY;
		} else {
			state->metric_results.cusum[index] = PASSED_BOTH;
			state->successful_tests++;
		}
	}

	return;
}


/*
 * CumulativeSums_metrics - uniformity and proportional analysis of a test
 *
 * given:
 *      state           // run state to test under
 *
 * This function is called once to complete the test analysis for all iterations.
 *
 * NOTE: The initialize and iterate functions must be called before this function is called.
 */
void
CumulativeSums_metrics(struct state *state)
{
	long int sampleCount;	// Number of bitstreams in which we will count p_values
	long int toolow;	// p_values that were below alpha
	double p_value;		// p_value iteration test result(s)
	long int *freqPerBin;	// Uniformity frequency bins
	long int i;
	long int j;

	/*
	 * Check preconditions (firewall)
	 */
	if (state == NULL) {
		err(36, __func__, "state arg is NULL");
	}
	if (state->testVector[test_num] != true) {
		dbg(DBG_LOW, "metrics driver interface for %s[%d] called when test vector was false", state->testNames[test_num],
		    test_num);
		return;
	}
	if (state->partitionCount[test_num] < 1) {
		err(36, __func__,
		    "metrics driver interface for %s[%d] called with state.partitionCount: %d < 0",
		    state->testNames[test_num], test_num, state->partitionCount[test_num]);
	}
	if (state->p_val[test_num]->count != (state->tp.numOfBitStreams * state->partitionCount[test_num])) {
		warn(__func__,
		     "metrics driver interface for %s[%d] called with p_val length: %ld != bit streams: %ld",
		     state->testNames[test_num], test_num, state->p_val[test_num]->count,
		     state->tp.numOfBitStreams * state->partitionCount[test_num]);
	}

	/*
	 * Allocate uniformity frequency bins
	 */
	freqPerBin = malloc(state->tp.uniformity_bins * sizeof(freqPerBin[0]));
	if (freqPerBin == NULL) {
		errp(36, __func__, "cannot malloc of %ld elements of %ld bytes each for freqPerBin",
		     state->tp.uniformity_bins, sizeof(long int));
	}

	/*
	 * Print for each partition (or the whole set of p_values if partitionCount is 1)
	 */
	for (j = 0; j < state->partitionCount[test_num]; ++j) {

		/*
		 * Set counters to zero
		 */
		toolow = 0;
		sampleCount = 0;
		memset(freqPerBin, 0, state->tp.uniformity_bins * sizeof(freqPerBin[0]));

		/*
		 * Tally p_value
		 */
		for (i = j; i < state->p_val[test_num]->count; i += state->partitionCount[test_num]) {

			// Get the iteration p_value
			p_value = get_value(state->p_val[test_num], double, i);
			if (p_value == NON_P_VALUE) {
				continue;	// the test was not possible for this iteration
			}
			// Case: random excursion test
			if (state->is_excursion[test_num] == true) {
				// Random excursion tests only sample > 0 p_values
				if (p_value > 0.0) {
					++sampleCount;
				} else {
					// Ignore p_value of 0 for random excursion tests
					continue;
				}
			}
			// Case: general (non-random excursion) test
			else {
				// All other tests count all p_values
				++sampleCount;
			}

			// Count the number of p_values below alpha
			if (p_value < state->tp.alpha) {
				++toolow;
			}
			// Tally the p_value in a uniformity bin
			if (p_value >= 1.0) {
				++freqPerBin[state->tp.uniformity_bins - 1];
			} else if (p_value >= 0.0) {
				++freqPerBin[(int) floor(p_value * (double) state->tp.uniformity_bins)];
			} else {
				++freqPerBin[0];
			}
		}

		/*
		 * Print uniformity and proportional information for a tallied count
		 */
		CumulativeSums_metric_print(state, sampleCount, toolow, freqPerBin, j);

		/*
		 * Track maximum samples
		 */
		if (state->is_excursion[test_num] == true) {
			if (sampleCount > state->maxRandomExcursionSampleSize) {
				state->maxRandomExcursionSampleSize = sampleCount;
			}
		} else {
			if (sampleCount > state->maxGeneralSampleSize) {
				state->maxGeneralSampleSize = sampleCount;
			}
		}
	}

	/*
	 * Free allocated storage
	 */
	free(freqPerBin);
	freqPerBin = NULL;

	return;
}


/*
 * CumulativeSums_destroy - post process results for this text
 *
 * given:
 *      state           // run state to test under
 *
 * This function is called once to cleanup any storage or state
 * associated with this test.
 */
void
CumulativeSums_destroy(struct state *state)
{
	/*
	 * Check preconditions (firewall)
	 */
	if (state == NULL) {
		err(37, __func__, "state arg is NULL");
	}
	if (state->testVector[test_num] != true) {
		dbg(DBG_LOW, "destroy function[%d] %s called when test vector was false", test_num, __func__);
		return;
	}

	/*
	 * Free dynamic arrays
	 */
	if (state->stats[test_num] != NULL) {
		free_dyn_array(state->stats[test_num]);
		free(state->stats[test_num]);
		state->stats[test_num] = NULL;
	}
	if (state->p_val[test_num] != NULL) {
		free_dyn_array(state->p_val[test_num]);
		free(state->p_val[test_num]);
		state->p_val[test_num] = NULL;
	}

	/*
	 * Free other test storage
	 */
	if (state->datatxt_fmt[test_num] != NULL) {
		free(state->datatxt_fmt[test_num]);
		state->datatxt_fmt[test_num] = NULL;
	}
	if (state->subDir[test_num] != NULL) {
		free(state->subDir[test_num]);
		state->subDir[test_num] = NULL;
	}

	return;
}
