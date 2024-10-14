#!/bin/bash

export PATH=$(pwd)/../bin:$PATH

jobCommander linux05.di.uoa.gr 12345 issueJob ./progDelay 20 &
jobCommander linux05.di.uoa.gr 12345 poll
jobCommander linux05.di.uoa.gr 12345 exit