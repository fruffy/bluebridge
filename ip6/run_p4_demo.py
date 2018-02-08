#!/usr/bin/env python2

# Copyright 2013-present Barefoot Networks, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

import os

from mininet.net import Mininet
from mininet.topo import Topo
from mininet.log import setLogLevel, info
from mininet.cli import CLI
from functools import partial
from mininet.term import makeTerm
from subprocess import Popen, PIPE
from mininet.node import Host
from p4_mininet.p4_mininet import P4Switch, P4Host

from time import sleep

HOSTS = 3


class BlueBridgeTopo(Topo):

    def __init__(self, sw_path, json_path, thrift_port, pcap_dump, n, **opts):
        "Create custom topo."

        # Initialize topology
        Topo.__init__(self, **opts)

        switch = self.addSwitch('s1',
                                sw_path=sw_path,
                                json_path=json_path,
                                thrift_port=thrift_port,
                                pcap_dump=pcap_dump)
        # Create a network topology of a single switch
        # connected to three nodes.
        # +------s1------+
        # |      |       |
        # h1     h2      h3
        for h in range(n):
            host = self.addHost('h%d' % (h + 1),
                                ip="10.0.0.%d/24" % (h + 1),
                                mac='00:04:00:00:00:%02x' % h)
            self.addLink(host, switch)


topos = {'BlueBridge': (lambda: BlueBridgeTopo())}


def configureHosts(net):
    hostNum = 1
    hosts = net.hosts
    for host in hosts:
        print(host)

        # Insert host configuration
        configString = "\"INTERFACE=h" + \
            str(hostNum) + \
            "-eth0\n\HOSTS=0:0:102::,0:0:103::\n\SERVERPORT=5000\n\SRCPORT=0\n\SRCADDR=0:0:01" + \
            '{0:02x}'.format(hostNum) + "::\n\DEBUG=0\" > ./tmp/config/distMem.cnf"
        host.cmdPrint('echo ' + configString)

        # Configure the interface and respective routing
        host.cmdPrint('ip address change dev h' + str(hostNum) +
                      '-eth0 scope global 0:0:01' + '{0:02x}'.format(hostNum) + '::/48')
        host.cmdPrint('ip -6 route add local 0:0:0100::/40  dev h' +
                      str(hostNum) + '-eth0')
        # host.cmdPrint('ip -6 route add local 0:0:01' +
        #               '{0:02x}'.format(hostNum) + '::/48 dev lo')
        # Gotta get dem jumbo frames
        host.cmdPrint('ifconfig h' + str(hostNum) + '-eth0 mtu 9000')
        if hostNum != 1:
                # Run the server
            host.cmdPrint('xterm  -T \"server' + str(hostNum) +
                          '\" -e \"./applications/bin/server -c tmp/config/distMem.cnf; bash\" &')
            #host.cmdPrint('./applications/bin/server tmp/config/distMem.cnf &')
        hostNum += 1


def clean():
    ''' Clean any the running instances of POX '''
    Popen("killall xterm", stdout=PIPE, shell=True)
    # p = Popen("ps aux | grep 'xterm' | awk '{print $2}'",
    #           stdout=PIPE, shell=True)
    # p.wait()
    # procs = (p.communicate()[0]).split('\n')
    # for pid in procs:
    #     try:
    #         pid = int(pid)
    #         Popen('kill %d' % pid, shell=True).wait()
    #     except:
    #         pass

# "sudo python 1sw_demo.py --behavioral-exe ../targets/l2_switch/l2_switch --json ../targets/l2_switch/l2_switch.json"


def main():

    privateDirs = [('./tmp/config', '/tmp/%(name)s/var/config')]
    # c = RemoteController('c', '0.0.0.0', 6633)

    host = partial(Host,
                   privateDirs=privateDirs)
    behavioral = 'p4_switch/l2_switch/l2_switch'
    json = 'p4_switch/l2_switch/l2_switch.json'
    thrift_port = 9090
    topo = BlueBridgeTopo(behavioral,
                          json,
                          thrift_port,
                          False,
                          HOSTS)
    net = Mininet(topo=topo,
                  host=host,
                  switch=P4Switch,
                  controller=None)
    net.start()

    configureHosts(net)
    makeTerm(net.hosts[0])

    # Our current "switch"
    hostNum = HOSTS  # TODO: change back to 3
    i = 1
    while i <= hostNum:
        # Routing entries per port
        # Gotta get dem jumbo frames
        os.system('ifconfig s1-eth' + str(i) + ' mtu 9000')
        i += 1
    os.system('p4_switch/l2_switch/runtime_CLI < p4_switch/l2_switch/commands.txt')
    CLI(net)
    clean()
    net.stop()


if __name__ == '__main__':
    setLogLevel('info')
    main()
