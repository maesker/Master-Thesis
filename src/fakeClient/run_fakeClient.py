#!/usr/bin/python
import os
from datetime import datetime

number_of_total_files = 20000 
number_of_processes = 1

number_of_files_per_process = number_of_total_files / number_of_processes;

b_type = "-c -r -s -u -d"


if not os.path.isdir("/opt/fakeClientResults"):
	os.mkdir("/opt/fakeClientResults")
	
d = datetime.now()
date = d.strftime("%d-%B-%Y-%H-%M-%S")
fname = str(date) + "-" + str(number_of_processes) + "-clients-"  + str(number_of_total_files) + "-files" 
fname = "/opt/fakeClientResults/" + fname

cmd = 'git log | head -n 1 | cut -d " " -f 2  > ' + fname 
print "executing command: " + str(cmd)
os.system(cmd)

cmd = "mpirun.openmpi -np " + str(number_of_processes)  +  " fakeClient -n " + str(number_of_files_per_process) + str(" ") + str(b_type) + " >> " + fname 


print "executing command: +" + str(cmd)
os.system(cmd)


