#!/bin/bash
ALL_FILES=$(ls onsite-test/workspace_logs | grep ws-expirer-scratch)
for file in $ALL_FILES
do
  date=$(echo "$file" | grep -oE '[0-9]{4}-[0-9]{2}-[0-9]{2}')
  python3 onsite-test/cmp-expirer-logs.py -o onsite-test/workspace_logs/$file -n onsite-test/workspace_logs/ws-expirer-v2-scratch-$date-01-00.log
done