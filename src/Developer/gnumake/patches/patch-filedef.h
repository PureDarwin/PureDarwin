--- filedef.h.orig	Fri Oct 29 17:25:25 2004
+++ filedef.h	Fri Oct 29 17:25:59 2004
@@ -42,6 +42,10 @@
 				   entries for the same file.  */
     struct file *last;          /* Last entry for the same file name.  */
 
+#if defined(__APPLE__) || defined(NeXT) || defined(NeXT_PDO)
+    char *old_name;
+#endif
+
     /* File that this file was renamed to.  After any time that a
        file could be renamed, call `check_renamed' (below).  */
     struct file *renamed;
