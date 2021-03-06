#! /usr/bin/perl -w
#
# Generates the highly repetitive Visual C++ project file test/Makefile.am for 
# libpqxx when new tests have been added.
# 
# This script does not add carriage returns at the end of each line, the way
# MS-DOS likes it.  A simple "sed -e 's/$/\r/'" should do the trick.
#

my $dir = '';
if (@ARGV > 0) {
  $dir = $ARGV[0]
}
if ($dir eq '') {
  $dir = "../test"
}
my @tests;
opendir(DIRHANDLE, "$dir") or die "Could not open $dir: $!";
while (defined($filename=readdir(DIRHANDLE))) {
  if (substr($filename,-4) eq '.cxx') {
    push(@tests, substr($filename, 0, length($filename)-4));
  }
}
closedir(DIRHANDLE);

# Last of currently available tests
my $last = @tests[@tests-1];
$last =~ s/[^0-9]//g;

print <<EOF;
# AUTOMATICALLY GENERATED--DO NOT EDIT
# This file is generated automatically for automake whenever test programs are
# added to libpqxx, using the Perl script "maketestvcmak.pl" found in the tools
# directory.
#
# Contents of this file based on the original test.mak by Clinton James (March
# 2002) with changes including those by Bart Samwel (February 2006) 

!IF \"\$(CFG)\" != \"Release\" && \"\$(CFG)\" != \"Debug\"
!MESSAGE You can specify a specific testcase when running NMAKE. For example:
!MESSAGE     NMAKE /f \"test.mak\" testcase
!MESSAGE Possible choices for testcase are TEST000 through TEST$last or ALL
!MESSAGE
CFG=Release
!ENDIF

!IF \"\$(CFG)\" == \"Debug\"
!MESSAGE NOTE:
!MESSAGE The Debug configuration only works if you've built libpqxx
!MESSAGE with the debug target, AND you've built libpq with that as
!MESSAGE well. You can do this by doing:
!MESSAGE     CD postgresql-x.y.z\\src\\interfaces\\libpq
!MESSAGE     NMAKE /F win32.mak DEBUG=1
!ENDIF

!include common
OUTDIR=.\\lib
INTDIR=.\\obj

!IF  \"\$(CFG)\" == \"Release\"
CPP_EXTRA=/MD /D \"NDEBUG\"
LINK32_FLAG_LIB=libpqxx.lib
LINK32_FLAG_EXTRA=/incremental:no

!ELSEIF  \"\$(CFG)\" == \"Debug\"
CPP_EXTRA=/MDd /Gm /GZ /Zi /Od /D \"_DEBUG\"
LINK32_FLAG_LIB=libpqxxD.lib
LINK32_FLAG_EXTRA=/incremental:no /debug

!ENDIF

CPP=cl.exe
CPP_PROJ=/nologo \$(CPP_EXTRA) /W3 /GX /FD /GR /wd4800 \\
	/c \\
	/D \"WIN32\" /D \"_CONSOLE\" /D \"_MBCS\" /D \"HAVE_VSNPRINTF_DECL\" \$(STD) \\
	/I \"../include\" /I \"\$(PGSQLSRC)/include\" /I \"\$(PGSQLSRC)/interfaces/libpq\" \\
	/YX /Fo\"\$(INTDIR)\\\\\" /Fd\"\$(INTDIR)\\\\\"

LINK32=link.exe
LINK32_FLAGS=\$(LINK32_FLAG_LIB) kernel32.lib user32.lib wsock32.lib \\
        /nologo /subsystem:console /machine:I386 \\
        \$(LINK32_FLAG_EXTRA) \$(LIBPATH) /libpath:\"lib\"

EOF

print "ALL :";
foreach my $t (@tests) {
  my $ut = uc $t;
  print " $ut"
}
print "\n\n";

foreach my $t (@tests) {
  my $ut = uc $t;
  print "$ut".': "$(OUTDIR)\\'."$t".'.exe"'."\n"
}
print "\n";

print <<EOF;

CLEAN :
	-\@del \"\$(INTDIR)\" /Q
	-\@del vc70.pch /Q
	-\@del \"\$(OUTDIR)\\test*.exe\" /Q

\"\$(INTDIR)\" :
        if not exist \"\$(INTDIR)/\$(NULL)\" mkdir \"\$(INTDIR)\"

\"\$(OUTDIR)\" :
    if not exist \"\$(OUTDIR)/\$(NULL)\" mkdir \"\$(OUTDIR)\"

EOF

foreach my $t (@tests) {
  print <<EOF
\"\$(OUTDIR)\\$t.exe\" : \"\$(OUTDIR)\" \$(DEF_FILE) \"\$(INTDIR)\\$t.obj\"
    \@\$(LINK32) \@<<
  \$(LINK32_FLAGS) /out:\"\$(OUTDIR)\\$t.exe\" \"\$(INTDIR)\\$t.obj\"
<<
	-\@del \"\$(INTDIR)\" /Q
	\@\"\$(OUTDIR)\\$t.exe\"

EOF
}

print <<EOF;

.c{\$(INTDIR)}.obj::
   \$(CPP) \@<<
   \$(CPP_PROJ) \$<
<<

.cpp{\$(INTDIR)}.obj::
   \$(CPP) \@<<
   \$(CPP_PROJ) \$<
<<

.cxx{\$(INTDIR)}.obj::
   \$(CPP) \@<<
   \$(CPP_PROJ) \$<
<<

!IF \"\$(CFG)\" == \"Release\" || \"\$(CFG)\" == \"Debug\"

EOF


foreach my $t (@tests) {
  print <<EOF
SOURCE=..\\test\\$t.cxx
\"\$(INTDIR)\\$t.obj\" : \$(SOURCE) \"\$(INTDIR)\"
        \@\$(CPP) \$(CPP_PROJ) \$(SOURCE)


EOF
}

print <<EOF;

!ENDIF

EOF

