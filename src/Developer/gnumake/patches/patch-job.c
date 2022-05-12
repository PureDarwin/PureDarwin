--- job.c.orig	2006-03-19 19:03:04.000000000 -0800
+++ job.c	2006-11-30 17:47:36.000000000 -0800
@@ -1107,8 +1107,16 @@
 #else
       (argv[0] && !strcmp (argv[0], "/bin/sh"))
 #endif
+#if defined(__APPLE__) || defined(NeXT) || defined(NeXT_PDO)
+      /* allow either -ec or -c */
+      && ((argv[1]
+	   && argv[1][0] == '-' && argv[1][1] == 'c' && argv[1][2] == '\0') ||
+          (argv[1]
+	   && argv[1][0] == '-' && argv[1][1] == 'e' && argv[1][2] == 'c' && argv[1][3] == '\0'))
+#else
       && (argv[1]
           && argv[1][0] == '-' && argv[1][1] == 'c' && argv[1][2] == '\0')
+#endif __APPLE__ || NeXT || NeXT_PDO
       && (argv[2] && argv[2][0] == ':' && argv[2][1] == '\0')
       && argv[3] == NULL)
     {
@@ -1601,6 +1609,19 @@
 						     file);
     }
 
+#if defined(__APPLE__) || defined(NeXT) || defined(NeXT_PDO) /* for NEXT_VPATH_FLAG support */
+  if (next_flag & NEXT_VPATH_FLAG) {
+      for (i = 0; i < cmds->ncommand_lines; ++i) {
+	  char *line;
+	  if (lines[i] != 0) {
+	      line = allocated_vpath_expand_for_file (lines[i], file);
+	      free (lines[i]);
+	      lines[i] = line;
+	  }
+      }
+  }
+#endif	/* __APPLE__ || NeXT || NeXT_PDO */
+
   /* Start the command sequence, record it in a new
      `struct child', and add that to the chain.  */
 
@@ -2690,15 +2711,33 @@
        argument list.  */
 
     unsigned int shell_len = strlen (shell);
+#if defined(__APPLE__) || defined(NeXT) || defined(NeXT_PDO)
+    char *minus_c;
+    int minus_c_len;
+
+    if (next_flag & NEXT_ERREXIT_FLAG) {
+      minus_c = " -ec ";
+      minus_c_len = 5;
+    } else {
+      minus_c = " -c ";
+      minus_c_len = 4;
+    }
+#else
 #ifndef VMS
     static char minus_c[] = " -c ";
 #else
     static char minus_c[] = "";
 #endif
+#endif /* __APPLE__ || NeXT || NeXT_PDO */
     unsigned int line_len = strlen (line);
 
+#if defined(__APPLE__) || defined(NeXT) || defined(NeXT_PDO)
+    char *new_line = (char *) alloca (shell_len + minus_c_len
+				      + (line_len * 2) + 1);
+#else
     char *new_line = (char *) alloca (shell_len + (sizeof (minus_c) - 1)
 				      + (line_len * 2) + 1);
+#endif /* __APPLE__ || NeXT || NeXT_PDO */
     char *command_ptr = NULL; /* used for batch_mode_shell mode */
 
 # ifdef __EMX__ /* is this necessary? */
@@ -2709,8 +2748,13 @@
     ap = new_line;
     bcopy (shell, ap, shell_len);
     ap += shell_len;
+#ifdef __APPLE__
+    bcopy (minus_c, ap, minus_c_len);
+    ap += minus_c_len;
+#else /* !__APPLE__ */
     bcopy (minus_c, ap, sizeof (minus_c) - 1);
     ap += sizeof (minus_c) - 1;
+#endif /* __APPLE__ */
     command_ptr = ap;
     for (p = line; *p != '\0'; ++p)
       {
@@ -2761,7 +2805,7 @@
 #endif
 	*ap++ = *p;
       }
-    if (ap == new_line + shell_len + sizeof (minus_c) - 1)
+    if (ap == new_line + shell_len + minus_c_len)
       /* Line was empty.  */
       return 0;
     *ap = '\0';
