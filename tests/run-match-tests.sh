
#!/bin/bash

export PATH="$PATH:."

./match_test
match_testd

match_test naive test-data/mest-proposal.txt > test-data/naive-match.txt     || exit 1
match_test border test-data/modest-proposal.txt > test-data/border-match.txt || exit 1
match_test kmp test-data/modest-proposal.txt > test-data/kmp-match.txt       || exit 1
match_test bmh test-data/modest-proposal.txt > test-data/bmh-match.txt       || exit 1

if cmp test-data/naive-match.txt test-data/border-match.txt;
then
    echo "naive and border agree"
else
    echo "naive and border disagree"
    exit 1
fi

if cmp test-data/naive-match.txt test-data/kmp-match.txt;
then
    echo "naive and KMP agree"
else
    echo "naive and KMP disagree"
    exit 1
fi

if cmp test-data/naive-match.txt test-data/bmh-match.txt;
then
    echo "naive and BMH agree"
else
    echo "naive and BMH disagree"
    exit 1
fi
