--- read.c.orig	Fri Oct 29 16:57:10 2004
+++ read.c	Fri Oct 29 16:59:17 2004
@@ -1972,6 +1972,9 @@
 	    fatal (flocp,
                    _("target file `%s' has both : and :: entries"), f->name);
 
+#if defined(__APPLE__) || defined(NeXT) || defined(NeXT_PDO)
+          if (!(next_flag & NEXT_QUIET_FLAG)) {
+#endif
 	  /* If CMDS == F->CMDS, this target was listed in this rule
 	     more than once.  Just give a warning since this is harmless.  */
 	  if (cmds != 0 && cmds == f->cmds)
@@ -1991,6 +1994,9 @@
                      _("warning: ignoring old commands for target `%s'"),
                      f->name);
 	    }
+#if defined(__APPLE__) || defined(NeXT) || defined(NeXT_PDO)
+	      }
+#endif
 
 	  f->is_target = 1;
 
