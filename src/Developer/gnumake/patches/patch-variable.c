--- variable.c.orig	2005-09-25 22:16:31.000000000 -0700
+++ variable.c	2006-04-10 12:23:51.000000000 -0700
@@ -144,6 +144,41 @@
 
 /* Implement variables.  */
 
+#if defined(__APPLE__) || defined(NeXT) || defined(NeXT_PDO)
+static void check_apple_pb_support (name, length, value)
+     const char *name;
+     unsigned int length;
+     char *value;
+{
+  char *p;
+
+  if (length == 20 && !strncmp (name, "USE_APPLE_PB_SUPPORT", length)) {
+    //if (makelevel == 0) error(NILF, "USE_APPLE_PB_SUPPORT is deprecated");
+    for (p = value; *p != '\0'; p++) {
+      if (isspace (*p)) {
+	continue;
+      }
+      if (!strncmp (p, "all", 3)) {
+	p += 3;
+	next_flag |= NEXT_ALL_FLAGS;
+      } else if (!strncmp (p, "vpath", 5)) {
+	p += 5;
+	next_flag |= NEXT_VPATH_FLAG;
+      } else if (!strncmp (p, "quiet", 5)) {
+	p += 5;
+	next_flag |= NEXT_QUIET_FLAG;
+      } else if (!strncmp (p, "makefiles", 9)) {
+	p += 9;
+	next_flag |= NEXT_MAKEFILES_FLAG;
+      } else if (!strncmp (p, "errexit", 7)) {
+	p += 7;
+	next_flag |= NEXT_ERREXIT_FLAG;
+      }
+    }
+  }
+}
+#endif /* __APPLE__ || NeXT || NeXT_PDO */
+
 void
 init_hash_global_variable_set (void)
 {
@@ -168,6 +203,10 @@
   struct variable **var_slot;
   struct variable var_key;
 
+#if defined(__APPLE__) || defined(NeXT) || defined(NeXT_PDO)
+  check_apple_pb_support (name, length, value);
+#endif /* __APPLE__ || NeXT || NeXT_PDO */
+
   if (set == NULL)
     set = &global_variable_set;
 
