# Klipper Motion Control Output

KlipSlice emits `SET_VELOCITY_LIMIT` for Klipper acceleration changes. When the
acceleration-to-deceleration option is enabled, it includes
`MINIMUM_CRUISE_RATIO`, replacing the removed `ACCEL_TO_DECEL` parameter. The
same conversion applies to combined acceleration/corner commands and Type1
prime-tower output.

## Profile Compatibility

The stored keys remain `accel_to_decel_enable` and `accel_to_decel_factor`, so
existing profiles and projects remain readable. The percentage converts to a
ratio using `1 - factor / 100`: 50% becomes 0.5, and 100% becomes 0.0, disabling
minimum cruising distance. This follows Klipper's documented migration from
pseudo acceleration to minimum cruise ratio. It does not change the configured
acceleration value or introduce a firmware ceiling.

Output requires firmware that supports `MINIMUM_CRUISE_RATIO`. Klipper v0.13.0
and the current pinned integration snapshot support it. Users on older firmware
without that interface must upgrade or disable `accel_to_decel_enable`; generated
acceleration changes then omit the cruise parameter. There is no automatic
firmware detection in the offline slicing pipeline. Custom start/end/tool-change
G-code is preserved and must be migrated by its author.

## Verification

`tests/fff_print/test_gcodewriter.cpp` checks enabled/disabled output, percentage
boundaries, combined commands and actual Type1 prime-tower generation. The
virtual fixture Dockerfile accepts an explicit repository/ref/commit triple and
rejects mismatched source hashes. FLUXRAKER's companion integration runner selects
pinned legacy/current targets and audits removed parameters before printing.
A successful slice or configuration check does not establish successful Klippy
execution; both integration targets must report an actual pass.
