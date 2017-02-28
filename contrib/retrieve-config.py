#!/usr/bin/env python
import sys
import os
import getopt
import shutil
from subprocess import call

try:
    import paramiko
except:
    print "Unable to import the Paramiko engine; please install"
    print "before continuing"
    sys.exit(1)

def help(argz):
    print "%s: %s [-u username] [-p password] [-k keyfile] hostname(s)..." % (argz, argz)
    print "Connect to a host, and execute plotnetcfg"
    print "-u username\tOptional username to use when connecting"
    print "-p password\tOptional password to use when connecting"
    print "-k keyfile \tOptional keyfile to use for key-based authentication"

def which(command):
    for path in os.environ["PATH"].split(os.pathsep):
        path = path.strip('"')
        executable = os.path.join(path, command)
        if os.path.isfile(executable) and os.access(executable, os.X_OK):
            return executable
    return None
    
USERNAME = os.getenv('PLOT_USER')
PASSWORD = os.getenv('PLOT_PASS')
KEY = os.getenv('PLOT_KEY')

# With standard gnu compilers, the following mapping tables may not make
# sense. However, some cross compilers (including the code sourcery) will
# not follow any kind of algorithmic mapping
host_compiler_mapping = { 'x86_64': ['x86_64-linux-gnu-gcc', 'x86_64-redhat-linux-gcc'], 'i686': ['i686-linux-gnu-gcc', 'i686-redhat-linux-gcc'], 'i386': ['i386-linux-gnu-gcc', 'i386-redhat-linux-gcc'], 'arm': ['arm-linux-gnu-gcc'], }

try:
    optlist, args = getopt.getopt(sys.argv[1:], 'u:p:k:h')
except:
    print "Failed to getopt:"
    print args
    sys.exit(1)

for o,a in optlist:
    if o == '-u':
        USERNAME=a
    elif o == '-p':
        PASSWORD=a
    elif o == '-k':
        KEY=a
    elif o == '-h':
        help(sys.argv[0])
        sys.exit(0)

if len(args) == 0:
    help(sys.argv[0])
    sys.exit(1)

for host in args:
    print "Connecting to %s" % host

    try:
        ssh = paramiko.SSHClient()
        ssh.load_system_host_keys()
        ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
        ssh.connect(host,username=USERNAME,password=PASSWORD,key_filename=KEY)
    except:
        print "Unable to successfully connect to %s" % host
        print "aborting remaining operations"
        sys.exit(1)

    try:
        stdin, stdout, stderr = ssh.exec_command("uname -m")
    except paramiko.SSHException:
        print "Unable to determine host type for %s. Aborting." % host
        sys.exit(1)

    unamedata = stdout.read().strip('\n').strip('\r').strip(' ')
    print "Host %s reports %s machinetype" % (host, unamedata)

    compiler_list = host_compiler_mapping[unamedata]
    if not compiler_list:
        print "Unknown host type '%s'. Please configure a cross-compiler and"
        print "send a patch to add support to retrieve-config.py"
        sys.exit(1)

    use_compiler = None
    for compiler in compiler_list:
        use_compiler = which(compiler)

    if use_compiler is None:
        print "Appropriate compiler for %s not found. Install one of the"
        print "following compilers and rerun:"
        for compiler in compiler_list:
            print " * %s" % compiler
        sys.exit(1)
    
    if not os.path.exists('%s_build/plotnetcfg.bin' % host):
        os.makedirs('%s_build' % host)

        if not os.path.exists('%s_build/plotnetcfg' % host):
            call(['git', 'clone', 'https://github.com/jbenc/plotnetcfg', '%s_build/plotnetcfg' % host])
        if not os.path.exists('%s_build/jansson' % host):
            call(['git', 'clone', 'https://github.com/akheron/jansson', '%s_build/jansson' % host])
        
        os.chdir('%s_build/jansson' % host)
        call(['autoreconf', '-if'])
        call(['./configure', '--host=%s' % unamedata, '--enable-static', '--disable-shared'])
        call(['make'])
        if not os.path.exists('src/.libs/libjansson.a'):
            print "Failed the libjansson build - manually try and build the"
            print "appropriate plotnetcfg binary and save it as:"
            print "%s_build/plotnetcfg.bin" % host
            sys.exit(1)
        
        
        os.chdir('../plotnetcfg')
        call(['make', 'jansson=../jansson', 'CC=%s' % use_compiler, 'EXTRA_CFLAGS="-static"', 'LDFLAGS="-static"'])

        if not os.path.exists('plotnetcfg'):
            print "Failure building ``plotnetcfg``: rebuild manually and"
            print "place in %s_build/plotnetcfg.bin"
            sys.exit(1)

        shutil.copyfile('plotnetcfg', '../plotnetcfg.bin')
        os.chdir('../../')

    sftp = paramiko.SFTPClient.from_transport(ssh.get_transport())

    sftp.put('%s_build/plotnetcfg.bin' % host, '/tmp/plotnetcfg.bin')

    try:
        stdin, stdout, stderr = ssh.exec_command("chmod +x /tmp/plotnetcfg.bin")
    except paramiko.SSHException:
        print "Unable to determine host type for %s. Aborting." % host
        sys.exit(1)

    try:
        stdin, stdout, stderr = ssh.exec_command("/tmp/plotnetcfg.bin")
    except paramiko.SSHException:
        print "Unable to determine host type for %s. Aborting." % host
        sys.exit(1)

    open('%s_build/plotnetcfg.out' % host, "w+").write(stdout.read())
    open('%s_build/plotnetcfg.err' % host, "w+").write(stderr.read())
    print "Extracted data into %s_build/plotnetcfg.out" % host

print "Completed."
sys.exit(0)
