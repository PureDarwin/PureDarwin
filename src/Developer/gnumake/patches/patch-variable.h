--- variable.h.orig	Fri Oct 29 16:28:58 2004
+++ variable.h	Fri Oct 29 16:29:35 2004
@@ -110,6 +110,13 @@
 
 /* expand.c */
 extern char *variable_buffer_output PARAMS ((char *ptr, char *string, unsigned int length));
+
+#if defined(__APPLE__) || defined(NeXT) || defined(NeXT_PDO)
+extern char *initialize_variable_output PARAMS ((void));
+extern char *save_variable_output PARAMS ((int *savelenp));
+extern void restore_variable_output PARAMS ((char *save, int savelen));
+#endif /* __APPLE__ || NeXT || NeXT_PDO */
+
 extern char *variable_expand PARAMS ((char *line));
 extern char *variable_expand_for_file PARAMS ((char *line, struct file *file));
 extern char *allocated_variable_expand_for_file PARAMS ((char *line, struct file *file));
