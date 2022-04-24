#!/bin/bash

# enable strict mode
set -euo pipefail
IFS=$'\t\n'

# get directory of script
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

# create bind mount directories if they don't exist
if ! [[ -d $DIR/../container-files/logs/apache ]]; then
    mkdir -p "$DIR/../container-files/logs/apache"
fi
if ! [[ -d $DIR/../container-files/logs/mysql ]]; then
    mkdir -p "$DIR/../container-files/logs/mysql"
fi
if ! [[ -d $DIR/../container-files/database ]]; then
    mkdir "$DIR/../container-files/database"
fi

# build/start containers
cd $DIR
docker-compose --compatibility up -d --build

# set proper ownership on apache logs
docker exec bok-webapp-container chown -R root:www-data /usr/local/apache2/logs
