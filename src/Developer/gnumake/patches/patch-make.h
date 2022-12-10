--- make.h.orig	Fri Oct 29 17:02:08 2004
+++ make.h	Fri Oct 29 17:02:32 2004
@@ -508,6 +508,21 @@
 extern int warn_undefined_variables_flag, posix_pedantic, not_parallel;
 extern int second_expansion, clock_skew_detected, rebuilding_makefiles;
 
+#if defined(__APPLE__) || defined(NeXT) || defined(NeXT_PDO)
+extern unsigned int next_flag;
+#define NEXT_VPATH_FLAG			(1 << 0)
+#define NEXT_QUIET_FLAG			(1 << 1)
+#define NEXT_MAKEFILES_FLAG		(1 << 2)
+#define NEXT_ERREXIT_FLAG		(1 << 3)
+#define NEXT_ALL_FLAGS			(NEXT_VPATH_FLAG | 	\
+					 NEXT_QUIET_FLAG | 	\
+					 NEXT_MAKEFILES_FLAG |	\
+					 NEXT_ERREXIT_FLAG)
+
+extern int general_vpath_search();
+extern char *allocated_vpath_expand_for_file();
+#endif	/* __APPLE__ || NeXT || NeXT_PDO */
+
 /* can we run commands via 'sh -c xxx' or must we use batch files? */
 extern int batch_mode_shell;
 
