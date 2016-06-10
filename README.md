# RK-Lab
RK Lab: Approximiate document matching

Introduction
In many scenarios, we would like to know how “similar” two documents are are to each other. For example, search engines like Google or Bing need to group similar web pages together such that only one among a group of similar documents is displayed as part of the search result. As another example, professors can detect plagirism by checking how similar students’ handins are. We refer to the process of measuring document similarity as approximate matching. In this lab, you will write a program to approximately match one input file against another file. The goal is to get your hands dirty in programming using C, e.g. manipulating arrays, pointers, number and character representation, bit operations etc.

Approximate Matching

Checking whether a document is an exact duplicate of another is straightforward 1. By contrast, it is trickier to determine similarity. Let us first start with an inefficient but working algorithm that measures how well document X (of size m)) approximately-matches another document Y (of size n). The algorithm considers
every substring of length k in X and checks if that substring appears in document Y. For example, if the
content of X is “abcd” and k = 2, then the algorithm tries to match 3 substrings (“ab”, “bc”, “cd”) in Y. For
each substring matched, the algorithm increments a counter ctr. Since there are total m − k + 1 substrings
to match, the algorithm calculates the fraction of matched substrings, i.e. ctr , as the approximate match m−k+1
score. The more similar file X is to Y, the higher its final score. In particular, if file X is identical to Y, the final score would be 1.0.
The naive algorithm works, but is slow. In particular, the naive way of checking whether a string appears as a substring in Y is to check if that string matches a substring of length k at every position 0, 1, 2, ..., (n − k) in Y. Thus, each substring matching takes O(k ∗ n) times, and since there are a total of m − k + 1 substrings of X to be matched in Y, the total runtime would be O(k ∗ m ∗ n). This runtime is pretty bad and we will improve it greatly in this lab step by step.

Simple Approximate Matching

As the first optimization, we observe that it is not necessary to do substring matching for all m − k + 1 substrings of X. Rather, we simply “chop” X (conceptually) into ⌊ m ⌋ chunks and tries to match each chunk
k
in Y. For example, if the content of X is “abcd” and k = 2, the optimized algorithm only matches 2 chunks (“ab”, “cd”) instead of 3 substrings as in the original naive algorithm. Doing so cuts down the runtime by a factor of k to O(m ∗ n) 2. We refer to this version as the simple algorithm.
In addition to speed improvement, we also make the simple algorithm more robust to spurious differences between documents. In particular, we “normalize” a document by doing the following
1. Convert all upper case letters to lower case ones.
2. Convert different white space characters (e.g. carriage return, tab,...) into the space character, 3. Shrink any sequence of two or more space characters to exactly one space.
4. Remove any whitespace at the beginning and end of the text
As an example, if the original content of X is “I am             A\nDog” where       is the space character and \n is the new line character, the normalized content of X should be “i am a dog”.
Your job: Implement the simple approximate matching algorithm. The rkmatch.c file already contains a code skeleton with a number of helper procedures. Read rkmatch.c and make sure you understand the basic structure of the program. To implement the simple algorithm, you need to complete the procedures simple match and normalize.
The main procedure first invokes normalize to normalize the content of files X and Y. It then considers each chunk of X in turn and invokes simple match to find a match in file Y. The invocation passes in—as part of the procedure arguments—the pointer to the chunk (const char *ps) as well as the pointer to the content of Y (const char *ts). If a match is found, the procedure returns 1, otherwise, it returns 0. (If a chunk of X appears many times in Y, the return value should still be 1.)
Testing: Runthegiventesterprogram$./rktest.py0

Rabin-Karp Approximate Matching

Our next optimization comes from using the brilliant Rabin-Karp substring matching algorithm (RK for short), invented in the eighties by two famous computer scientists, Michael Rabin and Richard Karp 3.
RK checks if a given query string P appears as a substring in Y. At a high level, RK works by computing a hash for the query string, hash(P ), as well as a hash for each of the n − k + 1 substrings of length k in Y, hash(Y [0...k − 1]), hash(Y [1...k]), ..., hash(Y [n − k...n − 1]). A hash function turns any arbitary string into a b-bit hash value with the property that collision (two different strings with the same hash value) is unlikely. Therefore, by comparing hash(P ) with each of the n − k + 1 hashes from Y, we can check if P appears as a substring in Y. There are many nice hash functions out there (such as MD5, SHA-1), but RK’s magical ingredient is its “rolling” hash function. Specifically, given hash(Y [i...i + k − 1]), it takes only constant time instead of O(k) time to compute hash(Y [i + 1...i + k]).
Now we explain how the rolling hash is computed. Let’s treat each character as a digit in radix-d notation. We choose radix d = 256 since each character in the C language is represented by a single byte and we can conveniently use the byte value of the character as its digit. For example, the string ’ab’ corresponds to two digits with one being 97 (the ASCII value of ’a’), and the other being 98 (the ASCII value of ’b’). The decimal value of ’ab’ in radix-256 can be calculated as 256 ∗ 97 + 98 = 24930. The hash of a string P in RKishash(P[0...k−1])=256k−1 ∗P[0]+256k−2 ∗P[1]+...+256∗P[k−2]+P[k−1].Nowlet’s see how to do a rolling calculation of the values for every substring of Y. Let y0 = hash(Y [0...k − 1] and yi = hash(Y [i...i + k − 1]). We can compute yi+1 from yi in constant time, by observing that
yi+1 =256∗(yi −256k−1 ∗Y[i])+Y[i+k]
Note that we have to remember the value of 256k−1 in a variable instead of re-computing it for yi each time.
Now we’ve seen how rolling hash works. The only fly in the ointment is that these radix-256 hash values are too huge to work with efficiently and conveniently. Therefore, we perform all the computation in modulo q, where q is chosen to be a large4 prime5. Hash collisions are infrequent, but still possible. Therefore once we detect some yi = hash(P ), we should compare the actual strings Y [i...i + k − 1] and P [0...k − 1] to see if they are indeed identical.
Since RK speeds up substring matching to O(n) on average instead of O(n ∗ k) as in the simple algorithm. However, we still need to run RK ⌊ m ⌋ times for each of the ⌊ m ⌋ chunks of X to be matched in Y. Thus, our
approximate matching algorithm using RK has an overall runtime of O( m ∗ n). k
Your job: Implement the RK substring matching algorithm by completing the rabin karp match pro- cedure. When calculating the hash values, you should use the given modulo arithmatic functions, madd, mdel, mmul.

As with simple match, our main procedure will invoke rabin karp match for each chunk of X to be matched. rabin karp match has the same interface as simple match and should return 1 if the chunk appears as a substring in Y or 0 if otherwise.
Testing: Runthegiventesterprogram$./rktest.py1
