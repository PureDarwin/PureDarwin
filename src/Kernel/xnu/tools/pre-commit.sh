#!/bin/bash

#
# Abort a commit if the code style is incorrect.
#

DENYLIST=tools/uncrustify-denylist
UNCRUSTIFY="$(xcrun -f uncrustify)"

if git rev-parse --verify HEAD >/dev/null 2>&1 ; then
  printf >&2 "Validating code style diff against previous commit...\n"
  against=HEAD
else
  # Initial commit: diff against an empty tree object
  printf >&2 "Validating code style diff for entire source tree...\n"
  against=$(git hash-object -t tree /dev/null)
fi

diff_with_stdin()
{
  if which colordiff >/dev/null 2>&1; then
    diff -u "$1" - | colordiff
  else
    diff -u "$1" -
  fi
}

# Keep track of offending files
staged_paths_with_format_errors=()

# Note that we exclude staged deletions via --diff-filter
for path in $(git diff --staged --name-only --diff-filter="d" $against); do
  # Parse our deny-list to find what to skip
  while IFS= read -r deny_path; do
    # Skip empty lines and comments
    if [[ -z "$deny_path" || "$deny_path" == \#* ]]; then
      continue
    fi

    # (Prepend ./ to the path in question to match the format used in the denylist)
    # Note that excluded directories must specify a trailing slash (or the latter string here needs tweaking)
    if [[ "./$path" == "$deny_path" || "./$path" == "$deny_path"* ]]; then
      # (Continue outer loop of files to be committed)
      continue 2
    fi
  done < "$DENYLIST"

  # Skip non-C/++ files
  case "$path" in
  *.c|*.h|*.cpp)
    ;;
  *)
    continue
    ;;
  esac

  printf >&2 "Validating code style for $path: "

  if "$UNCRUSTIFY" -q -c tools/xnu-uncrustify.cfg --check -f "$path" >/dev/null 2>&1; then
    printf >&2 "\e[1;32mok\e[0m.\n"
  else
    printf >&2 "\e[1;31minvalid style\e[0m.\n"
    "$UNCRUSTIFY" -q -c tools/xnu-uncrustify.cfg -f "$path" | diff_with_stdin "$path"
    staged_paths_with_format_errors+=($path)
  fi
done

if [ ${#staged_paths_with_format_errors[@]} -ne 0 ]; then
    path_list="${staged_paths_with_format_errors[*]}"
    printf >&2 "\e[1;31mSome files have invalid code style, aborting commit. To reformat:\n"
    printf >&2 "$ $UNCRUSTIFY -q -c tools/xnu-uncrustify.cfg --replace --no-backup $path_list\e[0m\n"
    exit 1
fi

exit 0
