#!/bin/sh

set -eu

wrapper=$1
temp_dir=$(mktemp -d)
trap 'rm -rf "$temp_dir"' EXIT HUP INT TERM

# Substitute a harmless command so accepted inputs can be checked without
# invoking APT. Rejected inputs exit before reaching this command.
test_wrapper="$temp_dir/apt-install"
sed "s|exec /usr/bin/apt|exec /usr/bin/printf '%s\\\\n'|" "$wrapper" > "$test_wrapper"
chmod +x "$test_wrapper"

valid_package=$(mktemp "$temp_dir/package.XXXXXX.deb")
canonical_package=$(readlink -f -- "$valid_package")
output=$("$test_wrapper" "$valid_package")

if ! printf '%s\n' "$output" | grep -Fqx -- "$canonical_package"; then
    echo "validated package path was not passed to APT" >&2
    exit 1
fi
if ! printf '%s\n' "$output" | grep -Fqx -- '--'; then
    echo "APT argument separator was not passed" >&2
    exit 1
fi

rejects()
{
    if "$test_wrapper" "$@" >/dev/null 2>&1; then
        echo "wrapper accepted invalid arguments: $*" >&2
        exit 1
    fi
}

rejects
rejects relative.deb

mkdir "$temp_dir/directory.deb"
rejects "$temp_dir/directory.deb"

ln -s /etc/passwd "$temp_dir/link.deb"
rejects "$temp_dir/link.deb"
