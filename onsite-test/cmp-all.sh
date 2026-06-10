#!/bin/bash
for filepath in onsite-test/workspace_logs/ws-expirer-scratch-*.log; do
  file=$(basename "$filepath")
  date=$(echo "$file" | grep -oE '[0-9]{4}-[0-9]{2}-[0-9]{2}')
  python3 onsite-test/cmp-expirer-logs.py -o "$filepath" -n "onsite-test/workspace_logs/ws-expirer-v2-scratch-$date-01-00.log"
  echo ""
done