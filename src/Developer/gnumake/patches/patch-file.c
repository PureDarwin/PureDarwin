--- file.c.orig	Fri Oct 29 17:26:18 2004
+++ file.c	Fri Oct 29 17:26:56 2004
@@ -207,6 +207,9 @@
   rehash_file (from_file, to_hname);
   while (from_file)
     {
+#if defined(__APPLE__) || defined(NeXT) || defined(NeXT_PDO)
+      from_file->old_name = from_file->name;
+#endif  /* __APPLE__ || NeXT || NeXT_PDO */
       from_file->name = from_file->hname;
       from_file = from_file->prev;
     }
