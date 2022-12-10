--- remake.c.orig	2006-03-19 18:36:37.000000000 -0800
+++ remake.c	2006-11-30 17:49:56.000000000 -0800
@@ -223,6 +223,9 @@
 		     or not at all.  G->changed will have been set above if
 		     any commands were actually started for this goal.  */
 		  && file->update_status == 0 && !g->changed
+#if defined(__APPLE__) || defined(NeXT) || defined(NeXT_PDO)
+		  && !(next_flag & NEXT_QUIET_FLAG)
+#endif
 		  /* Never give a message under -s or -q.  */
 		  && !silent_flag && !question_flag)
 		message (1, ((file->phony || file->cmds == 0)
@@ -481,6 +484,9 @@
 
       if (is_updating (d->file))
 	{
+#if __APPLE__ || NeXT || NeXT_PDO
+         if (!(next_flag & NEXT_QUIET_FLAG))
+#endif
 	  error (NILF, _("Circular %s <- %s dependency dropped."),
 		 file->name, d->file->name);
 	  /* We cannot free D here because our the caller will still have
@@ -740,6 +746,9 @@
 
       while (file)
         {
+#if __APPLE__ || NeXT || NeXT_PDO
+          file->old_name = file->name;
+#endif	/* __APPLE__ || NeXT || NeXT_PDO */
           file->name = file->hname;
           file = file->prev;
         }
@@ -993,6 +1002,9 @@
 
 	      if (is_updating (d->file))
 		{
+#if __APPLE__ || NeXT || NeXT_PDO
+		  if (!(next_flag & NEXT_QUIET_FLAG))
+#endif
 		  error (NILF, _("Circular %s <- %s dependency dropped."),
 			 file->name, d->file->name);
 		  if (lastd == 0)
@@ -1106,12 +1118,23 @@
 	   Pretend it was successfully remade.  */
 	file->update_status = 0;
       else
+#if defined(__APPLE__) || defined(NeXT) || defined(NeXT_PDO)
+      {
+	char *name = file->name;
+	if ((next_flag & NEXT_VPATH_FLAG) && general_vpath_search(&name)) {
+	  free(name);
+	  file->update_status = 0;
+	} else
+#endif /* defined(__APPLE__) || defined(NeXT) || defined(NeXT_PDO) */
         {
           /* This is a dependency file we cannot remake.  Fail.  */
           if (!rebuilding_makefiles || !file->dontcare)
             complain (file);
           file->update_status = 2;
         }
+#if defined(__APPLE__) || defined(NeXT) || defined(NeXT_PDO)
+      }
+#endif
     }
   else
     {
