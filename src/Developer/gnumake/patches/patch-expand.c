--- expand.c.orig	Fri Oct 29 17:27:40 2004
+++ expand.c	Fri Oct 29 17:32:45 2004
@@ -76,7 +76,11 @@
 
 /* Return a pointer to the beginning of the variable buffer.  */
 
+#if defined(__APPLE__) || defined(NeXT) || defined(NeXT_PDO)
+char *
+#else
 static char *
+#endif
 initialize_variable_output (void)
 {
   /* If we don't have a variable output buffer yet, get one.  */
@@ -90,6 +94,35 @@
 
   return variable_buffer;
 }
+
+#if defined(__APPLE__) || defined(NeXT) || defined(NeXT_PDO)
+char *
+save_variable_output(savelenp)
+    int *savelenp;
+{
+  char *save;
+
+  save = variable_buffer;
+  *savelenp = variable_buffer_length;
+  
+  variable_buffer = 0;
+  variable_buffer_length = 0;
+
+  return (save);
+}
+
+void
+restore_variable_output (save, savelen)
+    char *save;
+    int savelen;
+{
+  if (variable_buffer != 0)
+    free (variable_buffer);
+
+  variable_buffer = save;
+  variable_buffer_length = savelen;
+}
+#endif /* __APPLE__ || NeXT || NeXT_PDO */
 
 /* Recursively expand V.  The returned string is malloc'd.  */
 
