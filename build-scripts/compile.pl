#! /usr/bin/perl -w

# TODO:
# -autodep mode
use Carp;

# ---------------------------------------------------------------------
#               the database of build info
# ---------------------------------------------------------------------
my %files = ();
my %objects = ();
my %libraries = ();
my %exes = ();
my %versions = ();
my %currently_building = ();
my %created = ();
my %existed = ();
my %defaults = ();
my %versions_by_suffix = ();

my $print_tags = 1;
my $print_commands = 0;
my $debug_rebuilding = 0;
my $do_run = 1;

my $must_rewrite_dependencies = 0;

sub get_object_name($$) {
  my ($version, $basename) = @_;
  return ".generated/objects/$version/$basename$obj_ext";
}

sub get_lib_name($$$) {
  my ($version, $libtype, $basename) = @_;
  return ".generated/libs/$version/lib$basename.$libtype";
}

sub force_object($) {
  my ($object_file_basename) = @_;
  my $o = $objects{$object_file_basename};
  if (!defined $o) {
    $o = { 'deps' => [],
           'src' => "$object_file_basename.c",
           'deps_by_version' => {},
	   'defines' => [],
           'pkgs' => [] };
    $objects{$object_file_basename} = $o;
  }
  return $o;
}
sub force_lib($) {
  my ($lib_file_basename) = @_;
  croak unless defined $lib_file_basename;
  my $o = $libraries{$lib_file_basename};
  if (!defined $o) {
    $o = { 'objects' => [], 'pkgs' => [] };
    $libraries{$lib_file_basename} = $o;
  }
  return $o;
}
sub force_exe($) {
  my ($exe_filename) = @_;
  my $o = $exes{$exe_filename};
  if (!defined $o) {
    $o = { 'objects' => [], 'pkgs' => [], 'libs' => [],
           'extlibs' => [] };
    $exes{$exe_filename} = $o;
  }
  return $o;
}
sub force_file($) {
  my ($filename) = @_;
  my $o = $files{$filename};
  if (!defined $o) {
    $o = { };
    $files{$filename} = $o;
  }
  return $o;
}
sub get_pkgconfig_info {
  my ($pkgs, $option) = @_;
  my %tmp = (); for (@$pkgs) { $tmp{$_} = 1 }
  my $ptag = join(' ', sort keys %tmp);
  return "" if $ptag eq '';
  my $rv = $pkg_flags{$ptag}{$option};
  if (!defined $rv) {
    $rv = `pkg-config $option $ptag`;
    die "error running pkg-config $option $ptag" unless ($?==0);
    chomp $rv;
    $pkg_flags{$ptag}{$option} = $rv;
  }
  return $rv;
}
sub has_pkg($) {
  my $pkg = $_[0];
  return system("pkg-config $pkg") == 0;
}
sub object_add_deps {
  my $object_file_basename = shift @_;
  for my $filename (@_) {
    my $o = force_object($object_file_basename);
    my $d = $o->{'deps'};
    push @$d, $filename;
  }
}
sub object_configure_warnings($$) {
  my ($object_file_basename, $flag) = @_;
  my $o = force_object($object_file_basename);
  if (!defined $o->{'warnings'}) { $o->{'warnings'} = {} }
  $o->{'warnings'}->{$flag} = 1;
}
sub object_set_source($$) {
  my ($object_file_basename, $src_file_basename) = @_;
  my $o = force_object($object_file_basename);
  $o->{'src'} = $src_file_basename;
}
sub object_add_pkgs {
  my $object_file_basename = shift @_;
  my $o = force_object($object_file_basename);
  my $pkgs = $o->{'pkgs'};
  push @$pkgs, @_
}
sub object_add_defines {
  my $object_file_basename = shift @_;
  my $o = force_object($object_file_basename);
  my $defs = $o->{'defines'};
  for (@_) { push @$defs, $_ }
}
sub lib_add_objects {
  my $lib_file = shift(@_);
  my $l = force_lib($lib_file);
  my $os = $l->{'objects'};
  push @$os, @_;
}
sub lib_add_dir_objects {
  my $lib_file = shift(@_);
  my $dir = shift(@_);
  lib_add_objects($lib_file, map { "$dir/$_" } @_);
}

sub exe_add_objects {
  my $exe_filename = shift(@_);
  my $exe = force_exe($exe_filename);
  my $os = $exe->{'objects'};
  push @$os, @_;
}
sub exe_add_libs {
  my $exe_filename = shift(@_);
  my $exe = force_exe($exe_filename);
  my $libs = $exe->{'libs'};
  for (@_) {
    die unless defined;
    push @$libs, $_;
  }
}
sub exe_add_extlibs {
  my $exe_filename = shift(@_);
  my $exe = force_exe($exe_filename);
  my $libs = $exe->{'extlibs'};
  for (@_) {
    die unless defined;
    push @$libs, $_;
  }
}
sub exe_add_pkgs {
  my $exe_filename = shift(@_);
  my $exe = force_exe($exe_filename);
  my $pkgs = $exe->{'pkgs'};
  push @$pkgs, @_
}
sub file_add_dep($$) {
  my ($file_name, $misc_rule) = @_;
  my $f = force_file ($file_name);
}
sub add_misc_rule($$$$) {
  my ($short, $inputs, $outputs, $cmds) = @_;
  my $misc_rule = { 'short' => $short,
                    'inputs' => $inputs,
                    'outputs' => $outputs,
                    'cmds' => $cmds };
  for my $output (@$outputs)
    {
      my $f = force_file($output);
      if (defined ($f->{'misc_rule'})) { die "file $output generated by more than one rule" }
      $f->{'misc_rule'} = $misc_rule;
    }
}

sub are_deps_newer {
  my $target = shift @_;
  #print "are_deps_newer: target=$target; deps=".join(' ',@_)."\n";
  return 1 unless (-r $target);
  return 0 if defined $created{$target};
  my $target_age = -M $target;
  for my $dep (@_) {
    if (defined $created{$dep}) {
      print "must rebuild $target since $dep is newly created\n" if $debug_rebuilding;
      return 1;
    }
    my $dep_age = -M $dep;
    if (!defined $dep_age || $dep_age < $target_age) {
      print "must rebuild $target (age=$target_age) due to $dep (age=$dep_age)\n" if $debug_rebuilding;
      return 1;
    }
  }
  return 0;
}

sub add_defaults {
  push @defaults, @_;
}

# ---------------------------------------------------------------------
#                         building preferences
# ---------------------------------------------------------------------
sub add_default_versions()
{
  my @std_options = (qw(-g -Wall -W -Wno-unused-parameter));
  if (defined($ENV{CFLAGS})) { push @std_options, (split /\s+/,$ENV{CFLAGS}) }
  $versions{'default'} = { 'compiler' => 'gcc',
                           'linker' => 'gcc',
                           'compile_options'  => [@std_options, "-O2"] };
  $versions{'debug'}   = { 'exe_suffix' => '_g',
                           'options'  => [@std_options, "-O0"] };
  $versions{'profile'}   = { 'exe_suffix' => '_p',
                             'compile_options'  => [@std_options, "-O2", "-pg"] };
  $versions{'unopt'}     = { 'exe_suffix' => '_0',
                             'compile_options'  => [@std_options, "-O0"] };
  for my $v (keys %versions) {
    my $ver = $versions{$v};
    my $suf = (defined($ver->{'exe_suffix'}) ? $ver->{'exe_suffix'} : '');
    $ver->{'name'} = $v;
    $versions_by_suffix{$suf} = $ver;
  }
  $CC_TEMPLATE = "__CC__ -MD -c -o __OUTPUT__ __SOURCES__ __CFLAGS__";
  $LD_TEMPLATE = "__LD__ -o __OUTPUT__ __OBJECTS__ __LIBS__ __LDFLAGS__";
  $AUTODEP_MODE = 'dot_d_files';
  $obj_ext = '.o';
  $UNAME = `uname`;
  chomp $UNAME;
  $sys_ldflags = [] unless defined $sys_ldflags;
  $sys_cflags = [] unless defined $sys_cflags;
#  if ($UNAME eq 'Darwin') {
#    $versions{'default'}->{'link_options'} = [qw(-bind_at_load)];
#    push @$sys_cflags, "-I/sw/include";
#    push @$sys_ldflags, "-L/sw/lib";
#  }
}

# ---------------------------------------------------------------------
#                    saving and loading dependency data
# ---------------------------------------------------------------------
sub save_dependencies() {
  open DEPS, ">.generated/dependencies" or return;
  for my $o (keys %objects) {
    my $obj = $objects{$o};
    die unless defined $obj;
    my $version_deps = $obj->{'deps_by_version'};
    if (defined $version_deps) {
      for my $version (keys %$version_deps) {
        my $dlist = $version_deps->{$version};
        print DEPS ".generated/objects/$version/$o$obj_ext: ".join(" ",@$dlist)."\n";
      }
    } else { warn "no deps_by_version" }
  }
  close DEPS;
}
sub normalize_path($) {
  my $f = $_[0];
  $f =~ s,//+,/,g;
  $f =~ s{^[^/]+/\.\./}{}g;
  $f =~ s{/[^/]+/\.\./}{/}g;
  $f =~ s{/[^/]+/\.\.$}{}g;
  $f =~ s{/\./}{/}g;
  $f =~ s{/\.$}{}g;
  return $f;
}
sub load_dependencies() {
  open DEPS, "<.generated/dependencies" or return;
  while (<DEPS>) {
    die unless /^([^:]+):\s*(.*)$/;
    my ($fname, $deps) = ($1,$2);
    $deps =~ s/^\s+//;
    $deps =~ s/\s+$//;
    $deps =~ s/ \s+/ /g;

    if ($fname =~ /^\.generated\/objects\/([^\/]+)\/(.*)\.o$/) {
      my ($version, $basename) = ($1,$2);
      my $o = force_object($basename);
      $o->{'deps_by_version'} = {} unless defined $o->{'deps_by_version'};
      $o->{'deps_by_version'}->{$version} = [ map {normalize_path($_)} split(/ /, $deps) ];
      $must_rewrite_dependencies = 1;
    } else {
      die "unexpected line $. in .generated/dependencies";
    }
  }
  close DEPS;
}
sub maybe_save_dependencies() {
  if ($must_rewrite_dependencies) {
    save_dependencies();
    $must_rewrite_dependencies = 0;
  }
}

# ---------------------------------------------------------------------
#                              running commands
# ---------------------------------------------------------------------
sub run {
  my $tag = shift;
  print STDERR "$tag\n" if ($print_tags);
  for my $cmd (@_) {
    print STDERR "$cmd\n" if $print_commands;
    if ($do_run) {
      my $rv = system($cmd);
      if ($rv != 0) {
        if ($rv >= 256) { warn ("running '$cmd' exitted with code " . ($rv>>8) . " (tag=$tag)") }
        else { warn "'$cmd' killed by signal $rv" }
        maybe_save_dependencies();
        exit(1);
      }
    }
  }
}

# ---------------------------------------------------------------------
#                    building: default compilation methods
# ---------------------------------------------------------------------
sub get_for_version_or_default($$$) {
  my ($version, $param, $def) = @_;
  my $rv = $versions{$version}->{$param};
  $rv = $versions{'default'}->{$param} unless defined $rv;
  $rv = $def unless defined $rv;
  return $rv;
}
sub mkdir_p($);
sub mkdir_p($) {
  my $d = $_[0];
  return if -d $d;
  if (!mkdir($d, 0755)) {
    my $t = $d;
    die "error making $d" unless ($t =~ s/\/[^\/]+$//);
    mkdir_p($t);
    mkdir($d, 0755) or die "mkdir($d) failed: $!"
  }
}
sub ensure_dir($) {
  my $f = $_[0];
  if ($f =~ s/\/[^\/]+$//) { mkdir_p($f); }
}
sub do_build_executable {
  my ($version, $exe_basename) = @_;
  my $suffix = get_for_version_or_default($version, "exe_suffix", "");
  my $linker = get_for_version_or_default($version, "linker", "cc");
  my $link_options = get_for_version_or_default($version, "link_options", []);
  my $cmd = get_for_version_or_default($version, "link_template", $LD_TEMPLATE);
  my $exe = force_exe($exe_basename);
  my %libs = ();
  my %objects = ();
  my %pkgs = map {$_ => 1} (split /\s+/, $default_pkgs);
  my $tmplibs; my $tmpobjs;
  my $output = "$exe_basename$suffix";
  $tmplibs = $exe->{'libs'};
  $tmpobjs = $exe->{'objects'};
  $tmppkgs = $exe->{'pkgs'};
  $tmpextlibs = $exe->{'extlibs'};
  for (@$tmppkgs) { $pkgs{$_} = 1 }
  my @libqueue = (@$tmplibs);
  for (@$tmpobjs) { $objects{$_} = 1 }
  while (scalar(@libqueue) != 0)
    {
      my $lib = shift @libqueue;
      my $L = force_lib($lib);
      if ($L->{'type'} eq 'symbolic') {
	my $Lo = $L->{'objects'};
	for (@$Lo) { $objects{$_} = 1 }
	my $Ll = $L->{'libs'};
	push @libqueue, @$Ll if defined $Ll;
	my $Lp = $L->{'pkgs'};
	for (@$Lp) { $pkgs{$_} = 1 }
      } else {
        $libs{$lib} = 1;
      }
    }
  my @extlibs = ();
  for (@$tmpextlibs) { push @extlibs, "-l$_" }
  my $libs = join(' ', keys %libs, @$sys_ldflags, @extlibs);
  my $pkg_ldflags = get_pkgconfig_info([keys %pkgs], '--libs');
  my $objects = join(' ', map {get_object_name($version, $_)} (sort keys %objects));
  $cmd =~ s/__LD__/$linker/g;
  $cmd =~ s/__OUTPUT__/$output/g;
  $cmd =~ s/__OBJECTS__/$objects/g;
  $cmd =~ s/__LIBS__/$libs -lm/g;
  my $ldflags = join(' ', $pkg_ldflags, @$link_options);
  $cmd =~ s/__LDFLAGS__/$ldflags/g;
  ensure_dir($output);
  run("LD $output", $cmd);
  $created{$output} = 1;
}
sub do_build_library() {
  die;
}
sub read_dep_makefile($) {
  my $fname = $_[0];
  open DM, "<$fname" or die "could not open $fname";
  $all = '';
  while (<DM>) {
    chomp;
    s/\\$/ /;
    $all .= $_;
  }
  $all =~ s/^.*?://;
  return map {normalize_path($_)} (split /\s+/, $all);
}
sub do_build_object($$) {
  my ($version, $basename) = @_;
  my $obj = force_object($basename);
  my $tag = "CC $basename";
  $tag .= " [$version]" unless ($version eq 'default');
  my $cc = get_for_version_or_default($version, "compiler", "cc");
  my $cflags = get_for_version_or_default($version, "compile_options", []);
  my $depmode = get_for_version_or_default($version, 'dep_mode', $AUTODEP_MODE);
  $cflags = join(" ", @$cflags);
  my $cmd = get_for_version_or_default($version, 'cc_template', $CC_TEMPLATE);
  my $output = get_object_name($version, $basename);
  my $pkgs = [ split /\s+/, $default_pkgs ];
  my $obj_pkgs = $obj->{'pkgs'};
  for (@$obj_pkgs) { push @$pkgs, $_; }
  my $src = $obj->{'src'};
  $cflags .= " " . get_pkgconfig_info($pkgs, '--cflags')
                 . ' ' . join(' ', @$sys_cflags);
  if ($obj->{'warnings'})
    {
      my $w = $obj->{'warnings'};
      $cflags .= " -Wno-sign-compare" if defined $w->{'no-sign-compare'};
      $cflags .= " -Wno-unused-variable -Wno-unused-value" if defined $w->{'no-unused'};
      $cflags .= " -Wno-uninitialized" if defined $w->{'no-uninitialized'};
      $cflags .= " -Wno-unused-function" if defined $w->{'no-unused-function'};
    }
  my $defs = $obj->{'defines'};
  for (@$defs)
    {
      $cflags .= " -D$_";
    }
  $cmd =~ s/__CC__/$cc/g;
  $cmd =~ s/__CFLAGS__/$cflags/g;
  $cmd =~ s/__OUTPUT__/$output/g;
  $cmd =~ s/__SOURCES__/$src/g;
  ensure_dir($output);
  run($tag, $cmd);

  if ($depmode eq 'dot_d_files') {
    my $dfile = $output;
    $dfile =~ s/$obj_ext$/.d/;
    my $vd = $obj->{'deps_by_version'};
    $vd->{$version} = [read_dep_makefile($dfile)];
    unlink($dfile);
    $must_rewrite_dependencies = 1;
  }

  $created{$output} = 1;
}

# ---------------------------------------------------------------------
#                    building (generics)
# ---------------------------------------------------------------------

sub do_run_misc_rule($) {
  my $misc_rule = $_[0];
  my $outs = $misc_rule->{'outputs'};
  for (@$outs) { ensure_dir($_) }
  my $cmds = $misc_rule->{'cmds'};
  run ($misc_rule->{'short'}, @$cmds);
  for (@$outs) { $created{$_} = 1 }
}

sub build_target($);
sub build_target($) {
  my $t = $_[0];

  return if defined $created{$t};
  return if defined $existed{$t};

  if (defined($currently_building{$t})) { die "circular dependency on $t" }
  $currently_building{$t} = 1;

  # classify the target and build it.
  if ($t =~ /^\.generated\/objects\/([^\/]+)\/(.*)\.o$/) {
    my ($version,$basename) = ($1,$2);
    my $o = force_object($basename);
    my $version_deps = $o->{'deps_by_version'};
    my @deps = ();
    if (defined $version_deps->{$version})
      { my $list = $version_deps->{$version};
	push @deps, @$list }
    else
      { my $d = $o->{'deps'}; push @deps, @$d; }

    # check existence/dependencies
    for my $d (@deps) { build_target($d); }
    if (scalar(@deps) == 0
     || are_deps_newer($t, @deps)) {
      do_build_object($version,$basename);
    }
  } elsif ($t =~ /^\.generated\/libs\/([^\/]+)\/lib([^\/]+)(\.so)$/
      ||   $t =~ /^\.generated\/libs\/([^\/]+)\/lib([^\/]+)(\.symbolic)$/
      ||   $t =~ /^\.generated\/libs\/([^\/]+)\/lib([^\/]+)(\.a$)/) {
    my ($version,$basename,$extension) = ($1,$2,$3);
    my $lib = force_lib($basename);
    my $objects = $lib->{'objects'};
    my @deps = ();

    # check existence/dependencies
    for my $lo (@$objects) {
      my $oname = get_object_name($version, $lo);
      push @deps, $oname;
      build_target($oname);
    }

    if (are_deps_newer($t, @deps)) {
      ensure_dir($t);
      if ($extension eq '.symbolic') {
        ### SHOULD HAVE A FUNCTION THAT TAKES ["TAG", ["touch $t"], {open TOUCH...}]
        print "TOUCH $version/lib$basename$extension\n" if $print_tags;
        print "touch $t\n" if $print_commands;
        if ($do_run) {
          open TOUCH, ">$t" or die "error touching $t";
          close TOUCH;
        }
        $created{$t} = 1;
      } else {
        die "no library building implemented";
      }
    }
  } else {
    # is it an executable?
    for my $exe (keys %exes) {
      if ($t =~ /^$exe(.*)/) {
	my $suffix = $1;
	my $ver = $versions_by_suffix{$suffix};
        if (!defined $ver) {
	  warn "no version with suffix '$suffix' found" unless ($suffix eq '.c');
	  next;
	}
	my $vername = $ver->{'name'};
	die unless defined  $vername;

	my $E = force_exe($exe);
	my $tmp;
	my @all_deps = ();
	$tmp = $E->{'libs'};
	for my $ttt (@$tmp)
          {
             my $LL = force_lib($ttt);
             my $tmp2 = get_lib_name($vername, $LL->{'type'}, $ttt);
             push @all_deps, $tmp2;
             build_target($tmp2);
          }
	$tmp = $E->{'objects'};
	for my $ttt (@$tmp) { my $tmp2 = get_object_name($vername, $ttt); push @all_deps, $tmp2; build_target($tmp2); }

	if (are_deps_newer($t, @all_deps)) {
	  do_build_executable($vername, $exe);
	}
	goto DONE;
      }
    }

    # is a misc_rule-generated file?
    my $f = $files{$t};
    if ((defined $f) && (defined $f->{'misc_rule'})) {
      my $rule = $f->{'misc_rule'};
      my $ins = $rule->{'inputs'};
      for my $in (@$ins) {
        build_target($in);
      }
      if (are_deps_newer($t, @$ins)) {
        do_run_misc_rule($rule);
      }
      goto DONE
    }

    # does it exist?
    if (-e $t) {
      $existed{$t} = 1;
      goto DONE;
    }

    # die
    maybe_save_dependencies();
    die "no rule to build $t";
  }
DONE:
  delete $currently_building{$t};
}

sub usage
{
  print <<"EOF";
usage: $0 [OPTIONS] TARGETS

OPTIONS include:
   -d, --debug           Debug.
   -n, --dry-run         Do not run anything.
   -p, --print-commands  Print the actual commands that are run.
   --no-tags             Do not print taglines.
EOF
  exit(1);
}

sub do_build {
  my @targets = ();
  for (@ARGV) {
    if (/^-d$/ || /^--debug$/) { $debug_rebuilding = 1 }
    elsif (/^-n$/ || /^--dry-run$/) { $do_run = 0 }
    elsif (/^-p$/ || /^--print-commands$/) { $print_commands = 1 }
    elsif (/^--no-tags$/) { $print_tags = 0 }
    elsif (/^-/) {
      usage();
    } else {
      push @targets, $_
    }
  }
  if (scalar(@targets) == 0) {
    @targets = @defaults;
    die "no default targets or specified targets" if (scalar(@targets) == 0);
  }
  for my $target (@targets) {
    build_target($target);
  }
}

#####################################
add_default_versions();
load_dependencies();
$default_pkgs = 'gsk-1.0';
exe_add_objects("build-tools/lemon", "build-tools/lemon");
add_misc_rule("LEMON core/bb-p",
              [qw(build-tools/lemon core/bb-p.lemon)], 
              [qw(core/bb-p.h core/bb-p.c)],
              ["build-tools/lemon core/bb-p.lemon"]);
object_add_deps("core/bb-init",
		".generated/init-headers.inc",
		".generated/init-code.inc");
object_add_deps("core/bb-vm-init",
		".generated/binding-impls.inc",
		".generated/binding-regs.inc");
exe_add_objects("build-tools/make-bindings", "build-tools/make-bindings");
exe_add_libs("build-tools/make-bindings", "bb");


add_misc_rule("MAKE-BINDINGS",
	      [qw(core/binding-data
		  build-tools/make-bindings)],
	      [qw(.generated/binding-impls.inc
		  .generated/binding-regs.inc)],
	      ["build-tools/make-bindings"]);

# Shaker includes its own data via macro now.
@enum_files = (qw(core/bb-utils.h instruments/random_envelope.h
		  core/bb-fft.h));
add_misc_rule("MKENUMS ".join(' ',@enum_files),
              [@enum_files, "build-scripts/mkenums.template"],
              [qw(.generated/bb-enums.inc)],
              ["glib-mkenums --template build-scripts/mkenums.template ".join(' ',@enum_files) . " > .generated/bb-enums.inc"]);
object_add_deps("core/bb-enums", ".generated/bb-enums.inc");

my @init_files = (<instruments/*.c>, <core/*.c>);
add_misc_rule("GEN_INIT",
              [qw(build-scripts/gen-init.pl), @init_files],
              [qw(.generated/init-headers.inc .generated/init-code.inc)],
              ["perl build-scripts/gen-init.pl instruments/*.c core/*.c"]);
lib_add_dir_objects("bb", "core",
                    qw(bb-enums bb-duration bb-score bb-score-render
                       wav-header bb-input-file bb-instrument bb-gnuplot bb-script
                       bb-parse bb-types bb-fft bb-filter1 bb-function bb-inlines
                       bb-rate-limiter bb-tapped-delay-line bb-render 
                       bb-tuning bb-tuning-defaults bb-vm bb-vm-parse bb-waveform
                       bb-p bb-init bb-instrument-multiplex bb-utils bb-xml));
lib_add_dir_objects("bb", "instruments",
                    qw(adsr buzz complex constant convolve exp_decay fade
                       mix modulate slide_whistle vibrato noise pink_noise
                       param_map reverse gain filter offset sin poly_sin sweep
                       noise_filterer normalize compose waveform wavetable waveshape
		       random_envelope grain0 grain1 grain2 fractal0
                       variable_fade wave_train
 
                       fm fm_drum fm_piano piano1 particle_system
                       bowed brass clarinet modal piano shaker voice0));
force_lib('bb')->{'type'} = 'symbolic';
object_add_deps("core/bb-vm-parse", "core/bb-p.h");
exe_add_libs("bb-run", "bb");
exe_add_objects("bb-run", qw(programs/bb-run core/bb-vm-init));
exe_add_libs("bb-analyze", "bb");
exe_add_objects("bb-analyze", qw(programs/bb-analyze programs/bb-font core/bb-vm-init));
exe_add_pkgs("bb-perform", "gtk+-2.0");
exe_add_libs("bb-perform", "bb");
exe_add_objects("bb-perform", qw(programs/bb-perform programs/mix programs/bb-sample-widget
                                 programs/bb-perform-interpreter));
object_add_pkgs('programs/bb-perform', 'gtk+-2.0', 'gthread-2.0');
object_add_pkgs('programs/bb-sample-widget', 'gtk+-2.0', 'gthread-2.0');
object_add_pkgs('programs/mix', 'gthread-2.0');
exe_add_libs("bb-test-compile-expr", "bb");
exe_add_objects("bb-test-compile-expr", qw(tests/test-compile-expr));
exe_add_libs("test-delay-lines", "bb");
exe_add_objects("test-delay-lines", qw(tests/test-delay-lines));
exe_add_libs("test-beat-computations", "bb");
exe_add_objects("test-beat-computations", qw(tests/test-beat-computations));
exe_add_libs("test-convolve", "bb");
exe_add_objects("test-convolve", qw(tests/test-convolve));
object_configure_warnings("core/bb-p", 'no-sign-compare');
object_configure_warnings("build-tools/lemon", 'no-unused');
object_configure_warnings("build-tools/lemon", 'no-uninitialized');
object_configure_warnings("programs/mix", 'no-unused-function');

my $audio_mode = undef;
if (-r "/usr/include/linux/soundcard.h") {
  $audio_mode = 'OSS';
  object_add_defines('programs/mix', "OSS_AUDIO");
} elsif ((-r "/usr/include/portaudio.h")
    ||   (-r "/sw/include/portaudio.h"))
{
  $audio_mode = 'portaudio';
  object_add_defines('programs/mix', "PORTAUDIO_AUDIO");
  exe_add_extlibs("bb-perform", 'portaudio');
}
 

exe_add_libs("tlip", "bb");
exe_add_objects("tlip", qw(tlip));

add_defaults("bb-run");
add_defaults("bb-analyze");
add_defaults("build-tools/make-bindings");
if (has_pkg('gtk+-2.0')) {
  if (defined $audio_mode) {
    add_defaults("bb-perform");
  } else {
    print STDERR "note: not building bb-perform because no audio-device driver found.\n";
  }
} else {
  print STDERR "note: not building bb-perform because gtk not found.\n";
}
do_build();
maybe_save_dependencies();
exit(0);
