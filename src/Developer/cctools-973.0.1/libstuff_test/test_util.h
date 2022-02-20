//
//  test_util.h
//  libstuff_test
//
//  Created by Michael Trent on 5/31/19.
//

#ifndef test_util_h
#define test_util_h

#include <unistd.h>

// test_write_tmp_data
// write data of size to a file in /tmp. The path to the file will be returned
// in *name, which will need to be deallocated via free(). Returns 0 on success
// or -1 on error.
int test_write_tmp_data(const void* data, size_t size, char** name);

#endif /* test_util_h */
