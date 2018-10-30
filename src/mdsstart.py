#!/usr/bin/python
import os, sys

os.system("./metadataServer --ds.pw=%s &"%sys.argv[1])
