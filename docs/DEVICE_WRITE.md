# Device writes — first implementation step

The portable writer accepts an explicit byte extent, physical sector size
and transfer limit. Each call validates the whole destination extent and
the batch before it invokes the transport. The first implementation
restricts the target to the first 4 GiB. A full write must be followed
by a successful flush and an exact byte-for-byte readback. A short write,
a flush failure and a mismatched readback are distinct outcomes.

The adapter must keep exclusive access to the validated medium and
implement the three callbacks against that same medium. The first
tests use a disposable file to model a partition, verify adjacent
sectors and inject transport failures. The native adapter and the GUI
restore path have yet to be connected. Before exposing native writes,
the caller must validate destination identity and mount state, present
the target range and obtain destructive-operation confirmation. A
failed or interrupted write can leave part of the destination changed.
