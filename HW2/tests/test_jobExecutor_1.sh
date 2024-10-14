#!/bin/bash

export PATH=$(pwd)/../bin:$PATH

jobCommander linux05.di.uoa.gr 12345 issueJob touch myFile.txt
ls ../bin/myFile.txt
jobCommander linux05.di.uoa.gr 12345 issueJob rm myFile.txt
ls ../bin/myFile.txt
jobCommander linux05.di.uoa.gr 12345 exit