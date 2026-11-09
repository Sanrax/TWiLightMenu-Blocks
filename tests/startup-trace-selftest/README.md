# Startup watchdog self-test

Build with `make -C tests/startup-trace-selftest`. Run `watchdog_check.nds`
in DS mode. No SD image or console files are required. After about ten seconds
it must display `Stopped during: watchdog self-test` and
`simulated SD wait / sector 123, count 1`. This exercises the actual IRQ-time
console path and its 4 KiB stack; it never issues a storage request.
