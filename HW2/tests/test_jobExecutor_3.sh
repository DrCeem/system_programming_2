#!/bin/bash

export PATH=$(pwd)/../bin:$PATH

jobCommander linux05.di.uoa.gr 12345 issueJob ls -l /usr/bin/* /usr/local/bin/* /bin/* /sbin/* /opt/* /etc/* /usr/sbin/*
jobCommander linux05.di.uoa.gr 12345 exit