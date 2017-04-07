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
from mininet.topo import Topo

class BlueBridge( Topo ):
	"Simple topology example."

	def __init__( self ):
		"Create custom topo."

		# Initialize topology
		Topo.__init__( self )
		switch = self.addSwitch( 's1' )
		for hostNum in range(1, 42):
			# Add hosts and switches
			host = self.addHost( 'h' + str(hostNum) )
			self.addLink( host, switch )


topos = { 'BlueBridge': ( lambda: MyTopo() ) }

def run():
    #c = RemoteController('c', '0.0.0.0', 6633)
    net = Mininet(topo=BlueBridge(), host=CPULimitedHost, controller=None)
    #net.addController(c)
    net.start()

    CLI(net)
    net.stop()

# if the script is run directly (sudo custom/optical.py):
if __name__ == '__main__':
    setLogLevel('info')
    run()