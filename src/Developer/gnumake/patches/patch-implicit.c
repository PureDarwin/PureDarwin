--- implicit.c.orig	Fri Oct 29 17:24:20 2004
+++ implicit.c	Fri Oct 29 17:24:55 2004
@@ -786,8 +786,24 @@
 
   /* RULE is nil if the loop went all the way
      through the list and everything failed.  */
+#if defined(__APPLE__) || defined(NeXT) || defined(NeXT_PDO) /* for NEXT_VPATH_FLAG support */
+  if (rule == 0) {
+    if ((next_flag & NEXT_VPATH_FLAG) && file->old_name != 0) {
+      char *save_name = file->name;
+      file->name = file->old_name;
+      file->old_name = 0;
+      /* bad hack: rule should be non-null so the function returns a non-zero result */
+      if (pattern_search(file, archive, depth, recursions))
+        rule = (struct rule *)1;
+      file->old_name = file->name;
+      file->name = save_name;
+    }
+    goto done;
+  }
+#else
   if (rule == 0)
     goto done;
+#endif	/* __APPLE__ || NeXT || NeXT_PDO */
 
   foundrule = i;
 
