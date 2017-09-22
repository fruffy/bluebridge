sudo ethtool -N enp66s0f0 rx-flow-hash udp6 sdfn
sudo ethtool -C enp66s0f0 rx-usecs 0
sudo ethtool --offload  enp66s0f0  rx on tx on
sudo ethtool -A enp66s0f0 autoneg off rx off tx off
sudo ip6tables -t raw -I PREROUTING 1 --src 0:0:100::/40 -j NOTRACK
sudo ip6tables -I INPUT 1 --src 0:0:100::/40 -j ACCEPT