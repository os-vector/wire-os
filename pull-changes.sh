#!/bin/bash
echo "Pulling Viccyware changes"
git pull
cd anki/victor
git checkout Viccyware-tester
./update-viccyware.sh
