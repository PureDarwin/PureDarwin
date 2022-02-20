#!/usr/bin/python2.7

import string
import os
import json
import sys
import imp
import os.path
import traceback

sys.dont_write_bytecode = True

import KernelCollection


if __name__ == "__main__":
    test_dir = os.path.realpath(os.path.dirname(__file__))
    sys.path.append(test_dir)
    all_tests = os.listdir(test_dir)
    all_tests.sort()
    test_to_run = ""
    if len(sys.argv) == 2:
        test_to_run = sys.argv[1]
        all_tests = [ test_to_run ]
    for f in all_tests:
        test_case = test_dir + "/" + f + "/test.py"
        if os.path.isfile(test_case):
            py_mod = imp.load_source(f, test_case)
            check_func = getattr(py_mod, "check", 0)
            if check_func == 0:
                print "FAIL: " + f + ", missing check() function";
            else:
                try:
                    kernelCollection = KernelCollection.KernelCollection(test_to_run != "")
                    check_func(kernelCollection)
                    print "PASS: " + f
                except AssertionError, e:
                    _, _, tb = sys.exc_info()
                    tb_info = traceback.extract_tb(tb)
                    filename, line, func, text = tb_info[-1]
                    print "FAIL: " + f + ", " + text
                except KeyError, e:
                    _, _, tb = sys.exc_info()
                    tb_info = traceback.extract_tb(tb)
                    filename, line, func, text = tb_info[-1]
                    print "FAIL: " + f + ", " + text

