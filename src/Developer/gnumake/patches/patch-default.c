--- default.c.orig	2006-02-11 14:16:04.000000000 -0800
+++ default.c	2011-12-05 16:41:05.000000000 -0800
@@ -45,7 +45,7 @@
 .mod .sym .def .h .info .dvi .tex .texinfo .texi .txinfo \
 .w .ch .web .sh .elc .el .obj .exe .dll .lib";
 #else
-  = ".out .a .ln .o .c .cc .C .cpp .p .f .F .r .y .l .s .S \
+  = ".out .a .ln .o .c .cc .C .cpp .p .f .F .m .r .y .l .ym .lm .s .S \
 .mod .sym .def .h .info .dvi .tex .texinfo .texi .txinfo \
 .w .ch .web .sh .elc .el";
 #endif
@@ -192,6 +192,8 @@
     "$(LINK.cpp) $^ $(LOADLIBES) $(LDLIBS) -o $@",
     ".f",
     "$(LINK.f) $^ $(LOADLIBES) $(LDLIBS) -o $@",
+    ".m",
+    "$(LINK.m) $^ $(LOADLIBES) $(LDLIBS) -o $@",
     ".p",
     "$(LINK.p) $^ $(LOADLIBES) $(LDLIBS) -o $@",
     ".F",
@@ -221,6 +223,8 @@
     "$(COMPILE.cpp) $(OUTPUT_OPTION) $<",
     ".f.o",
     "$(COMPILE.f) $(OUTPUT_OPTION) $<",
+    ".m.o",
+    "$(COMPILE.m) $(OUTPUT_OPTION) $<",
     ".p.o",
     "$(COMPILE.p) $(OUTPUT_OPTION) $<",
     ".F.o",
@@ -250,6 +254,11 @@
     ".l.c",
     "@$(RM) $@ \n $(LEX.l) $< > $@",
 
+    ".ym.m",
+    "$(YACC.m) $< \n mv -f y.tab.c $@",
+    ".lm.m",
+    "@$(RM) $@ \n $(LEX.m) $< > $@",
+
     ".F.f",
     "$(PREPROCESS.F) $(OUTPUT_OPTION) $<",
     ".r.f",
@@ -304,6 +313,9 @@
 
 static char *default_variables[] =
   {
+#if defined(__APPLE__) || defined(NeXT) || defined(NeXT_PDO)
+    "GNUMAKE", "YES",	/* I'm not sure who uses this.  Dave Payne 8/10/99 */
+#endif /* __APPLE__ || NeXT || NeXT_PDO */
 #ifdef VMS
 #ifdef __ALPHA
     "ARCH", "ALPHA",
@@ -400,7 +412,7 @@
 # endif /* __MSDOS__ */
 #else
     "CC", "cc",
-    "CXX", "g++",
+    "CXX", "c++",
 #endif
 
     /* This expands to $(CO) $(COFLAGS) $< $@ if $@ does not exist,
@@ -468,6 +480,8 @@
     "LINK.o", "$(CC) $(LDFLAGS) $(TARGET_ARCH)",
     "COMPILE.c", "$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c",
     "LINK.c", "$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) $(TARGET_ARCH)",
+    "COMPILE.m", "$(COMPILE.c)",
+    "LINK.m", "$(LINK.c)",
     "COMPILE.cc", "$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c",
     "COMPILE.C", "$(COMPILE.cc)",
     "COMPILE.cpp", "$(COMPILE.cc)",
@@ -476,6 +490,8 @@
     "LINK.cpp", "$(LINK.cc)",
     "YACC.y", "$(YACC) $(YFLAGS)",
     "LEX.l", "$(LEX) $(LFLAGS) -t",
+    "YACC.m", "$(YACC) $(YFLAGS)",
+    "LEX.m", "$(LEX) $(LFLAGS) -t",
     "COMPILE.f", "$(FC) $(FFLAGS) $(TARGET_ARCH) -c",
     "LINK.f", "$(FC) $(FFLAGS) $(LDFLAGS) $(TARGET_ARCH)",
     "COMPILE.F", "$(FC) $(FFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c",
