package ifneeded appc::bsd 1.0.0 [list source [file join $dir bsd.tcl]]
package ifneeded appc::build 1.0.0 [list source [file join $dir build.tcl]]
package ifneeded appcd::client 1.0.0 [list source [file join $dir appcd_client.tcl]]
package ifneeded appc::definition_file 1.0.0 [list source [file join $dir build.tcl]]
package ifneeded appc::dns 1.0.0 [list source [file join $dir dns dns.tcl]]
package ifneeded appc::env 1.0.0 [list source [file join $dir environment.tcl]]
package ifneeded appc::jail 1.0.0 [list source [file join $dir jail.tcl]]
package ifneeded appc::native 1.0.0 [list load [file join $dir libappctcl.so]]
package ifneeded appc::network 1.0.0 [list source [file join $dir network network.tcl]]
package ifneeded appc::pty_shell 1.0.0 [list source [file join $dir pty_shell.tcl]]
package ifneeded appc::publish 1.0.0 [list source [file join $dir publish.tcl]]
package ifneeded appc::pull 1.0.0 [list source [file join $dir pull.tcl]]
package ifneeded appc::run 1.0.0 [list source [file join $dir run.tcl]]
package ifneeded appc::zfs 1.0.0 [list source [file join $dir zfs.tcl]]

