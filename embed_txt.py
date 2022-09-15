#!/usr/bin/env python
# -*- coding: utf-8 -*-

import sys

SRC_FILE= sys.argv[1]
VAR_NAME= sys.argv[2]
DST_FILE= sys.argv[3]

src = open(SRC_FILE, 'r')
Lines = src.readlines()

Lines = ["\"" + e + "\"" for e in Lines]

dst = open(DST_FILE, 'w')
dst.write("/* Auto generated from "+SRC_FILE+" */\n")
dst.write("#pragma once\nconst char* const "+VAR_NAME+"=");
for line in Lines:
    line=line.replace("\n", "\\n")
    dst.write(line + "\n")
dst.write(";")

print ("Generated " + DST_FILE + " from " + SRC_FILE)
