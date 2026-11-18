# Device writes

The portable writer validates the selected destination extent, physical
sector size and transfer limit before issuing each batch. The initial
adapter uses 32-bit byte offsets and limits the destination extent to
the first 4 GiB. It submits a full `CMD_WRITE` through a DMA-checked
bounce buffer, then flushes with `CMD_UPDATE`. With verification on, it
issues `CMD_READ` on the same request and compares every byte against
the original data. The Settings preference defaults to off and persists
in `dolly.prefs`. Verification relies on the driver's cache behavior.

The native adapter borrows an open Exec request and a DMA buffer. Its
caller must hold exclusive access to that medium, revalidate identity
and extent, and establish driver support for write and flush before
calling it. A failed or interrupted write can leave part of the
destination changed. The caller must stop on failure and report the
affected range.

Host tests use a disposable file for the portable writer and a mocked
Exec request for the native adapter. The GUI Restore path still requires
destination and mounted-volume checks, destructive-operation confirmation
and integration with the saved verification preference.

## Restore preflight

Pressing Start in Restore now runs a read-only preflight. It opens the
selected flat RAW/HDF image and the destination for read access. The
classifier recognizes checksum-validated RDB layouts and boot-sector
signatures for DOS, PFS and SFS. Signatures identify a filesystem family
and variant; complete filesystem integrity checking is outside this step.
The comparison covers size, object kind, filesystem IDs, and for RDB
images the reported geometry and partition attributes. The first
discrepancy is displayed in a system requester.

**Allow Restore compatibility override** defaults to off. With it on,
an overridable metadata discrepancy can be acknowledged with Proceed
after a ten-second countdown. The final image-volume overlap check
still runs. A size mismatch, geometric bounds failure or unreadable
extent prevents approval. The size relationship is represented
separately so a future resize planner can handle it explicitly. Both
growth and shrinkage are deferred. Classic AmigaDOS file offsets also
limit this adapter to images smaller than 2 GiB.

The read-only preview closes both handles when it finishes. A future
write operation must retain its source and revalidate the destination
under exclusive access before submitting the first write.

## Destructive confirmation and transfer batches

The shared native requester gates dangerous choices, including Restore
compatibility overrides and transfers whose image volume cannot be
identified. It requires a click on Proceed after ten seconds. The
same-device performance notice is informational.

The portable Restore transfer engine reads one sector-aligned batch at
a time and submits it to an already approved target. Progress advances
after the batch succeeds. A read, write or verification failure halts
the engine and preserves the offset of the failed batch. The GUI
preflight does not invoke the engine.
