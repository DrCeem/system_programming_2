#!/bin/bash

export PATH=$(pwd)/../bin:$PATH

jobCommander linux05.di.uoa.gr 12345 issueJob ./progDelay 100 &
jobCommander linux05.di.uoa.gr 12345 issueJob ./progDelay 110 &
jobCommander linux05.di.uoa.gr 12345 issueJob ./progDelay 115 &
jobCommander linux05.di.uoa.gr 12345 issueJob ./progDelay 120 & 
jobCommander linux05.di.uoa.gr 12345 issueJob ./progDelay 125 &
jobCommander linux05.di.uoa.gr 12345 poll
jobCommander linux05.di.uoa.gr 12345 setConcurrency 2
jobCommander linux05.di.uoa.gr 12345 poll
jobCommander linux05.di.uoa.gr 12345 exit