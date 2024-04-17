/*****************************************************************************
 U T I L I T Y  F U N C T I O N  P R O T O T Y P E S
 *****************************************************************************/

/*
 * This code has been heavily modified by the following people:
 *
 *      Landon Curt Noll
 *      Tom Gilgan
 *      Riccardo Paccagnella
 *
 * See the README.txt and the initial comment in assess.c for more information.
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

#ifndef UTILITY_H
#   define UTILITY_H

extern long int getNumber(FILE * input, FILE * output);
extern void makePath(char *dir);
extern bool checkWritePermissions(char *dir);
extern FILE *openTruncate(char *filename);
extern char *filePathName(char *head, char *tail);
extern char *data_filename_format(int partitionCount);
extern void precheckPath(struct state *state, char *dir);
extern char *precheckSubdir(struct state *state, char *subdir);
extern long int str2longint(bool * success_p, char *string);
extern void generatorOptions(struct state *state);
extern void chooseTests(struct state *state);
extern void fixParameters(struct state *state);
extern bool copyBitsToEpsilon(struct state *state, long int thread_id, BYTE *x, long int xBitLength, long int *num_0s,
			      long int *num_1s, long int *bitsRead);
extern void invokeTestSuite(struct state *state);
extern void read_from_p_val_file(struct state *state);
extern void write_p_val_to_file(struct state *state);
extern void print_option_summary(struct state *state, char *where);
extern int sum_will_overflow_long(long int si_a, long int si_b);
extern int multiplication_will_overflow_long(long int si_a, long int si_b);
extern void getTimestamp(char *buf, size_t len);
extern void append_string_to_linked_list(struct Node **head, char* string);

extern int job_rank;

#endif				/* UTILITY_H */
