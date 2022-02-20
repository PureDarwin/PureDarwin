#!/usr/bin/python2.7

import string
import os
import json
import sys
import commands
import subprocess


class KernelCollection:

    def __init__(self):
        self.print_json=False

    def __init__(self, print_json):
        self.print_json = print_json

    def buildKernelCollection(self, arch_flag, kernel_cache_path, kernel_path, extensions_dir, bundle_ids=[], options=[]):
        try:
            test_root = os.path.dirname(__file__)
            build_root = os.path.realpath(os.path.dirname(__file__) + "/..")
            app_cache_util = build_root + "/../build/Release/dyld_app_cache_util"
            args = [app_cache_util,
                    "-create-kernel-collection", test_root + kernel_cache_path,
                    "-kernel", test_root + kernel_path,
                    "-arch", arch_flag]
            if extensions_dir is not None:
                args.append("-extensions")
                args.append(test_root + extensions_dir)
            for bundle_id in bundle_ids:
                args.append("-bundle-id")
                args.append(bundle_id)
            file_text = subprocess.check_output(["file", build_root + kernel_cache_path]);
            for opt in options:
                args.append(opt.replace("$PWD", test_root))

            runline = ""
            for arg in args:
                if not arg:
                    runline = runline + '"' + arg + '"' + ' '
                else:
                    runline = runline + arg + ' '

            self.dict = {}
            if self.print_json:
                print "Run with: " + runline
            process = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            self.json_text, self.error_message = process.communicate()
            if self.print_json:
                print self.json_text
                print self.error_message
            if process.returncode:
                if not self.print_json:
                    print self.error_message
                print "Non-zero return code"
                print "Run with: " + runline
                sys.exit(0)
            #print self.json_text
            #print self.error_message
            if self.json_text:
                self.dict = json.loads(self.json_text)
                self.error_message = ""
        except subprocess.CalledProcessError as e:
            #print "can't make closure for " + kernel_cache_path
            self.error_message = e.output
            self.dict = {}
        except:
            assert False
            self.dict = {}

    def buildPageableKernelCollection(self, arch_flag, aux_kernel_cache_path, kernel_cache_path, extensions_dir, bundle_ids=[], options=[]):
        try:
            test_root = os.path.dirname(__file__)
            build_root = os.path.realpath(os.path.dirname(__file__) + "/..")
            app_cache_util = build_root + "/../build/Release/dyld_app_cache_util"
            args = [app_cache_util,
                    "-create-pageable-kernel-collection", test_root + aux_kernel_cache_path,
                    "-kernel-collection", test_root + kernel_cache_path,
                    "-arch", arch_flag]
            if extensions_dir is not None:
                args.append("-extensions")
                args.append(test_root + extensions_dir)
            for bundle_id in bundle_ids:
                args.append("-bundle-id")
                args.append(bundle_id)
            file_text = subprocess.check_output(["file", build_root + kernel_cache_path]);
            for opt in options:
                args.append(opt)
            self.dict = {}
            if self.print_json:
                print "Run with: " + ' '.join(args)
            process = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            self.json_text, self.error_message = process.communicate()
            if self.print_json:
                print self.json_text
                print self.error_message
            if process.returncode:
                if not self.print_json:
                    print self.error_message
                print "Non-zero return code"
                print "Run with: " + ' '.join(args)
                sys.exit(0)
            #print self.json_text
            #print self.error_message
            if self.json_text:
                self.dict = json.loads(self.json_text)
                self.error_message = ""
        except subprocess.CalledProcessError as e:
            #print "can't make closure for " + kernel_cache_path
            self.error_message = e.output
            self.dict = {}
        except:
            assert False
            self.dict = {}

    def buildAuxKernelCollection(self, arch_flag, aux_kernel_cache_path, kernel_cache_path, pageable_cache_path, extensions_dir, bundle_ids=[], options=[]):
        try:
            test_root = os.path.dirname(__file__)
            build_root = os.path.realpath(os.path.dirname(__file__) + "/..")
            app_cache_util = build_root + "/../build/Release/dyld_app_cache_util"
            args = [app_cache_util,
                    "-create-aux-kernel-collection", test_root + aux_kernel_cache_path,
                    "-kernel-collection", test_root + kernel_cache_path,
                    "-arch", arch_flag]
            if pageable_cache_path:
                args.append("-pageable-collection")
                args.append(test_root + pageable_cache_path)
            if extensions_dir is not None:
                args.append("-extensions")
                args.append(test_root + extensions_dir)
            for bundle_id in bundle_ids:
                args.append("-bundle-id")
                args.append(bundle_id)
            file_text = subprocess.check_output(["file", build_root + kernel_cache_path]);
            for opt in options:
                args.append(opt)
            self.dict = {}
            if self.print_json:
                print "Run with: " + ' '.join(args)
            process = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            self.json_text, self.error_message = process.communicate()
            if self.print_json:
                print self.json_text
                print self.error_message
            if process.returncode:
                if not self.print_json:
                    print self.error_message
                print "Non-zero return code"
                print "Run with: " + ' '.join(args)
                sys.exit(0)
            #print self.json_text
            #print self.error_message
            if self.json_text:
                self.dict = json.loads(self.json_text)
                self.error_message = ""
        except subprocess.CalledProcessError as e:
            #print "can't make closure for " + kernel_cache_path
            self.error_message = e.output
            self.dict = {}
        except:
            assert False
            self.dict = {}

    def analyze(self, app_cache_path, options=[]):
        try:
            test_root = os.path.dirname(__file__)
            build_root = os.path.realpath(os.path.dirname(__file__) + "/..")
            app_cache_util = build_root + "/../build/Release/dyld_app_cache_util"
            args = [app_cache_util, "-app-cache", test_root + app_cache_path, "-platform", "kernel"]
            file_text = subprocess.check_output(["file", build_root + app_cache_path]);
            for opt in options:
                args.append(opt)
            self.dict = {}
            if self.print_json:
                print "Run with: " + ' '.join(args)
            process = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            self.json_text, self.error_message = process.communicate()
            if self.print_json:
                print self.json_text
                print self.error_message
            if process.returncode:
                if not self.print_json:
                    print self.error_message
                print "Non-zero return code"
                print "Run with: " + ' '.join(args)
                sys.exit(0)
            #print self.json_text
            #print self.error_message
            if self.json_text:
                self.dict = json.loads(self.json_text)
                self.error_message = ""
        except subprocess.CalledProcessError as e:
            #print "can't make closure for " + app_cache_path
            self.error_message = e.output
            self.dict = {}
        except:
            assert False
            self.dict = {}

    def dictionary(self):
        return self.dict

    def error(self):
        return self.error_message

