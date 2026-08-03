#!/bin/sh

set -eu

wrapper=$1
temp_dir=$(mktemp -d)
trap 'rm -rf "$temp_dir"' EXIT HUP INT TERM

# Substitute a harmless command so accepted inputs can be checked without
# invoking APT. Rejected inputs exit before reaching this command.
#
# Fail closed if the production wrapper no longer spells the apt invocation
# this way: the substitution below would then be a silent no-op, and the
# "test" wrapper would exec the real /usr/bin/apt against the temporary
# .deb files below instead of the harmless stand-in command.
if ! grep -Fq 'exec /usr/bin/apt' "$wrapper"; then
    echo "apt-install wrapper no longer spells the apt invocation as expected; refusing to run the substitution test" >&2
    exit 1
fi

test_wrapper="$temp_dir/apt-install"
sed "s|exec /usr/bin/apt|exec /usr/bin/printf '%s\\\\n'|" "$wrapper" > "$test_wrapper"

if grep -Fq 'exec /usr/bin/apt' "$test_wrapper"; then
    echo "apt substitution did not apply to the copy under test" >&2
    exit 1
fi

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
if ! printf '%s\n' "$output" | grep -Fqx -- 'reinstall'; then
    echo "APT reinstall command was not passed" >&2
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
