#!/usr/bin/python
#-*-coding:UTF-8 -*-

import sys

fp = open(sys.argv[1], "r").read()
fp = fp.replace("../common/", "")
print fp
