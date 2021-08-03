The PureDarwin Package Database
===============================

The PureDarwin Package Database (pdpd) is a SQLite database assembled from text files
by the `pdpdmake' tool, and read on the Darwin system by the `dm' installer tools.

The file "pdpd" is an example database assembled in this directory with the command:

	tools/pdpdmake -n pdpd -v 1 -p PackageLists/package_list.txt -p PackageLists/found.txt
	-p PackageLists/missing.txt -p PackageLists/extra_packages.txt -c Groups/core.txt 
	-g Groups/Editors.txt -s Systems/Example_System.txt -l Descriptions/desc.en.txt 

svn directories
---------------

Under trunk/projects/PureDarwinPackageDatabase are these directories:

  tools/         -- Holds pdpdmake, dbaudit and dbstats, described below
  PackageLists/  -- Holds text files containing information about the packages which
                    will go in pdpd and be available for install via it
  Description/   -- Holds text files which provide descriptions for each package
	Groups/  -- Holds text files which list the packages which are arranged into groups
	Systems/ -- Holds text files which describe the systems which are made up of lists
                    of groups and packages

PackageLists
------------

These package lists have been created so far (but note that their names are only use
so that we known what are in them -- pdpdmake doesn't care how the files are named):

  package_list.txt   -- All binary packages available from src.macosforge.org
  missing.txt        -- All packages for which no binary is currently available (but...)
  found.txt          -- All packages for which we have provided a binary
  extra_packages.txt -- Packages which are not part of darwinbuild (eg. CFNetwork)

These files list one package per line, e.g.:

From package_list.txt line #66:

  DiskArbitration Y 9G55pd1 183 { CF IOKitUser Security libgcc Libsystem configd }

From missing.txt line #18:

  MacPorts N 9G55pd1

Each line has the form:

  Package_name Available? BuildVersion ProjectVersion { Dependencies.. }

  Package_name    -- e.g., "DiskArbitration", "MacPorts"
  Available?      -- "DiskArbitration" is, "MacPorts" isn't
  BuildVersion    -- Which darwinbuild version this information is from
  ProjectVersion  -- Only if the package is available
  Dependencies    -- A list of packages between "{" and "}"

Unresolved dependencies, eg.:

From package_list.txt line #53:

  apr Y 9G55pd1 12 { CoreOSMakefiles Libsystem libgcc SQLite /usr/lib/libexpat.1.dylib libiconv }

If the package a dependency belongs to could not be found, the full path of the 
missing library is shown instead. (This is a bad example because I added a binary 
for the "expat" project and have updated all the package lists.)

* Someone needs to go through these lists and see which missing packages can be built
  in darwinbuild and added to the svn, and then update these lists. Please.

Description Files
-----------------

Description files are named "anything.xx.txt", where "xx" is a lower-case language code
like "en". This tells pdpdmake what language a description is for. The files contain one line per entry, which consists of the package, group or system name, a space, and the
description, which can be as long as you like but must not contain linefeeds, eg.:

From desc.en.txt line #5:

  securityd System securityd daemon, which handles authentication tasks.

Each line has the form:

  {Package_name|group|system name} Description

System and Group Files
----------------------

These are named "system_name.txt" and "group_name.txt". System files are lists of group
and package names, one per line. Group files should only contain package names. You can
leave empty lines, and use "#" for comments.

! Systems and groups should not be given the same names as packages, or the database
  will get confused and bad things will happen.

eg. if we wanted to create an editors group, we would make "Editors.txt" which would
contain these lines:

  vim
  pico
  nano
  emacs

Or "Scripting_Languages.txt" (please, no spaces in names):

  perl
  ruby
  python
  tcl

Then you could create a system which added just these, plus the Chess and top packages
to the core installation, with "Scripting_and_Editors.txt":

  Editors               # a group
  Scripting_Languages   # another group
  Chess                 # a package
  top                   # another package

The user would then be able to install this via `dm --add Scripting_and_Editors'.

pdpdpmake
=========

pdpdmake assemble the files described about into a package database (pdpd), either by creating
a new one, or by updating an existing one.

Usage
-----

Invoke pdpdmake without any arguments for help.
(We really need a little shell script which will call this with our files.)

  pdpdmake { -n | -u } [ -v version ] file [ { -c | -p | -l | -g | -s } file ]\n";
	
  -n file  Create a new database, or
  -u file  Update an existing database

           One of these options must be provided.

  -v version  Set the package database's internal version number. If -u is used and
              this is absent, the internal version will just be increased by 1.

  -c file  Set core packages. See "core packages" below.

  -p file  Enter a package list file into the database.

  -l file  Enter a localised language file into the database.

  -g file  Enter a package group file into the database.

  -s file  Enter a system group file into the database.

Any number of -c, -p, -l, -g and -s options can be provided. The contents of each file
will be added in turn to the database.

Core Packages
-------------

Core packages are the packages which form "PureDarwin Core". Since these are installed
at the same time as (or effectively before) the package manager, they are treated in a
special way:

 * They are always installed
 * They cannot be un-installed

Core package lists should have one package name per line.

--

OK, now tell me what I forgot to add.

--

README for dbaudit and dbstats.

dbaudit
=======

dbaudit performs a brute-force analysis of all of the packages available to a 
darwinbuild installation, checking for the availability of a binary root by
calling `darwinbuild -load <package_name>` and checking dependencies by running
`otool -L` on binary executables and examining the "#!" prefix of script files.

dbaudit should be invoked from the darwinbuild directory (eg. /Volumes/dbufs/9G55/) 
because it needs to invoke the darwinbuild and darwinxref tools.

Usage	
-----

	dbaudit [ -p package_name ] [ -f package_list_file ] [ -o output_dir ]

	-p package_name         Tells dbaudit to examine a particular package.
	                        This should be the package name without version
				number suffix (eg. "curl", not "curl-42").

	                        Multiple -p options can be specified at once, e.g.,: 
                                dbaudit -p xnu -p perl -p curl

                                The -p flag may be used along with the -f flag.

	-f package_list_file	Tells dbaudit to read a list of packages to examine for a
                                file. The file should contain a list of the package names
                                without their version strings, one per line. Multiple -f
                                options can be specified at once.

                                The -f flag may be used along with the -p flag.

	-o output_dir		Specifies the file to write the package list out to. The file will
                                be truncated before writing.

                                If output_dir is a directory (ie. it ends in "/"), a file named
                                "dbaudit.txt" will be created in it.

                                If not -o option is specified, output will instead be written to
                                stdout.

If neither -p nor -f flags are specified, dbaudit will get a list of all packages
available (using `darwinxref version '*'`) and process these. On my 9G55 install this
took approximately 2 hours to complete.

Output Format
-------------

Output is a simple text file, one line per package, in the format:

	PackageName ROOT? Build Version { Dependency1 Dependency2 ... DependencyN }

Where:

	* PackageName is the package name without version suffix.
	* ROOT? is "Y" or "N", depending on whether a binary root was successfully found.
	* Build was the darwinbuild build version (eg. "9G55", "9G55pd1"), and should be the
	  same for all packages in the file
	* Version is the package's version, or ??? if this information wasn't available.
	* DependencyX is either:
                * A resolved package dependency, in which case it is a package name
		*  An unresolved dependency, in which case it is the full path to a library
		If the space between braces in blank in means no dependency information could be
                determined.

dbstats
=======

dbstats prints basic information about a package list file created by dbaudit.

Usage
-----

	dbstats -f package_list [-m] [-u]
	
		-f package_list      The package list, as produced by dbaudit, to analyse

		-m                   Print a list of packages without binary roots.

		-u                   Print a list of all unresolved dependencies. These will be absolute paths
                                     to library files not provided by any other project.

Without either -m or -u option dbstats simply prints a count of missing packages and
unresolved dependencies.
 
