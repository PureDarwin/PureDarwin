--- /dev/null	Fri Oct 29 17:01:51 2004
+++ next.c	Fri Oct 29 17:01:41 2004
@@ -0,0 +1,226 @@
+#if defined(__APPLE__) || defined(NeXT) || defined(NeXT_PDO) /* for NEXT_VPATH_FLAG support */
+#include "make.h"
+#include "job.h"
+#include "commands.h"
+#include "filedef.h"
+#include "variable.h"
+#include "dep.h"
+#include <sys/types.h>
+#include <sys/stat.h>
+
+#if !__STDC__
+#define const
+#endif
+
+/* Search through a pathlist for a file.  `search_path' will parse `path',
+ * a list of pathnames separated by colons, prepending each pathname to
+ * `filename'.  The resulting filename will be checked for existence via
+ * stat().
+ */
+static int
+search_path(path, file)
+    const char *path;
+    char **file;
+{
+    int n, length;
+    char *filename, *name;
+    const char *nextchar, *lastchar;
+
+    filename = *file;
+    n = strlen(filename);
+    length = strlen(path) + n + 10;
+    name = alloca(length) + length - 1;
+    *name = '\0';
+
+    filename += n;
+    while (--n >= 0)
+	*--name = *--filename;
+
+    if (*name == '/' || path == 0)
+	path = "";
+
+    /* Strip off leading './'s, if any. */
+    while (*name == '.' && *(name + 1) == '/')
+	name += 2;
+
+    do {
+	/* Advance to the end of the next path in our path list. */
+	nextchar = path;
+#if defined (__MSDOS__) || defined (WIN32)
+	while ((*nextchar != '\0' && *nextchar != ':' && !isspace (*nextchar))
+	|| (*nextchar == ':' && nextchar - path == 1
+	  && (nextchar[1] == '/' || nextchar[1] == '\\')))
+	    nextchar++;
+#else
+	while (*nextchar != '\0' && *nextchar != ':' && !isspace (*nextchar))
+	    nextchar++;
+#endif
+
+	lastchar = nextchar;
+	filename = name;
+
+	/* If we actually have a path, prepend the file name with a '/'. */
+	if (nextchar != path)
+	    *--filename = '/';
+
+	/* Prepend the file name with the path. */
+	while (nextchar != path)
+	    *--filename = *--nextchar;
+
+	path = (*lastchar) ? lastchar + 1 : lastchar;
+
+	{
+	    struct stat s;
+	    if (stat(filename, &s) >= 0) {
+		/* We have found a file.
+		 * Store the name we found into *FILE for the caller.  */
+		*file = savestring(filename, strlen(filename));
+		return (1);
+	    }
+	}
+    } while (*path != 0);
+    return (0);
+}
+
+int
+general_vpath_search(file)
+    char **file;
+{
+    int s;
+    int savelen;
+    char *vpath, *save;
+    
+    save = save_variable_output(&savelen);
+    vpath = variable_expand ("$(VPATH)");
+    if (**file == '/' || *vpath == '\0') {
+	restore_variable_output(save, savelen);
+	return 0;
+    }
+    s = search_path(vpath, file);
+    restore_variable_output(save, savelen);
+    return s;
+}
+
+static int
+match_dep(filename, file)
+    char **filename;
+    struct file *file;
+{
+    struct dep *d;
+
+    /* don't substitute for . or .. */
+    if (!strcmp (*filename, ".") || !strcmp (*filename, ".."))
+        return 0;
+    
+    for (d = file->deps; d != 0; d = d->next) {
+	if (d->file->old_name != 0) {
+	    if (strcmp(*filename, d->file->old_name) == 0) {
+		*filename = dep_name(d);
+		return 1;
+	    }
+	}
+	if (strcmp(*filename, dep_name(d)) == 0) {
+	    if (general_vpath_search(filename))
+		return 1;
+	}
+    }
+    return 0;
+}
+
+/* Scan LINE for vpath references. */
+
+static char *
+vpath_expand(line, file)
+    char *line;
+    struct file *file;
+{
+    struct variable *v;
+    char *p, *p1, *o;
+    static char *meta = 0;
+
+    if (meta == 0) {
+	static char buffer[256] = {0};
+	meta = buffer;
+	meta['\0'] = 1;
+	for (p = "=|^();&<>*?[]:$`'\"\\\n"; *p != 0; p++)
+	    meta[*p] = 1;
+    }
+
+    p = line;
+    o = initialize_variable_output ();
+
+    while (1) {
+	/* Copy all following uninteresting chars all at once to the
+	   variable output buffer, and skip them.  Uninteresting chars end
+	   at the next space or semicolon. */
+
+	for (p1 = p; *p1 != 0 && (isspace(*p1) || meta[*p1]); p1++)
+	    ;
+	o = variable_buffer_output (o, p, p1 - p);
+	if (*p1 == 0)
+	    break;
+	p = p1;
+	while (*p1 != 0 && !(isspace(*p1) || meta[*p1]))
+	    p1++;
+	{
+	    unsigned int n = p1 - p;
+	    char *buffer = malloc(n + 1);
+	    char *filename = buffer;
+	    struct dep *dep;
+
+	    strncpy(filename, p, n);
+	    filename[n] = 0;
+
+	    if (match_dep(&filename, file)) {
+		static struct file *last_file = 0;
+		if (last_file != file) {
+		    last_file = file;
+		    if (!(next_flag & NEXT_QUIET_FLAG)) {
+		      error(&file->cmds->fileinfo,
+			    "Using old-style VPATH substitution.");
+		      error(&file->cmds->fileinfo,
+			    "Consider using automatic variable substitution instead.");
+		    }
+		}
+		o = variable_buffer_output (o, filename, strlen(filename));
+	    } else {
+		o = variable_buffer_output (o, filename, n);
+	    }
+	    p = p1;
+	    free(buffer);
+	}
+	if (*p == '\0')
+	    break;
+    }
+    (void) variable_buffer_output (o, "", 1);
+    return initialize_variable_output ();
+}
+
+char *
+allocated_vpath_expand_for_file(line, file)
+    char *line;
+    struct file *file;
+{
+    char *save, *value;
+    struct variable_set_list *save_set_list;
+    int savelen;
+
+    if (file == 0)
+	fatal(NILF, "Can't do VPATH expansion on a null file.\n");
+    
+    save = save_variable_output (&savelen);
+    
+    save_set_list = current_variable_set_list;
+    current_variable_set_list = file->variables;
+    reading_file = &file->cmds->fileinfo;
+    value = vpath_expand (line, file);
+    current_variable_set_list = save_set_list;
+    reading_file = 0;
+
+    value = savestring (value, strlen (value));
+    
+    restore_variable_output (save, savelen);
+    
+    return value;
+}
+#endif	/* __APPLE__ || NeXT || NeXT_PDO */
