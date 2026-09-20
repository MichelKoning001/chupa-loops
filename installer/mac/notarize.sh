#!/bin/bash
# Submits a file to Apple's notary service, prints Apple's log when it is rejected, and staples the ticket.
# needs APPLE_ID, APPLE_TEAM_ID, APPLE_APP_PASSWORD in the environment
set -euo pipefail
FILE="$1"
RESULT=$(xcrun notarytool submit "$FILE" --apple-id "$APPLE_ID" --team-id "$APPLE_TEAM_ID" \
                --password "$APPLE_APP_PASSWORD" --wait --output-format json) || true
echo "$RESULT"
ID=$(echo "$RESULT" | /usr/bin/python3 -c 'import json,sys; print(json.load(sys.stdin).get("id",""))')
STATUS=$(echo "$RESULT" | /usr/bin/python3 -c 'import json,sys; print(json.load(sys.stdin).get("status",""))')
if [ "$STATUS" != "Accepted" ]; then
    echo "Notarization failed ($STATUS). Apple's log:"
    [ -n "$ID" ] && xcrun notarytool log "$ID" --apple-id "$APPLE_ID" --team-id "$APPLE_TEAM_ID" --password "$APPLE_APP_PASSWORD" || true
    exit 1
fi
xcrun stapler staple "$FILE"
