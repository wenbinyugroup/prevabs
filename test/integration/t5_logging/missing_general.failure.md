# PreVABS build failure report

- Input: `C:/Users/tian50/work/dev/prevabs/test/integration/t5_logging/missing_general.xml`
- Case: `missing_general`
- Time: 2026-07-03 17:35:44
- Failure: fatal exception: homogenization failed: Missing required XML element <general>
- Catch context: initialize model

## What failed

PreVABS stopped while building the cross-section. See the fatal message and nearby diagnostic log lines below.

## Diagnostic evidence

From `C:/Users/tian50/work/dev/prevabs/test/integration/t5_logging/missing_general.log`:

- xx  fatal exception: homogenization failed: Missing required XML element <general>

From `C:/Users/tian50/work/dev/prevabs/test/integration/t5_logging/missing_general.debug.log`:

- [error]     fatal exception: homogenization failed: Missing required XML element <general>  main.cpp:logFatalWithProgress:723

## Related files

- Log: `C:/Users/tian50/work/dev/prevabs/test/integration/t5_logging/missing_general.log`
- Debug log: `C:/Users/tian50/work/dev/prevabs/test/integration/t5_logging/missing_general.debug.log`
- DCEL dump: `C:/Users/tian50/work/dev/prevabs/test/integration/t5_logging/missing_general.dcel_dump.txt`

## User action

The build was stopped instead of silently dropping local faces or elements. Inspect the reported segment/offset region and adjust the cross-section geometry or layup so the local laminate faces remain meshable and connected.
