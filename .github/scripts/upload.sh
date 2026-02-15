#!/bin/bash
set -e

# Check if files exist
for file in "$@"; do
    if [ ! -e "$file" ]; then
        echo "Error: $file not found"
        exit 1
    fi
done

if [ $# -eq 0 ]; then
    echo "No files to upload"
    exit 0
fi

# Environment variables
REPO="${GITHUB_REPOSITORY}"
TOKEN="${GITHUB_TOKEN}"
COMMIT="${GITHUB_SHA}"
TAG="continuous"
BODY="${UPLOADTOOL_BODY:-Automated build}"

if [ -z "$REPO" ] || [ -z "$TOKEN" ]; then
    echo "Error: GITHUB_REPOSITORY and GITHUB_TOKEN must be set"
    exit 1
fi

API="https://api.github.com/repos/${REPO}"

echo "Repository: $REPO"
echo "Commit: $COMMIT"
echo "Tag: $TAG"

# Delete old release
RELEASE_ID=$(curl -s -H "Authorization: token $TOKEN" \
    "${API}/releases/tags/${TAG}" | grep -m 1 '"id":' | sed 's/[^0-9]*//g')

if [ ! -z "$RELEASE_ID" ]; then
    echo "Deleting old release..."
    curl -s -X DELETE -H "Authorization: token $TOKEN" \
        "${API}/releases/${RELEASE_ID}"
fi

# Delete old tag
curl -s -X DELETE -H "Authorization: token $TOKEN" \
    "${API}/git/refs/tags/${TAG}" 2>/dev/null || true

# Create new release
echo "Creating release..."
RELEASE_DATA=$(curl -s -X POST -H "Authorization: token $TOKEN" \
    -d "{
        \"tag_name\": \"${TAG}\",
        \"target_commitish\": \"${COMMIT}\",
        \"name\": \"Continuous Build\",
        \"body\": $(echo "$BODY" | jq -Rs .),
        \"draft\": false,
        \"prerelease\": true
    }" "${API}/releases")

UPLOAD_URL=$(echo "$RELEASE_DATA" | grep -o '"upload_url": *"[^"]*"' | head -1 | sed 's/"upload_url": *"\([^"]*\)".*/\1/' | sed 's/{.*}//')

if [ -z "$UPLOAD_URL" ]; then
    echo "Error: Failed to create release"
    exit 1
fi

# Upload files
for FILE in "$@"; do
    BASENAME=$(basename "$FILE")
    echo "Uploading: $BASENAME"
    
    curl -s -X POST \
        -H "Authorization: token $TOKEN" \
        -H "Content-Type: application/octet-stream" \
        --data-binary "@${FILE}" \
        "${UPLOAD_URL}?name=${BASENAME}"
    
    echo "✓ Uploaded: $BASENAME"
done

echo "Done!"