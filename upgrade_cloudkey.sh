#!/bin/bash

echo "This will probably take a while as the container needs to rebuild."

git pull

docker build -t agreenbhm/unifi-cloudkey:latest --network host .

docker-compose up -d