/* Match every k-character snippet of the query_doc document
	 among a collection of documents doc1, doc2, ....

	 ./rkmatch snippet_size query_doc doc1 [doc2...]

*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <strings.h>
#include <assert.h>
#include <time.h>

#include "bloom.h"

enum algotype { EXACT=0, SIMPLE, RK, RKBATCH};

/* a large prime for RK hash (BIG_PRIME*256 does not overflow)*/
long long BIG_PRIME = 5003943032159437; 

/* constants used for printing debug information */
const int PRINT_RK_HASH = 5;
const int PRINT_BLOOM_BITS = 160;


long long
timediff(struct timespec ts, struct timespec ts0)
{
	return (ts.tv_sec-ts0.tv_sec)*1000000+(ts.tv_nsec-ts0.tv_nsec)/1000;
}

/* modulo addition */
long long
madd(long long a, long long b)
{
	return ((a+b)>BIG_PRIME?(a+b-BIG_PRIME):(a+b));
}

/* modulo substraction */
long long
mdel(long long a, long long b)
{
	return ((a>b)?(a-b):(a+BIG_PRIME-b));
}

/* modulo multiplication*/
long long
mmul(long long a, long long b)
{
	return ((a*b) % BIG_PRIME);
}

/* read the entire content of the file 'fname' into a 
	 character array allocated by this procedure.
	 Upon return, *doc contains the address of the character array
	 *doc_len contains the length of the array
	 */
void
read_file(const char *fname, unsigned char **doc, int *doc_len) 
{
	struct stat st;
	int fd;
	int n = 0;

	fd = open(fname, O_RDONLY);
	if (fd < 0) {
		perror("read_file: open ");
		exit(1);
	}

	if (fstat(fd, &st) != 0) {
		perror("read_file: fstat ");
		exit(1);
	}

	*doc = (unsigned char *)malloc(st.st_size);
	if (!(*doc)) {
		fprintf(stderr, " failed to allocate %d bytes. No memory\n", (int)st.st_size);
		exit(1);
	}

	n = read(fd, *doc, st.st_size);
	if (n < 0) {
		perror("read_file: read ");
		exit(1);
	}else if (n != st.st_size) {
		fprintf(stderr,"read_file: short read!\n");
		exit(1);
	}
	
	close(fd);
	*doc_len = n;
}

/* The normalize procedure normalizes a character array of size len 
   according to the following rules:
	 1) turn all upper case letters into lower case ones
	 2) turn any white-space character into a space character and, 
	    shrink any n>1 consecutive whitespace characters to exactly 1 whitespace

	 When the procedure returns, the character array buf contains the newly 
     normalized string and the return value is the new length of the normalized string.

     hint: you may want to use C library function isupper, isspace, tolower
     do "man isupper"
*/
int
normalize(unsigned char *buf,	/* The character array contains the string to be normalized*/
					int len	    /* the size of the original character array */)
{
	int j=0;
	int i=0;
	int removeSpace = 1;

	while ( i < len ) {
		if ( isspace(buf[i]) == 0 ) {
			// copies a character to it's new position
			buf[j] = tolower(buf[i]);
			j++;
			removeSpace = 0;
		}
		else if ( removeSpace == 0 ) {
			// copies only first whitespace to it's new position
			buf[j] = ' ';
			j++;
			removeSpace = 1;
		}
		i++;
	}

	// removes a whitespace at the end of the string
	if ( isspace(buf[j-1]) != 0 ) {
		j--;
	}

	buf[j] = 0;

    return j;
}

int
exact_match(const unsigned char *qs, int m, 
        const unsigned char *ts, int n)
{
	int i;
	if ( m != n) {
		return 0;
	}
	for (i=0; i < m; i++) {
		if ( qs[i] != ts[i] ) {
			return 0;
		}
	}
	return 1;
}

/* check if a query string ps (of length k) appears 
	 in ts (of length n) as a substring 
	 If so, return 1. Else return 0
	 You may want to use the library function strncmp
	 */
int
simple_substr_match(const unsigned char *ps,	/* the query string */
						 int k, 					/* the length of the query string */
						 const unsigned char *ts,	/* the document string (Y) */ 
						 int n						/* the length of the document Y */)
{
	int i;
	// Looks for ps in ts character by character
    for(i=0; i < n - k; i++) {
		if (strncmp(ts+i, ps, k) == 0) {
			return 1;
		}
	}
	return 0;
}

// Calculates a hash for a string
long long getHash(const unsigned char *str,	/* the string */
								 int k 	/* the length of the string */)
{
	int i;
	long long hash = 0;
	for (i = 0; i < k; i++) {
		hash = madd(mmul(hash, 256), str[i]);
	}
	return hash;
}

/* Check if a query string ps (of length k) appears 
	 in ts (of length n) as a substring using the rabin-karp algorithm
	 If so, return 1. Else return 0


	 In addition, print the hash value of ps (on one line)
	 as well as the first 'PRINT_RK_HASH' hash values of ts (on another line)
	 Example:
	 $ ./rkmatch -t 1 -k 20 X Y
	 4537305142160169
	 1137948454218999 2816897116259975 4720517820514748 4092864945588237 3539905993503426 
	 2021356707496909
	 1137948454218999 2816897116259975 4720517820514748 4092864945588237 3539905993503426 
	 0 chunks matched (out of 2), percentage: 0.00

	Hint: Use "long long" type for the RK hash value.  Use printf("%lld", x) to print 
	out x of long long type.
	 */
int
rabin_karp_match(const unsigned char *ps,	/* the query string */
								 int k, 					/* the length of the query string */
								 const unsigned char *ts,	/* the document string (Y) */ 
								 int n						/* the length of the document Y */ )
{
	long long ps_hash = getHash(ps, k);
	long long y0 = getHash(ts, k);
	long long y = y0;
	int i;
	long long k_1_256 = 1 << (8*(k-1));
	
	// Prints hash values
	printf("%lld\n", ps_hash);
	printf("%lld ", y);
	for (i = 1; i < PRINT_RK_HASH; i++) {
		y = madd(mmul((long long)256, mdel(y, mmul(k_1_256, (long long)ts[i-1])) ), (long long)ts[i-1+k]);
		printf("%lld ", y);
	}
	printf("\n");
	
	y = y0;
	// Checks if ts starts with ps
	if ( (ps_hash == y) && (strncmp(ts, ps, k) == 0) ) {
		return 1;
	}
	// Looks for ps in ts
	for (i = 1; i < n; i++) {
		// Gets a new hash value
		y = madd(mmul(256, mdel(y, mmul(k_1_256, ts[i-1])) ), ts[i-1+k]);
		if ( (ps_hash == y) && (strncmp(ts+i, ps, k) == 0) ) {
			// Hashes are equal and substrings are equal too
			return 1;
		}
	}
	// No ps was found
	return 0;
}


/* Allocate a bitmap containing bsz bits for the bloom filter (using the malloc library function), 
   and insert all m/k RK hashes of qs into the bloom filter.  Compute each of the n-k+1 RK hashes 
   of ts and check if it's in the filter.  Specifically, you are expected to use the given procedure, 
   hash_i(i, p), to compute the i-th bloom filter hash value for the RK value p.  

   The function returns the total number of matched chunks. 

   The bloom filter implemention uses a character array to represent the bitmap.
   You are expected to use the character array in big-endian format. As an example,
   To set the 9-th bit of the bitmap to be "1", you should set the left-most
   bit of the second character in the character array to be "1".

   For testing purpose, you should print out the first PRINT_BLOOM_BITS bits 
   of bloom bitmap in hex after inserting all m/k chunks from qs.

   Hint: the printf statement for printing out one byte of the bitmap array in hex is 
   printf("%02x ", (unsigned char)bitmap[i])
 
   Example output:
   $./rkmatch -t 2 X Y
    00 04 10 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
    1.00 matched: 10 out of 10
	
   In the above example, the 14-th, and 20-th bits of the bloom filter are set to be "1"
 */
int
rabin_karp_batchmatch(int bsz, /* size of bitmap (in bits) to be used */
    int k, 					/* chunk length to be matched */
    const unsigned char *qs, /* query docoument (X)*/
    int m, 					/* query document length */ 
    const unsigned char *ts, /* to-be-matched document (Y) */
    int n 					/* to-be-matched document length*/)
{

    /* Your code here */
    return 0;
}

int 
main(int argc, char **argv)
{
	int k = 20; /* default match size is 20*/
	int which_algo = SIMPLE; /* default match algorithm is simple */

	unsigned char *qdoc, *doc; 
	int qdoc_len, doc_len;
	int i;
	int num_matched = 0;
	int c;

	/* Refuse to run on platform with a different size for long long*/
	assert(sizeof(long long) == 8);

	/*getopt is a C library function to parse command line options */
	while (( c = getopt(argc, argv, "t:k:q:")) != -1) {
		switch (c) 
		{
			case 't':
				/*optarg is a global variable set by getopt() 
					it now points to the text following the '-t' */
				which_algo = atoi(optarg);
				break;
			case 'k':
				k = atoi(optarg);
				break;
			case 'q':
				BIG_PRIME = atoi(optarg);
				break;
			default:
				fprintf(stderr,
						"Valid options are: -t <algo type> -k <match size> -q <prime modulus>\n");
				exit(1);
			}
	}

	/* optind is a global variable set by getopt() 
		 it now contains the index of the first argv-element 
		 that is not an option*/
	if (argc - optind < 1) {
		printf("Usage: ./rkmatch query_doc doc\n");
		exit(1);
	}

	/* argv[optind] contains the query_doc argument */
	read_file(argv[optind], &qdoc, &qdoc_len); 
//	printf("\nqdoc_len = %d\n", qdoc_len);
	qdoc_len = normalize(qdoc, qdoc_len);
//	printf("\nqdoc_len = %d\n", qdoc_len);

	/* argv[optind+1] contains the doc argument */
	read_file(argv[optind+1], &doc, &doc_len);
//	printf("\ndoc_len = %d\n", doc_len);
	doc_len = normalize(doc, doc_len);
//	printf("\ndoc_len = %d\n", doc_len);

	switch (which_algo) 
		{
            case EXACT:
                if (exact_match(qdoc, qdoc_len, doc, doc_len)) 
                    printf("Exact match\n");
                else
                    printf("Not an exact match\n");
                break;
			case SIMPLE:
				/* for each chunk of qdoc (out of qdoc_len/k chunks of qdoc, 
					 check if it appears in doc as a substring*/
				for (i = 0; (i+k) <= qdoc_len; i += k) {
					if (simple_substr_match(qdoc+i, k, doc, doc_len)) {
						num_matched++;
					}
				}
                printf("%d chunks matched (out of %d), percentage: %.2f\n", \
                       num_matched, qdoc_len/k, (double)num_matched/(qdoc_len/k));
				break;
			case RK:
				/* for each chunk of qdoc (out of qdoc_len/k in total), 
					 check if it appears in doc as a substring using 
				   the rabin-karp substring matching algorithm */
				   //printf("'%s'", doc);
				for (i = 0; (i+k) <= qdoc_len; i += k) {
					if (rabin_karp_match(qdoc+i, k, doc, doc_len)) {
						num_matched++;
					}
				}
                printf("%d chunks matched (out of %d), percentage: %.2f\n", \
                       num_matched, qdoc_len/k, (double)num_matched/(qdoc_len/k));
				break;
			case RKBATCH:
				/* match all qdoc_len/k chunks simultaneously (in batch) by using a bloom filter*/
				num_matched = rabin_karp_batchmatch(((qdoc_len*10/k)>>3)<<3, k, \
                        qdoc, qdoc_len, doc, doc_len);
                printf("%d chunks matched (out of %d), percentage: %.2f\n", \
                       num_matched, qdoc_len/k, (double)num_matched/(qdoc_len/k));
				break;
			default :
				fprintf(stderr,"Wrong algorithm type, choose from 0 1 2 3\n");
				exit(1);
		}
	

	free(qdoc);
	free(doc);

	return 0;
}
