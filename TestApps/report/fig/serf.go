func (s serfNode) serf(conn UDPConnection) {
	for true {
		msg := conn.Read()
		switch msg.Type {
		case PING:
			//@dump
			conn.WriteToUDP("ACK",msg.Sender)
			break
		case GOSSIP:
			s.Events = append(s.Events,msg.Event)
		}
		dinv.Dump("L1",msg.Type,msg.Sender,msg.Event,s.Events)
		timeout := s.CheckForTimeouts()
		switch timeout.Type {
		case PING:
			conn.WriteToUDP("PING",timeout.Node)
			break
		case GOSSIP:
			gossip(s.Events)
			break
		}
	}
}



