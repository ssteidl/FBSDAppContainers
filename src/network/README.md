# Networking Features
The following networking features should be supported

## Topologies
* Jail inheriting the host network
* Internal networks with bridge, vlan, epair interfaces
* Nat and rdr networks for exposing internal networks to the
  outside world
* No networking

## Management
* VLAN per internal network
* Auto generated internal ipv4 address
* Internal network DNS lookup for A records.  Hopefully txt in the future.
  Does it make sense to use DHCP here?

# cli design

* Run with inherited network stack.  This is the default and doesn't
  require any flags.  Still must support internal DNS lookup.
`vessel run devel:0 -- bash`

* Create an internal network
`vessel create-network --name="shane-net" --dns="postgres-dev:192.168.3.2:<ttl>"

* Run with internal network that is nat'd for outside world
  access.  Network vlan is created if it doesn't yet exist
`vessel run --network "shane-net" devel:0 -- bash`

* Run with internal network with port 80 of host mapped to
  port 8080 of jail.  The internal network used will be the
  default. Jail will also be nat'd
`vessel run -p 80:8080 devel:0 -- bash`

* Run with no internal network so internal DNS will not be available
`vessel run --no-internal-net devel:0 -- bash`

* Run without any network besides localhost
`vessel run --no-net devel:0 --bash`