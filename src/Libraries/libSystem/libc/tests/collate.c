#include <TargetConditionals.h>
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <stdlib.h>
#include <xlocale.h>

#include <darwintest.h>

void __collate_lookup_l(const __darwin_wchar_t *, int *, int *, int *,
                        locale_t);
void __collate_lookup(const unsigned char *, int *, int *, int *);

#define CHARS_WITHOUT_ENTRIES "\xdf"

/*
 * in C or POSIX locales
 *  __collate_lookup("", ... )  -> len: 0 prim: 0 sec: 0
 *  __collate_lookup("a", ... )   -> len: 1 prim: (int)'a' sec: 0
 *  __collate_lookup("ab", ... )  -> len: 1 prim: (int)'a' sec: 0
 *
 * in a Latin-1 locale (de_DE.ISO8859-1)
 *  __collate_lookup("", ... )  -> len: 0 prim: 0 sec: 0
 *  __collate_lookup("a", ... )   -> len: 1 prim: > 0 sec: > 0
 *  __collate_lookup("ab", ... )  -> len: 1 prim: > 0 sec: > 0
 *  # a character not in the table - lookup failure
 *  __collate_lookup("\xdf", ... )  -> len: 0 prim: -1 sec: -1
 *
 * in a UTF-8 locale (de_DE.UTF-8)
 *  __collate_lookup("", ... )  -> len: 0 prim: 0 sec: 0
 *  __collate_lookup("a", ... )   -> len: 1 prim: > 0 sec: > 0
 *  __collate_lookup("ab", ... )   -> len: 1 prim: > 0 sec: > 0
 *  # An invalid multi-byte sequence
 *  __collate_lookup("\xe4", ... )   -> len: 1 prim: (int)'\xe4' sec: 0
 *  # valid multi-byte sequence
 *  __collate_lookup("\xc3\xa4", ... )   -> len: 2 prim: > 0 sec: > 0
 */
T_DECL(collate_lookup, "Test __collate_lookup() behavior") {
  unsigned char c;
  unsigned char str[16];
  int len, prim, sec, prim2, sec2;
  char *result;

  /* ------------------------- C Locale ------------------------- */
  /* In the C locale primary weights should equal the int value of the
   * character*/
  result = setlocale(LC_ALL, "C");
  T_ASSERT_NOTNULL(result, "changed to C locale");

  __collate_lookup("", &len, &prim, &sec);
  T_ASSERT_EQ_INT(len, 0, "No characters read");
  T_EXPECT_EQ_INT(prim, 0, "No primary weight");
  T_EXPECT_EQ_INT(sec, 0, "No secondary weight");

  str[1] = 'X';
  str[2] = '\0';
  for (c = 1; c < UCHAR_MAX; c++) {
    len = 1;
    str[0] = c;
    __collate_lookup(str, &len, &prim, &sec);
    T_ASSERT_EQ_INT(len, 1, "Only read one character");
    T_EXPECT_EQ_INT(prim, (int)c, "Primary weight returned is the value of c");
    T_EXPECT_EQ_INT(sec, 0, "Secondary weight returned is 0");
  }

#if TARGET_OS_OSX
  /* ------------------------- German Latin-1 Locale ----------------------- */
  result = setlocale(LC_ALL, "de_DE.ISO8859-1");
  T_ASSERT_NOTNULL(result, "changed to german Latin-1 locale");

  __collate_lookup("", &len, &prim, &sec);
  T_ASSERT_EQ_INT(len, 0, "No characters read");
  T_EXPECT_EQ_INT(prim, 0, "No primary weight");
  T_EXPECT_EQ_INT(sec, 0, "No secondary weight");

  str[1] = 'X';
  str[2] = '\0';
  for (c = 1; c < UCHAR_MAX; c++) {
    len = 1;
    str[0] = c;
    __collate_lookup(str, &len, &prim, &sec);
    T_ASSERT_EQ_INT(len, (c == '\0' ? 0 : 1), "Only read one character");
    str[1] = '\0';
    if (strstr(CHARS_WITHOUT_ENTRIES, str)) {
      T_EXPECT_EQ(prim, -1, "0x%x is not present in the table", c);
      T_EXPECT_EQ(sec, -1, "0x%x is not present in the table", c);
    } else {
      T_EXPECT_GT(prim, 0, "0x%x Has primary weight", c);
      T_EXPECT_GT(sec, 0, "0x%x Has secondary weight", c);
    }
  }

  str[0] = 'a';
  __collate_lookup(str, &len, &prim, &sec);
  T_ASSERT_EQ_INT(len, 1, "Only read one character");

  /* a with dieresis in Latin-1 locales */
  str[0] = (unsigned char)'\xe4';
  __collate_lookup(str, &len, &prim2, &sec2);
  T_ASSERT_EQ_INT(len, 1, "Only read one character");
  T_EXPECT_EQ(prim, prim2, "Same primary weight");
  T_EXPECT_LT(sec, sec2, "Different secondary weight");

  /* ------------------------- German UTF-8 Locale ------------------------- */
  result = setlocale(LC_ALL, "de_DE.UTF-8");
  T_ASSERT_NOTNULL(result, "changed to german UTF-8 locale");

  __collate_lookup("", &len, &prim, &sec);
  T_ASSERT_EQ_INT(len, 0, "No characters read");
  T_EXPECT_EQ_INT(prim, 0, "No primary weight");
  T_EXPECT_EQ_INT(sec, 0, "No secondary weight");

  str[1] = 'X';
  str[2] = '\0';
  for (c = 1; c < UCHAR_MAX; c++) {
    len = 2; /* Tell it that this string is longer */
    str[0] = c;
    __collate_lookup(str, &len, &prim, &sec);
    T_ASSERT_EQ_INT(len, 1, "Only read one character");
    if (strstr(CHARS_WITHOUT_ENTRIES, (const char *)str)) {
      T_EXPECT_EQ(prim, -1, "0x%x is not present in the table", c);
      T_EXPECT_GT(sec, -1, "0x%x is not present in the table", c);
    } else {
      T_EXPECT_GT(prim, 0, "0x%x Has primary weight", c);
      /* weight will be 0 for sequences that result in mb failure */
      if (c < 128) {
        /* So only test secondary weights for the ASCII characters */
        T_EXPECT_GT(sec, 0, "0x%x Has secondary weight", c);
      }
    }
  }

  str[0] = 'a';
  __collate_lookup(str, &len, &prim, &sec);
  T_ASSERT_EQ_INT(len, 1, "Only read one character");

  /* a with dieresis in Latin-1 locales */
  /* this character is invalid in a UTF-8 locale */
  str[0] = (unsigned char)'\xe4';
  errno = 0;
  __collate_lookup(str, &len, &prim2, &sec2);
  T_EXPECT_EQ_INT(errno, EILSEQ, "errno indicates invalid character");
  T_ASSERT_EQ_INT(len, 1, "Only read one character");
  T_EXPECT_EQ(prim2, (unsigned int)L'\xe4',
              "Invalid character - Primary weight equal to value (228)");
  T_EXPECT_EQ(sec2, 0, "Invalid character - No secondary weight");

  T_EXPECT_NE(prim, prim2, "Different primary weight");
  T_EXPECT_NE(sec, sec2, "Different secondary weight");

  /* Test Multibyte lookup */
  str[0] = (unsigned char)'\xc3';
  str[1] = (unsigned char)'\xa4';
  str[2] = (unsigned char)'X';
  str[3] = (unsigned char)'\0';
  len = 3;
  __collate_lookup(str, &len, &prim2, &sec2);
  T_EXPECTFAIL_WITH_REASON(
      "__collate_lookup doesn't actually tell you how many bytes were used");
  T_ASSERT_EQ_INT(len, 2, "Only read 2 characters");
  T_EXPECT_EQ(prim, prim2, "Same primary weight");
  T_EXPECT_LT(sec, sec2, "Different secondary weight");
#endif
}

/*
 * Tests for the __collate_lookup_l() which is used to lookup weights of wide
 * characters
 */
T_DECL(collate_lookup_l, "Test __collate_lookup_l() behavior") {
  wchar_t wc;
  wchar_t wcs[16];
  char str[16] = {0};
  int len, prim, sec, prim2, sec2;
  char *result;

  /* ------------------------- C Locale ------------------------- */
  /* In the C locale primary weights should equal the int value of the
   * character*/
  result = setlocale(LC_ALL, "C");
  T_ASSERT_NOTNULL(result, "changed to C locale");

  __collate_lookup_l(L"", &len, &prim, &sec, LC_GLOBAL_LOCALE);
  T_ASSERT_EQ_INT(len, 0, "No characters read");
  T_EXPECT_EQ_INT(prim, 0, "No primary weight");
  T_EXPECT_EQ_INT(sec, 0, "No secondary weight");

  wcs[1] = L'X';
  wcs[2] = L'\0';
  for (wc = 1; wc < UCHAR_MAX; wc++) {
    len = 1;
    wcs[0] = wc;
    errno = 0;
    __collate_lookup_l(wcs, &len, &prim, &sec, LC_GLOBAL_LOCALE);
    T_ASSERT_EQ_INT(errno, 0, "No error occurred");
    T_ASSERT_EQ_INT(len, 1, "Only read one character");
    T_EXPECT_EQ_INT(prim, (int)wc,
                    "Primary weight returned is the value of wc");
    T_EXPECT_EQ_INT(sec, 0, "Secondary weight returned is 0");
  }

#if TARGET_OS_OSX
  /* ------------------------- German Latin-1 Locale -------------------------
   */
  result = setlocale(LC_ALL, "de_DE.ISO8859-1");
  T_ASSERT_NOTNULL(result, "changed to german Latin-1 locale");

  wcs[1] = L'X';
  wcs[2] = L'\0';
  for (wc = 1; wc < UCHAR_MAX; wc++) {
    len = 1;
    wcs[0] = wc;
    str[0] = wc & 0xFF;
    __collate_lookup_l(wcs, &len, &prim, &sec, LC_GLOBAL_LOCALE);
    T_ASSERT_EQ_INT(len, 1, "Only read one character");
    if (strstr(CHARS_WITHOUT_ENTRIES, str)) {
      T_EXPECT_EQ(prim, -1, "0x%x is not present in the table", wc);
      T_EXPECT_EQ(sec, -1, "0x%x is not present in the table", wc);
    } else {
      T_EXPECT_GT(prim, 0, "Wide char 0x%x Has primary weight", wc);
      T_EXPECT_GT(sec, 0, "Wide char 0x%x Has secondary weight", wc);
    }
  }

  wcs[0] = L'a';
  __collate_lookup_l(wcs, &len, &prim, &sec, LC_GLOBAL_LOCALE);
  T_ASSERT_EQ_INT(len, 1, "Only read one character");

  /* a with dieresis in Latin-1 locales */
  wcs[0] = L'\xe4';
  __collate_lookup_l(wcs, &len, &prim2, &sec2, LC_GLOBAL_LOCALE);
  T_ASSERT_EQ_INT(len, 1, "Only read one character");
  T_EXPECT_EQ(prim, prim2, "Same primary weight");
  T_EXPECT_LT(sec, sec2, "Different secondary weight");

  /* ------------------------- German UTF-8 Locale ------------------------- */
  result = setlocale(LC_ALL, "de_DE.UTF-8");
  T_ASSERT_NOTNULL(result, "changed to german UTF-8 locale");

  __collate_lookup_l(L"", &len, &prim, &sec, LC_GLOBAL_LOCALE);
  T_ASSERT_EQ_INT(len, 0, "No characters read");
  T_EXPECT_EQ_INT(prim, 0, "No primary weight");
  T_EXPECT_EQ_INT(sec, 0, "No secondary weight");

  for (wc = 1; wc < UCHAR_MAX; wc++) {
    len = 1;
    wcs[0] = wc;
    str[0] = wc & 0xFF;
    __collate_lookup_l(wcs, &len, &prim, &sec, LC_GLOBAL_LOCALE);
    T_ASSERT_EQ_INT(len, 1, "Only read one character");
    if (strstr(CHARS_WITHOUT_ENTRIES, str)) {
      T_EXPECT_EQ(prim, -1, "0x%x is not present in the table", wc);
      T_EXPECT_EQ(sec, -1, "0x%x is not present in the table", wc);
    } else {
      T_EXPECT_GT(prim, 0, "Wide char 0x%x Has primary weight", wc);
      T_EXPECT_GT(sec, 0, "Wide char 0x%x Has secondary weight", wc);
    }
  }

  /* Test that a lookup of 'a' and '\xe4' returns the same primary weight */
  wcs[0] = L'a';
  wcs[1] = L'\0';
  __collate_lookup_l(wcs, &len, &prim, &sec, LC_GLOBAL_LOCALE);
  T_ASSERT_EQ_INT(len, 1, "Only read one character");
  T_EXPECT_GT(prim, 0, "Wide char 0x%x Has primary weight", wc);
  T_EXPECT_GT(sec, 0, "Wide char 0x%x Has secondary weight", wc);

  wcs[0] = L'\xe4';
  wcs[1] = L'\0';
  errno = 0;
  __collate_lookup_l(wcs, &len, &prim2, &sec2, LC_GLOBAL_LOCALE);
  T_EXPECT_EQ_INT(errno, 0, "errno was not set");
  T_ASSERT_EQ_INT(len, 1, "Only read one character");
  T_EXPECT_GT(prim2, 0, "Wide char 0x%x Has primary weight", wc);
  T_EXPECT_GT(sec2, 0, "Wide char 0x%x Has secondary weight", wc);

  T_EXPECT_EQ(prim, prim2, "Primary weight equal");
  T_EXPECT_NE(sec, sec2, "Different secondary weight");
#endif
}
