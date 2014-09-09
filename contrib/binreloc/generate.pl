#!/usr/bin/env perl
use strict;
use FindBin qw($RealBin);
use File::Spec;

srand();

if ((!$ARGV[0]) || ($ARGV[0] eq "--help") || $ARGV[0] eq "-h") {
	print "Usage: ./generate.pl <TYPE> [SRC] [HEADER]\n";
	print "This script generates a version of BinReloc with a coding style/API that fits\n";
	print "your project's coding style and prefferred API. The result will be written to\n";
	print "the files SRC ('binreloc.c' by default) and HEADER ('binreloc.h' by default).\n\n";
	print "TYPE is one of the following:\n";
	print "  normal       Use normal (raw C) API, with no further dependancies.\n";
	print "  glib         Use glib-style API.\n\n";
	exit;
}

my $basic = getFile("basic.c");

my $br_license  = getSection($basic, 'LICENSE');
my $br_include  = getSection($basic, 'INCLUDE');
my $br_errors   = getSection($basic, 'ERROR');
my $br_function = getSection($basic, 'FUNCTION');
my $br_c_guard1 = "#ifndef __BINRELOC_C__\n" .
	          "#define __BINRELOC_C__\n";
my $br_c_guard2 = "#endif /* __BINRELOC_C__ */\n";
my $br_h_guard1 = $br_c_guard1;
my $br_h_guard2 = $br_c_guard2;

$br_h_guard1 =~ s/_C_/_H_/mg;
$br_h_guard2 =~ s/_C_/_H_/;

my $cppguard1 = "#ifdef __cplusplus\n" .
	        "extern \"C\" {\n" .
	        "#endif /* __cplusplus */\n";
my $cppguard2 = "#ifdef __cplusplus\n" .
	        "}\n" .
	        "#endif /* __cplusplus */\n";

my $mangle = '';
for (my $i = 0; $i < 4; $i++) {
	my @chars = qw(a b c d e f g h i j k l m n o p q r s t u v w x y z
		       A B C D E F G H I J K L M N O P Q R S T U V W X Y Z);
	$mangle .= $chars[rand(scalar @chars)];
}
$mangle .= int(rand(99999999999999));


my $srcfile = ($ARGV[1] || "binreloc.c");
my $headerfile = ($ARGV[2] || "binreloc.h");
my (undef, undef, $headerincludefile) = File::Spec->splitpath($headerfile);

my $src = '';
my $header = '';


if ($ARGV[0] eq "normal") {
	my $normal = getFile("normal.c");

	$src = "$br_license\n$br_c_guard1\n";
	$src .= $br_include;
	$src .= "#include \"$headerincludefile\"\n\n";
	$src .= "$cppguard1\n";
	$src .= "\n\n$br_function\n\n";
	$src .= "$normal\n\n";
	$src .= "$cppguard2\n";
	$src .= $br_c_guard2;

	$header = "$br_license\n$br_h_guard1\n$cppguard1\n\n";
	$header .= "$br_errors\n\n";
	$header .= "#ifndef BINRELOC_RUNNING_DOXYGEN\n";
	$header .= mangle(qw/br_init br_init_lib br_find_exe br_find_exe_dir br_find_prefix br_find_bin_dir
			     br_find_sbin_dir br_find_data_dir br_find_locale_dir
			     br_find_lib_dir br_find_libexec_dir br_find_etc_dir
			     br_strcat br_build_path br_dirname/);
	$header .= "#endif\n\n";

	$header .= "int   br_init             (BrInitError *error);\n";
	$header .= "int   br_init_lib         (BrInitError *error);\n\n";

	$header .= "char *br_find_exe         (const char *default_exe);\n";
	$header .= "char *br_find_exe_dir     (const char *default_dir);\n";
	$header .= "char *br_find_prefix      (const char *default_prefix);\n";
	$header .= "char *br_find_bin_dir     (const char *default_bin_dir);\n";
	$header .= "char *br_find_sbin_dir    (const char *default_sbin_dir);\n";
	$header .= "char *br_find_data_dir    (const char *default_data_dir);\n";
	$header .= "char *br_find_locale_dir  (const char *default_locale_dir);\n";
	$header .= "char *br_find_lib_dir     (const char *default_lib_dir);\n";
	$header .= "char *br_find_libexec_dir (const char *default_libexec_dir);\n";
	$header .= "char *br_find_etc_dir     (const char *default_etc_dir);\n";
	$header .= "\n/* Utility functions */\n";
	$header .= "char *br_strcat  (const char *str1, const char *str2);\n";
	$header .= "char *br_build_path (const char *dir, const char *file);\n";
	$header .= "char *br_dirname (const char *path);\n";

	$header .= "\n\n";
	$header .= "$cppguard2\n";
	$header .= $br_h_guard2;

} elsif ($ARGV[0] eq "glib") {
	my $glib = getFile("glib.c");

	$cppguard1 = "G_BEGIN_DECLS\n";
	$cppguard2 = "G_END_DECLS\n";

	$src = "$br_license\n$br_c_guard1\n";
	$src .= $br_include;
	$src .= "#include \"$headerincludefile\"\n\n";
	$src .= "$cppguard1\n\n";

	$br_function =~ s/BrInitError/GbrInitError/g;
	$br_function =~ s/BR_INIT_ERROR/GBR_INIT_ERROR/g;
	$br_function =~ s/ malloc \(/ g_try_malloc (/g;
	$br_function =~ s/ realloc \(/ g_try_realloc (/g;
	$br_function =~ s/ strdup \(/ g_strdup (/g;
	$br_function =~ s/free \(/g_free (/g;
	$src .= "$br_function\n\n";
	$src .= "$glib\n\n";

	$src .= "$cppguard2\n";
	$src .= $br_c_guard2;

	$br_errors =~ s/BrInitError/GbrInitError/;
	$br_errors =~ s/BR_INIT_ERROR/GBR_INIT_ERROR/g;
	$header = "$br_license\n$br_h_guard1\n";
	$header .= "#include <glib.h>\n\n";
	$header .= "$cppguard1\n\n";
	$header .= "$br_errors\n\n";
	$header .= "#ifndef BINRELOC_RUNNING_DOXYGEN\n";
	$header .= mangle(qw/gbr_find_exe gbr_find_exe_dir gbr_find_prefix gbr_find_bin_dir gbr_find_sbin_dir
			     gbr_find_data_dir gbr_find_locale_dir gbr_find_lib_dir
			     gbr_find_libexec_dir gbr_find_etc_dir/);
	$header .= "#endif\n\n";

	$header .= "gboolean gbr_init             (GError **error);\n";
	$header .= "gboolean gbr_init_lib         (GError **error);\n\n";

	$header .= "gchar   *gbr_find_exe         (const gchar *default_exe);\n";
	$header .= "gchar   *gbr_find_exe_dir     (const gchar *default_dir);\n";
	$header .= "gchar   *gbr_find_prefix      (const gchar *default_prefix);\n";
	$header .= "gchar   *gbr_find_bin_dir     (const gchar *default_bin_dir);\n";
	$header .= "gchar   *gbr_find_sbin_dir    (const gchar *default_sbin_dir);\n";
	$header .= "gchar   *gbr_find_data_dir    (const gchar *default_data_dir);\n";
	$header .= "gchar   *gbr_find_locale_dir  (const gchar *default_locale_dir);\n";
	$header .= "gchar   *gbr_find_lib_dir     (const gchar *default_lib_dir);\n";
	$header .= "gchar   *gbr_find_libexec_dir (const gchar *default_libexec_dir);\n";
	$header .= "gchar   *gbr_find_etc_dir     (const gchar *default_etc_dir);\n";
	$header .= "\n\n";
	$header .= "$cppguard2\n";
	$header .= $br_h_guard2;

} else {
	print STDERR "Unknown type '$ARGV[0]'\n";
	exit 1;
}



my $filename = $srcfile;
if (!open(F, "> $filename")) {
	print STDERR "Cannot write to $filename\n";
	exit 1;
}
print F $src;
close F;
print "Source code written to '$filename'\n";

$filename = $headerfile;
if (!open(F, "> $filename")) {
	print STDERR "Cannot write to $filename\n";
	exit 1;
}
print F $header;
close F;
print "Header written to '$filename'\n";



sub getSection {
	my ($code, $section) = @_;
	my ($result) = $code =~ /\/\*\*\* $section BEGIN \*\/\n(.*?)\/\*\*\* $section END \*\//s;
	return $result;
}

sub getFile {
	my ($file) = @_;
	my ($f, $content);

	if (!open($f, "< $RealBin/$file")) {
		print STDERR "Cannot open $file\n";
		exit 1;
	}
	local($/);
	$content = <$f>;
	close $f;
	return $content;
}

sub mangle {
	my $result = "	/* Mangle symbol names to avoid symbol\n" .
		"	 * collisions with other ELF objects.\n" .
		"	 */\n";
	my $len = 0;
	foreach (@_) {
		$len = length($_) if (length($_) > $len);
	}
	foreach (@_) {
		$result .= sprintf("	#define %-${len}s %s_%s\n", $_, ${mangle}, $_);
	}
	return $result;
}
