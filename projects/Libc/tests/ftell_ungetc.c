/* Thanks to the originator of Feedback FB8165838 / Radar 66131999 for
 * writing the original version of this test!
 */

#include <darwintest.h>

#include <assert.h>
#include <string.h>

#include <stdio.h>

T_DECL(ftell_ungetc, "Test interactions of ftell and ungetc")
{
	FILE *fp = stdin;
	fp = fopen("assets/ftell_ungetc.txt", "rb");
	T_QUIET; T_ASSERT_NE(fp, (FILE*)NULL, "Open the file");

	/* Test ftell without having done any reads/writes */
	T_ASSERT_EQ(ftell(fp), 0L, "ftell without having done any reads/writes");

	/* Read one character */
	T_ASSERT_EQ(fgetc(fp), '/', "Read one charatcter");

	/* Check ftell again after one read */
	T_ASSERT_EQ(ftell(fp), 1L, "ftell after one read");

	/* Push back one character */
	T_ASSERT_EQ(ungetc('/', fp), '/', "push back one character");

	/* Check ftell again after pushing back one char */
	T_ASSERT_EQ(ftell(fp), 0L, "ftell after pushing back one char");

	/* Read it again */
	T_ASSERT_EQ(fgetc(fp), '/', "read pushed back character again");
	T_ASSERT_EQ(ftell(fp), 1L, "ftell after reading again");

	/* Seek and test ftell again */
	T_ASSERT_EQ(fseek(fp, 2, SEEK_SET), 0, "seek");
	T_ASSERT_EQ(ftell(fp), 2L, "ftell after seeking");

	/* Push back invalid char (EOF) */
	T_ASSERT_EQ(ungetc(EOF, fp), EOF, "push back invalid char");
	T_ASSERT_EQ(ftell(fp), 2L, "ftell after pushing invalid char, pos should not have changed");

	/* Read, push back different char, read again
	 * and check ftell.
	 *
	 * Cppreference:
	 * A successful call to ungetc on a text stream modifies
	 * the stream position indicator in unspecified manner but
	 * guarantees that after all pushed-back characters are
	 * retrieved with a read operation, the stream position
	 * indicator is equal to its value before ungetc.
	 */
	T_ASSERT_EQ(fgetc(fp), '-', "read another character");
	T_ASSERT_EQ(ftell(fp), 3L, "ftell after read");
	T_ASSERT_EQ(ungetc('A', fp), 'A', "push back a different character");
	T_ASSERT_EQ(fgetc(fp), 'A', "read back the different character");
	T_ASSERT_EQ(ftell(fp), 3L, "ftell after pushback and read back");

	/* Push back a non-read character and test ftell.
	 *
	 * According to POSIX:
	 * The file-position indicator is decremented by each
	 * successful call to ungetc();
	 *
	 * Cppreference:
	 * A successful call to ungetc on a binary stream decrements
	 * the stream position indicator by one
	 */
	T_ASSERT_EQ(fgetc(fp), '+', "read another character");
	T_ASSERT_EQ(ungetc('A', fp), 'A', "push back a different character");
	T_EXPECTFAIL; T_ASSERT_EQ(ftell(fp), 3L, "ftell after pushback - EXPECTED FAIL rdar://66131999");
}
