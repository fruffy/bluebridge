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

class BlueBridge( Topo ):
	"Simple topology example."

	def __init__( self ):
		"Create custom topo."

		# Initialize topology
		Topo.__init__( self )
		switch = self.addSwitch( 's1' )
		hosts =  []
		for hostNum in range(1, 42):
			# Add hosts and switches
			host = self.addHost( 'h' + str(hostNum) )
			hosts.append(host)
			self.addLink( host, switch )

		for host in hosts:
			

topos = { 'BlueBridge': ( lambda: MyTopo() ) }

def run():
    #c = RemoteController('c', '0.0.0.0', 6633)
    net = TreeNet( depth=1, fanout=3 )
    #net.addController(c)
    net.start()

    CLI(net)
    net.start()
    net.stop()

# if the script is run directly (sudo custom/optical.py):
if __name__ == '__main__':
    setLogLevel('info')
    run()