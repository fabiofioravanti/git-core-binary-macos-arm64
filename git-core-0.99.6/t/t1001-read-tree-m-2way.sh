#!/bin/sh
#
# Copyright (c) 2005 Junio C Hamano
#

test_description='Two way merge with read-tree -m $H $M

This test tries two-way merge (aka fast forward with carry forward).

There is the head (called H) and another commit (called M), which is
simply ahead of H.  The index and the work tree contains a state that
is derived from H, but may also have local changes.  This test checks
all the combinations described in the two-tree merge "carry forward"
rules, found in <Documentation/git-rev-tree.txt>.

In the test, these paths are used:
        bozbar  - in H, stays in M, modified from bozbar to gnusto
        frotz   - not in H added in M
        nitfol  - in H, stays in M unmodified
        rezrov  - in H, deleted in M
        yomin   - not in H nor M
'
. ./test-lib.sh

read_tree_twoway () {
    git-read-tree -m "$1" "$2" && git-ls-files --stage
}

_x40='[0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f]'
_x40="$_x40$_x40$_x40$_x40$_x40$_x40$_x40$_x40"
compare_change () {
	sed -n >current \
	    -e '/^--- /d; /^+++ /d; /^@@ /d;' \
	    -e 's/^\([-+][0-7][0-7][0-7][0-7][0-7][0-7]\) '"$_x40"' /\1 X /p' \
	    "$1"
	diff -u expected current
}

check_cache_at () {
	clean_if_empty=`git-diff-files "$1"`
	case "$clean_if_empty" in
	'')  echo "$1: clean" ;;
	?*)  echo "$1: dirty" ;;
	esac
	case "$2,$clean_if_empty" in
	clean,)		:     ;;
	clean,?*)	false ;;
	dirty,)		false ;;
	dirty,?*)	:     ;;
	esac
}

cat >bozbar-old <<\EOF
This is a sample file used in two-way fast forward merge
tests.  Its second line ends with a magic word bozbar
which will be modified by the merged head to gnusto.
It has some extra lines so that external tools can
successfully merge independent changes made to later
lines (such as this one), avoiding line conflicts.
EOF

sed -e 's/bozbar/gnusto (earlier bozbar)/' bozbar-old >bozbar-new

test_expect_success \
    setup \
    'echo frotz >frotz &&
     echo nitfol >nitfol &&
     cat bozbar-old >bozbar &&
     echo rezrov >rezrov &&
     echo yomin >yomin &&
     git-update-cache --add nitfol bozbar rezrov &&
     treeH=`git-write-tree` &&
     echo treeH $treeH &&
     git-ls-tree $treeH &&

     cat bozbar-new >bozbar &&
     git-update-cache --add frotz bozbar --force-remove rezrov &&
     git-ls-files --stage >M.out &&
     treeM=`git-write-tree` &&
     echo treeM $treeM &&
     git-ls-tree $treeM &&
     git-diff-tree $treeH $treeM'

test_expect_success \
    '1, 2, 3 - no carry forward' \
    'rm -f .git/index &&
     read_tree_twoway $treeH $treeM &&
     git-ls-files --stage >1-3.out &&
     diff -u M.out 1-3.out &&
     check_cache_at bozbar dirty &&
     check_cache_at frotz dirty &&
     check_cache_at nitfol dirty'

echo '+100644 X 0	yomin' >expected

test_expect_success \
    '4 - carry forward local addition.' \
    'rm -f .git/index &&
     git-read-tree $treeH &&
     git-checkout-cache -u -f -q -a &&
     git-update-cache --add yomin &&
     read_tree_twoway $treeH $treeM &&
     git-ls-files --stage >4.out || return 1
     diff -u M.out 4.out >4diff.out
     compare_change 4diff.out expected &&
     check_cache_at yomin clean'

test_expect_success \
    '5 - carry forward local addition.' \
    'rm -f .git/index &&
     git-read-tree $treeH &&
     git-checkout-cache -u -f -q -a &&
     echo yomin >yomin &&
     git-update-cache --add yomin &&
     echo yomin yomin >yomin &&
     read_tree_twoway $treeH $treeM &&
     git-ls-files --stage >5.out || return 1
     diff -u M.out 5.out >5diff.out
     compare_change 5diff.out expected &&
     check_cache_at yomin dirty'

test_expect_success \
    '6 - local addition already has the same.' \
    'rm -f .git/index &&
     git-read-tree $treeH &&
     git-checkout-cache -u -f -q -a &&
     git-update-cache --add frotz &&
     read_tree_twoway $treeH $treeM &&
     git-ls-files --stage >6.out &&
     diff -u M.out 6.out &&
     check_cache_at frotz clean'

test_expect_success \
    '7 - local addition already has the same.' \
    'rm -f .git/index &&
     git-read-tree $treeH &&
     git-checkout-cache -u -f -q -a &&
     echo frotz >frotz &&
     git-update-cache --add frotz &&
     echo frotz frotz >frotz &&
     read_tree_twoway $treeH $treeM &&
     git-ls-files --stage >7.out &&
     diff -u M.out 7.out &&
     check_cache_at frotz dirty'

test_expect_success \
    '8 - conflicting addition.' \
    'rm -f .git/index &&
     git-read-tree $treeH &&
     git-checkout-cache -u -f -q -a &&
     echo frotz frotz >frotz &&
     git-update-cache --add frotz &&
     if read_tree_twoway $treeH $treeM; then false; else :; fi'

test_expect_success \
    '9 - conflicting addition.' \
    'rm -f .git/index &&
     git-read-tree $treeH &&
     git-checkout-cache -u -f -q -a &&
     echo frotz frotz >frotz &&
     git-update-cache --add frotz &&
     echo frotz >frotz &&
     if read_tree_twoway $treeH $treeM; then false; else :; fi'

test_expect_success \
    '10 - path removed.' \
    'rm -f .git/index &&
     git-read-tree $treeH &&
     git-checkout-cache -u -f -q -a &&
     echo rezrov >rezrov &&
     git-update-cache --add rezrov &&
     read_tree_twoway $treeH $treeM &&
     git-ls-files --stage >10.out &&
     diff -u M.out 10.out'

test_expect_success \
    '11 - dirty path removed.' \
    'rm -f .git/index &&
     git-read-tree $treeH &&
     git-checkout-cache -u -f -q -a &&
     echo rezrov >rezrov &&
     git-update-cache --add rezrov &&
     echo rezrov rezrov >rezrov &&
     if read_tree_twoway $treeH $treeM; then false; else :; fi'

test_expect_success \
    '12 - unmatching local changes being removed.' \
    'rm -f .git/index &&
     git-read-tree $treeH &&
     git-checkout-cache -u -f -q -a &&
     echo rezrov rezrov >rezrov &&
     git-update-cache --add rezrov &&
     if read_tree_twoway $treeH $treeM; then false; else :; fi'

test_expect_success \
    '13 - unmatching local changes being removed.' \
    'rm -f .git/index &&
     git-read-tree $treeH &&
     git-checkout-cache -u -f -q -a &&
     echo rezrov rezrov >rezrov &&
     git-update-cache --add rezrov &&
     echo rezrov >rezrov &&
     if read_tree_twoway $treeH $treeM; then false; else :; fi'

cat >expected <<EOF
-100644 X 0	nitfol
+100644 X 0	nitfol
EOF

test_expect_success \
    '14 - unchanged in two heads.' \
    'rm -f .git/index &&
     git-read-tree $treeH &&
     git-checkout-cache -u -f -q -a &&
     echo nitfol nitfol >nitfol &&
     git-update-cache --add nitfol &&
     read_tree_twoway $treeH $treeM &&
     git-ls-files --stage >14.out || return 1
     diff -u M.out 14.out >14diff.out
     compare_change 14diff.out expected &&
     check_cache_at nitfol clean'

test_expect_success \
    '15 - unchanged in two heads.' \
    'rm -f .git/index &&
     git-read-tree $treeH &&
     git-checkout-cache -u -f -q -a &&
     echo nitfol nitfol >nitfol &&
     git-update-cache --add nitfol &&
     echo nitfol nitfol nitfol >nitfol &&
     read_tree_twoway $treeH $treeM &&
     git-ls-files --stage >15.out || return 1
     diff -u M.out 15.out >15diff.out
     compare_change 15diff.out expected &&
     check_cache_at nitfol dirty'

test_expect_success \
    '16 - conflicting local change.' \
    'rm -f .git/index &&
     git-read-tree $treeH &&
     git-checkout-cache -u -f -q -a &&
     echo bozbar bozbar >bozbar &&
     git-update-cache --add bozbar &&
     if read_tree_twoway $treeH $treeM; then false; else :; fi'

test_expect_success \
    '17 - conflicting local change.' \
    'rm -f .git/index &&
     git-read-tree $treeH &&
     git-checkout-cache -u -f -q -a &&
     echo bozbar bozbar >bozbar &&
     git-update-cache --add bozbar &&
     echo bozbar bozbar bozbar >bozbar &&
     if read_tree_twoway $treeH $treeM; then false; else :; fi'

test_expect_success \
    '18 - local change already having a good result.' \
    'rm -f .git/index &&
     git-read-tree $treeH &&
     git-checkout-cache -u -f -q -a &&
     cat bozbar-new >bozbar &&
     git-update-cache --add bozbar &&
     read_tree_twoway $treeH $treeM &&
     git-ls-files --stage >18.out &&
     diff -u M.out 18.out &&
     check_cache_at bozbar clean'

test_expect_success \
    '19 - local change already having a good result, further modified.' \
    'rm -f .git/index &&
     git-read-tree $treeH &&
     git-checkout-cache -u -f -q -a &&
     cat bozbar-new >bozbar &&
     git-update-cache --add bozbar &&
     echo gnusto gnusto >bozbar &&
     read_tree_twoway $treeH $treeM &&
     git-ls-files --stage >19.out &&
     diff -u M.out 19.out &&
     check_cache_at bozbar dirty'

test_expect_success \
    '20 - no local change, use new tree.' \
    'rm -f .git/index &&
     git-read-tree $treeH &&
     git-checkout-cache -u -f -q -a &&
     cat bozbar-old >bozbar &&
     git-update-cache --add bozbar &&
     read_tree_twoway $treeH $treeM &&
     git-ls-files --stage >20.out &&
     diff -u M.out 20.out &&
     check_cache_at bozbar dirty'

test_expect_success \
    '21 - no local change, dirty cache.' \
    'rm -f .git/index &&
     git-read-tree $treeH &&
     git-checkout-cache -u -f -q -a &&
     cat bozbar-old >bozbar &&
     git-update-cache --add bozbar &&
     echo gnusto gnusto >bozbar &&
     if read_tree_twoway $treeH $treeM; then false; else :; fi'

# This fails with straight two-way fast forward.
test_expect_success \
    '22 - local change cache updated.' \
    'rm -f .git/index &&
     git-read-tree $treeH &&
     git-checkout-cache -u -f -q -a &&
     sed -e "s/such as/SUCH AS/" bozbar-old >bozbar &&
     git-update-cache --add bozbar &&
     if read_tree_twoway $treeH $treeM; then false; else :; fi'

# Also make sure we did not break DF vs DF/DF case.
test_expect_success \
    'DF vs DF/DF case setup.' \
    'rm -f .git/index &&
     echo DF >DF &&
     git-update-cache --add DF &&
     treeDF=`git-write-tree` &&
     echo treeDF $treeDF &&
     git-ls-tree $treeDF &&

     rm -f DF &&
     mkdir DF &&
     echo DF/DF >DF/DF &&
     git-update-cache --add --remove DF DF/DF &&
     treeDFDF=`git-write-tree` &&
     echo treeDFDF $treeDFDF &&
     git-ls-tree $treeDFDF &&
     git-ls-files --stage >DFDF.out'

test_expect_success \
    'DF vs DF/DF case test.' \
    'rm -f .git/index &&
     rm -fr DF &&
     echo DF >DF &&
     git-update-cache --add DF &&
     read_tree_twoway $treeDF $treeDFDF &&
     git-ls-files --stage >DFDFcheck.out &&
     diff -u DFDF.out DFDFcheck.out &&
     check_cache_at DF/DF dirty &&
     :'

test_done
