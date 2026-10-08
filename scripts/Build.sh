#!/bin/bash

mkdir -p ../build/

cd ../build/

cmake ..

cmake --build .

mkdir -p ../bin/

cp ./Forradia ../bin/