
set -e
pkgs="gsk-1.0"
cflags="-g -W -Wall -Wno-unused-parameter `pkg-config --cflags $pkgs`"
ldflags="-g `pkg-config --libs $pkgs` -lm"
test -d .generated || mkdir .generated
for subdir in instruments core programs build-tools tests ; do
  test -d .generated/$subdir || mkdir .generated/$subdir
done

compile () {
  basename="$1"
  shift
  echo "CC $basename.c"
  #echo gcc -c $cflags $* -o .generated/$basename.o $basename.c
  gcc -c $cflags $* -o .generated/$basename.o $basename.c
}
lib_compile () {
  libbasenames="$libbasenames $1"
  compile $*
}

link () {
  out="$1"
  shift
  objects=""
  for a in $* ; do objects="$objects .generated/$a.o" ; done
  echo "LINK $out $*"
  gcc -o $out $objects $ldflags
}
lemon () {
  echo "LEMON $1.lemon"
  build-tools/lemon $1.lemon
}

compile build-tools/lemon
link build-tools/lemon build-tools/lemon
lemon core/bb-p

lib_compile core/bb-score
lib_compile core/wav-header
lib_compile core/bb-instrument
lib_compile core/bb-gnuplot
lib_compile core/bb-script
lib_compile core/bb-parse
lib_compile core/bb-types
lib_compile core/bb-filter1
lib_compile core/bb-inlines
lib_compile core/bb-vm
lib_compile core/bb-vm-parse
lib_compile core/bb-waveform
lib_compile core/bb-p -Wno-sign-compare
echo "GEN_INIT"
perl build-scripts/gen-init.pl instruments/*.c core/*.c
lib_compile core/bb-init
lib_compile core/bb-instrument-multiplex
lib_compile core/bb-utils
lib_compile instruments/adsr
lib_compile instruments/constant
lib_compile instruments/exp_decay
lib_compile instruments/fade
lib_compile instruments/linear
lib_compile instruments/mix
lib_compile instruments/modulate
lib_compile instruments/slide_whistle
lib_compile instruments/vibrato
lib_compile instruments/noise
lib_compile instruments/param_map
lib_compile instruments/reverse
lib_compile instruments/gain
lib_compile instruments/filter
lib_compile instruments/sin
lib_compile instruments/noise_filterer
lib_compile instruments/compose
lib_compile instruments/waveform
lib_compile instruments/wavetable

#compile programs/test-0
#link test-0 programs/test-0 $libbasenames
compile programs/bb-run
link bb-run programs/bb-run $libbasenames
compile tests/test-compile-expr
link bb-test-compile-expr tests/test-compile-expr $libbasenames
