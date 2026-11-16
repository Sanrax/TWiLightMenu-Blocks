# Native SD cache completion

Master `cf53f41ec` adds read-completion cache synchronization at the SDK SD
interface. The r27 hardware trace, parsed in `r27-hardware-analysis.json`,
contains correct physical RAM with stale cached data on all three captured
pages; two pages publish corrupted banner pixels. This establishes the cache
visibility failure, not final hardware validation of the correction.

`coherency-tests.log` records actual production functions replaying all three
cache/RAM fixtures and checking dirty neighbors for every cache-line alignment,
batched requests, read failures and preserved callbacks under ASan/UBSan.
Only cache operations and the SDK transfer are modeled; hardware timing and
DMA are not simulated. LeakSanitizer is disabled under the ptrace sandbox.
`menu-tests.log` covers existing DLDI layouts, relocation/publication, fades
and Wood palette behavior. `check-build.log` records all 18 NDS header/DLDI
checks, seven loader images and native SDK storage ownership after `make -j2 all`.

Normal package staging, a CIA rebuilt from the current booter and all three
platform archive integrity checks passed. Official nds-bootstrap 2.17.0 remains
unchanged. Normal linked browsers contain no icon recorder; the diagnostic
callback observes the SDK result before the cache sync. No new emulator test
was run: the reported native SD cache timing requires a device retest.
