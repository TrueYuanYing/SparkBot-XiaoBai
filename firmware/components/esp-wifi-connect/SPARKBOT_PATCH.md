# SparkBot local Wi-Fi override

Upstream: 78/esp-wifi-connect 2.6.2, https://github.com/78/esp-wifi-connect (MIT).
Copied from the installed managed component. Local changes:
- Both retry timers now wait 10,000,000 microseconds (10 seconds), not 10,000 microseconds (10 ms).
- The old loop spent nearly all time scanning when the upstream hotspot disappeared. Real SparkBot AP connection timed out under that condition.

The main component manifest uses override_path so clean builds retain the fix.

0.8: An atomic three-second activity deadline defers background scans during local controls, snapshots, page loads and active voyage polls. A new SoftAP peer also gets a short discovery grace period; entering local activity cancels an already-running offline scan. Stops/idle restore automatic network search. This deliberately delays reconnecting to the cloud while local activity continues. No change to motor stop leases.
