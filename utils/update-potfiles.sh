#!/bin/sh
exec >po/POTFILES.in

echo '# List of source files containing translatable strings.'
echo '# This is a GENERATED file.'

for dir in libgwyddion libprocess libdraw libgwydgets libgwymodule app modules; do
  echo
  echo "# $dir"
  find $dir -name \*.c | xargs grep -E -l '\<N?_\(' \
    | grep -v '/\(main\|test\)\.c' | sort
done
