//
//  guess_short_name_test.c
//  libstuff_test
//
//  Created by Michael Trent on 1/19/19.
//

#include "test_main.h"

#include <string.h>
#include <sys/param.h>

#include "stuff/bool.h"
#include "stuff/guess_short_name.h"

static int check_name(const char* install_name, const char* desc, const char* a,
                     const char* b)
{
  int res = 0;
  
  if (a && b && 0 != strcmp(a, b)) {
    test_printerr("%s %s should be %s: %s", install_name, desc, a, b);
    res = 1;
  } else if (a && !b) {
    test_printerr("%s %s should be %s: %s", install_name, desc, a, "NULL");
    res = 1;
  } else if (!a && b) {
    test_printerr("%s %s should be %s: %s", install_name, desc, "NULL", b);
    res = 1;
  }

  return res;
}

static int check_ebool(const char* install_name, const char* desc, enum bool a,
                       enum bool b)
{
  int res = 0;

  if (a != b) {
    test_printerr("%s %s should be %s: %s", install_name, desc,
                  a == TRUE ? "TRUE" : "FALSE",
                  b == TRUE ? "TRUE" : "FALSE");
    res = 1;
  }
  
  return res;
}

static int check(const char* install_name, const char* short_name,
                               const char* suffix, enum bool isFramework)
{
  int res = 0;
  
  char iname[MAXPATHLEN];
  
  strncpy(iname, install_name, MAXPATHLEN);
  
  enum bool isFwk;
  char* suf;
  char* shname = guess_short_name(iname, &isFwk, &suf);
  
  res |= check_name(install_name, "short name", short_name, shname);
  res |= check_name(install_name, "suffix", suffix, suf);
  res |= check_ebool(install_name, "is framework", isFramework, isFwk);

  if (shname)
    free(shname);
  if (suf)
    free(suf);
  
  return res;
}

static void test_guess_short_name(void)
{
  // library, no suffix, no version
  check("/usr/lib/libFoo.dylib", "libFoo", NULL, FALSE);
  check("libFoo.dylib", "libFoo", NULL, FALSE);

  // library, no suffix, version
  check("/usr/lib/libFoo.A.dylib", "libFoo", NULL, FALSE);
  check("libFoo.A.dylib", "libFoo", NULL, FALSE);
  
  // library, suffix, no version
  check("/usr/lib/libFoo_profile.dylib", "libFoo", "_profile", FALSE);
  check("/usr/lib/libFoo_debug.dylib", "libFoo", "_debug", FALSE);
  check("libFoo_profile.dylib", "libFoo", "_profile", FALSE);
  check("libFoo_debug.dylib", "libFoo", "_debug", FALSE);
  
  // library, suffix, version
  check("/usr/lib/libFoo_profile.A.dylib", "libFoo", "_profile", FALSE);
  check("/usr/lib/libFoo_debug.A.dylib", "libFoo", "_debug", FALSE);
  check("/usr/lib/libATS.A_profile.dylib", "libATS", "_profile", FALSE);
  check("libFoo_profile.A.dylib", "libFoo", "_profile", FALSE);
  check("libFoo_debug.A.dylib", "libFoo", "_debug", FALSE);
  check("libATS.A_profile.dylib", "libATS", "_profile", FALSE);
  
  // framework, no suffix, no version
  check("Frameworks/Foo.framework/Foo", "Foo", NULL, TRUE);
  check("Foo.framework/Foo", "Foo", NULL, TRUE);
  
  // framework, no suffix, versions
  check("Frameworks/Foo.framework/Versions/A/Foo", "Foo", NULL, TRUE);
  check("Foo.framework/Versions/A/Foo", "Foo", NULL, TRUE);
  
  // framework, suffix, no version
  check("Frameworks/Foo.framework/Foo_profile", "Foo", "_profile", TRUE);
  check("Foo.framework/Foo_profile", "Foo", "_profile", TRUE);
  check("Frameworks/Foo.framework/Foo_debug", "Foo", "_debug", TRUE);
  check("Foo.framework/Foo_debug", "Foo", "_debug", TRUE);
  
  // framework, suffix, version
  check("Frameworks/Foo.framework/Versions/A/Foo_profile",
        "Foo", "_profile", TRUE);
  check("Foo.framework/Versions/A/Foo_profile", "Foo", "_profile", TRUE);
  check("Frameworks/Versions/A/Foo.framework/Foo_debug",
        "Foo", "_debug", TRUE);
  check("Foo.framework/Versions/A/Foo_debug", "Foo", "_debug", TRUE);
  
  // qtx, no suffix, no version
  check("/usr/lib/libFoo.qtx", "libFoo", NULL, FALSE);
  check("libFoo.qtx", "libFoo", NULL, FALSE);
  
  // qtx, no suffix, version
  check("/usr/lib/libFoo.A.qtx", "libFoo", NULL, FALSE);
  check("libFoo.A.qtx", "libFoo", NULL, FALSE);
  
  // qtx files do not support suffixes
  
  // non-standard suffixes
//  check("/usr/lib/libFoo_mdt.dylib", "libFoo", "_mdt", FALSE);
//  check("libFoo_network.dylib", "libFoo", "_network", FALSE);
//  check("/usr/lib/libFoo_network.A.dylib", "libFoo", "_network", FALSE);
//  check("libFoo_network.A.dylib", "libFoo", "_network", FALSE);
//  check("Frameworks/Foo.framework/Foo_network", "Foo", "_network", TRUE);
//  check("Foo.framework/Foo_network", "Foo", "_network", TRUE);
//  check("Frameworks/Versions/A/Foo.framework/Foo_network",
//        "Foo", "_network", TRUE);
//  check("Foo.framework/Versions/A/Foo_network", "Foo", "_network", TRUE);
  check("/usr/lib/libFoo_mdt.dylib", "libFoo_mdt", NULL, FALSE);
  check("libFoo_network.dylib", "libFoo_network", NULL, FALSE);
  check("/usr/lib/libFoo_network.A.dylib", "libFoo_network", NULL, FALSE);
  check("libFoo_network.A.dylib", "libFoo_network", NULL, FALSE);
  check("Frameworks/Foo_network.framework/Foo_network",
        "Foo_network", NULL, TRUE);
  check("Foo_network.framework/Foo_network", "Foo_network", NULL, TRUE);
  check("Frameworks/Versions/A/Foo_network.framework/Foo_network",
        "Foo_network", NULL, TRUE);
  check("Foo_network.framework/Versions/A/Foo_network",
        "Foo_network", NULL, TRUE);

  // questionable actors
  //   configd_libSystem
  check("/lib/system/libsystem_configuration.dylib",
        "libsystem_configuration", NULL, FALSE);
  check("/lib/system/libsystem_configuration_debug.dylib",
        "libsystem_configuration", "_debug", FALSE);
  check("/lib/system/libsystem_configuration_profile.dylib",
        "libsystem_configuration", "_profile", FALSE);
  //   Sandbox_libs
  check("/usr/lib/system/libsystem_sandbox.dylib",
        "libsystem_sandbox", NULL, FALSE);
  check("/usr/lib/system/libsystem_sandbox_debug.dylib",
        "libsystem_sandbox", "_debug", FALSE);
  check("/usr/lib/system/libsystem_sandbox_profile.dylib",
        "libsystem_sandbox", "_profile", FALSE);

  // bad actors
  //   nanocom
  check("/usr/local/lib/nanocom/libnanocom_bell.dylib",
        "libnanocom_bell", NULL, FALSE);
  check("/usr/local/lib/nanocom/libnanocom_fastsim.dylib",
        "libnanocom_fastsim", NULL, FALSE);
  check("/usr/local/lib/nanocom/libnanocom_login.dylib",
        "libnanocom_login", NULL, FALSE);

  //   mDNSResponder
  check("/usr/lib/libdns_services.dylib",
        "libdns_services", NULL, FALSE);

  //   ngttp2
  check("/usr/lib/libapple_nghttp2.dylib",
        "libapple_nghttp2", NULL, FALSE);
}

static int test_main(void)
{
  return test_add("test guess_short_name", test_guess_short_name);
}
