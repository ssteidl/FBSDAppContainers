#VLAN Bridging

The strategy for creating network is:

1. Create a single bridge at startup.
2. Create a single epair interface for vesseld at startup.
   We can name the interface vesseld or something to make it
   obvious.
3. Any new network that is requested by the vesseld client should
   be mapped to a vlan.  All jails with a network interface that
   is not inherited from the host should have a side of the epair
   that is connected to the vesseld bridge.  A vlan interface and
   ip address will be set for that vlan interface.
4. The vesseld epair will have a vlan address for each of the
   vlan created.  In that way it can serve DNS and other management.

As a convention, when using epair, we always connect the "b side" to
the bridge.  The "a side" is then used as a parent device for one
or more vlan devices.  