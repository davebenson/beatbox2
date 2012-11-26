#! /usr/bin/perl -w

open OH, ">.generated/init-headers.inc" or die;
open OC, ">.generated/init-code.inc" or die;

for my $f (@ARGV) {
  open X, "<$f"  or die "could not open $f: $!";
  while (<X>) {
    if (/^\s*BB_INIT_DEFINE\s*\(\s*(\S+)\s*\)\s*$/) {
      my $func = $1;
      print OH "void $func(void);\n";
      print OC "$func();\n";
    }
  }
}
