#!/bin/sh
#
# Copyright (c) 2005 Junio C Hamano
#

test_description='git-apply handling copy/rename patch.

'
. ./test-lib.sh

# setup

cat >test-patch <<\EOF
diff --git a/foo b/bar
similarity index 47%
copy from foo
copy to bar
--- a/foo
+++ b/bar
@@ -1 +1 @@
-This is foo
+This is bar
EOF

echo 'This is foo' >foo
chmod +x foo

test_expect_success setup \
    'git-update-cache --add foo'

test_expect_success apply \
    'git-apply --index --stat --summary --apply test-patch'

test_expect_success validate \
    'test -f bar && ls -l bar | grep "^-..x..x..x"'

test_done
