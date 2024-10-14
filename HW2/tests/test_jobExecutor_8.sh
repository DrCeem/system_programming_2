#!/bin/bash

export PATH=$(pwd)/../bin:$PATH

killall ./progDelay
jobCommander linux05.di.uoa.gr 12345 issueJob ./jobCommander linux05.di.uoa.gr 12345 poll 
jobCommander linux05.di.uoa.gr 12345 exit