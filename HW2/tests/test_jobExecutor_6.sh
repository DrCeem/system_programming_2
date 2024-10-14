#!/bin/bash

export PATH=$(pwd)/../bin:$PATH

killall ./progDelay
jobCommander linux05.di.uoa.gr 12345 issueJob ./progDelay 100 &
jobCommander linux05.di.uoa.gr 12345 issueJob ./progDelay 100 &
jobCommander linux05.di.uoa.gr 12345 issueJob ./progDelay 100 &
jobCommander linux05.di.uoa.gr 12345 issueJob ./progDelay 100 & 
jobCommander linux05.di.uoa.gr 12345 issueJob ./progDelay 100 &
jobCommander linux05.di.uoa.gr 12345 issueJob ./progDelay 100 &
jobCommander linux05.di.uoa.gr 12345 setConcurrency 4 
jobCommander linux05.di.uoa.gr 12345 poll
jobCommander linux05.di.uoa.gr 12345 stop job_3
jobCommander linux05.di.uoa.gr 12345 stop job_4
jobCommander linux05.di.uoa.gr 12345 poll
jobCommander linux05.di.uoa.gr 12345 exit 