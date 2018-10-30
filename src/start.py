#!/usr/bin/python

import os,sys, getpass, ConfigParser, re 

user = "markus"
password = ""
mds = "127.0.0.1"
ds = {}

srcbinname = os.path.basename(sys.argv[0])
srcdir = os.path.normpath(os.path.join(os.getcwd(),os.path.dirname(sys.argv[0])))
dsbin = os.path.join(srcdir,"netraid/dataServer")





def start_cluster():
    print "starting cluster"
    


def setup_cluster():
  print "Setup cluster"
  ds[0]="192.168.56.101" 
  #conn = rpyc.classic.connect(ds[0])
  start_rpyc(ds)
  



def parse():
  pat = re.compile("ds(?P<id>[0-9]+)" )
  cp = ConfigParser.ConfigParser()
  cp.read("conf/mds.conf")
  
  for op in cp.options("default"):
    m = pat.match(op)
    if (m):
      ds[m.group('id')] = cp.get("default",op)
  

if __name__=='__main__':
  #parse()
  #print "ssh requires user password"
  # password = getpass.getpass()

  if (len(sys.argv)>0):
    if (sys.argv[1]=="cluster"):
      start_cluster()
    elif (sys.argv[1]=="setup"):
      setup_cluster()
  
    else:
    #  os.system("rm -rf /tmp/0/*")
    #  os.system("rm -rf /tmp/1/*")
    #  os.system("rm -rf /tmp/2/*")
    #  os.system("rm -rf /tmp/3/*")
    #  os.system("rm -rf /tmp/4/*")
    #  os.system("rm -rf /tmp/5/*")
    #  os.system("rm -rf /tmp/6/*")
    #  os.system("rm -rf /tmp/7/*")
   #   os.system("rm -rf /tmp/8/*")
   #   os.system("rm -rf /tmp/9/*")
   #   os.system("rm -rf /tmp/*.log")
   #   os.system("rm -rf /tmp/DATA/*")
   #   os.system("rm -rf /tmp/storage/*")

      procid = int(sys.argv[1])
      cmd = dsbin 
      #for i in range(procid):
      print cmd , procid
      os.system("%s %s &"%(cmd,procid))

      #print cmd, procid
      #s = "  %s"%procid
      #os.execl(cmd,s)

      #print "\n<enter> to stop data server"
      #raw_input()

      #os.system("killall %s"%(dsbin))
      #os.system("killall metadataServer")
      #os.system("killall dataServer")
      #os.system("killall clientbin")
      #print "\nDataserver stopped.\n"
