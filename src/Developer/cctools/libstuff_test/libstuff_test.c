//
//  main.cpp
//  libstuff_test
//
//  Created by Michael Trent on 1/19/19.
//

#include "test.h"

#include "stuff/errors.h"

/* used by error routines as the name of this program */
char *progname = NULL;

int main(int argc, const char * argv[])
{
  progname = (char*)argv[0];
  
  // run the tests
  return test_run();
}

