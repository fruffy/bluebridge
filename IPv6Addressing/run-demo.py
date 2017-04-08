"""Custom topology example

One switch and 16 hosts:

   host --- switch --- switch --- host

Adding the 'topos' dict with a key/value pair to generate our newly defined
topology enables one to pass in '--topo=mytopo' from the command line.
"""
from mininet.node import CPULimitedHost
from mininet.topo import Topo
from mininet.net import Mininet
from mininet.log import setLogLevel, info
from mininet.node import RemoteController
from mininet.cli import CLI
from mininet.link import TCLink
from mininet.topolib import TreeNet


class BlueBridge(Topo):
    "Simple topology example."

    def __init__(self):
        "Create custom topo."

        # Initialize topology
        Topo.__init__(self)
        switch = self.addSwitch('s1')
        for hostNum in range(1, 42):
            # Add hosts and switches
            host = self.addHost('h' + str(hostNum))
            host.setIP(self, "::1101:0:0:0:0")
            self.addLink(host, switch)


topos = {'BlueBridge': (lambda: BlueBridge())}


def run():
    # c = RemoteController('c', '0.0.0.0', 6633)
    net = TreeNet(depth=1, fanout=3)
    # net.addController(c)
    net.start()

    hosts = net.hosts
    hostNum = 1
    for host in hosts:
        print host
        host.cmd('ifconfig h' + str(hostNum) + '-eth0 inet6 add 0:0:010' + str(hostNum) + '::/48')
        print host.cmd('ip -6 route add local 0:0:0100::/40  dev h' + str(hostNum) + '-eth0')
        hostNum += 1

    net.startTerms()
    for host in hosts:
        print host
        # host.cmd('./messaging/bin/server')
    CLI(net)

    net.stop()
# if the script is run directly (sudo custom/optical.py):


if __name__ == '__main__':
    setLogLevel('info')
    run()
