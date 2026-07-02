# PreVABS Examples Report

- Generated: 2026-07-02 10:07:20 -04:00
- Executable: `C:\Users\tian50\work\dev\prevabs\build_msvc\Release\prevabs.exe`
- Mode: PreVABS + VABS (-e)
- Filter: `(none)`
- Per-example timeout: 60s
- Total time: 11.713s

## Summary

| Total | Passed | Failed | Errors | Timeouts |
| ---: | ---: | ---: | ---: | ---: |
| 14 | 13 | 1 | 0 | 0 |

## Cases

| Status | Example | Exit | VABS result | Time | Message | Log |
| --- | --- | ---: | --- | ---: | --- | --- |
| PASS | `ex_channel` | 0 | `channel.sg.K` | 0.344s |  | [log](example-logs/ex_channel.log) |
| PASS | `ex_box` | 0 | `box_cus.sg.K` | 0.471s |  | [log](example-logs/ex_box.log) |
| PASS | `ex_box_nested_webs` | 0 | `box.sg.K` | 0.915s |  | [log](example-logs/ex_box_nested_webs.log) |
| PASS | `ex_tube` | 0 | `tube.sg.K` | 1.045s |  | [log](example-logs/ex_tube.log) |
| PASS | `ex_pipe` | 0 | `pipe.sg.K` | 0.428s |  | [log](example-logs/ex_pipe.log) |
| PASS | `ex_ibeam` | 0 | `i_web.sg.K` | 0.371s |  | [log](example-logs/ex_ibeam.log) |
| PASS | `ex_box_ii_quad` | 0 | `box_II.sg.K` | 0.389s |  | [log](example-logs/ex_box_ii_quad.log) |
| PASS | `ex_airfoil` | 0 | `mh104.sg.K` | 1.488s |  | [log](example-logs/ex_airfoil.log) |
| PASS | `ex_airfoil_cspar` | 0 | `cs_main.sg.K` | 0.661s |  | [log](example-logs/ex_airfoil_cspar.log) |
| PASS | `ex_airfoil_dspar` | 0 | `cs_main.sg.K` | 0.461s |  | [log](example-logs/ex_airfoil_dspar.log) |
| PASS | `ex_airfoil_r` | 0 | `mh104.sg.ELE` | 3.347s |  | [log](example-logs/ex_airfoil_r.log) |
| FAIL | `ex_uh60a` | 1 | `uh60a_section.sg.K` | 0.106s | fatal exception: homogenization failed: layered offset[main_spar]: failed to tile closed layer 1 after the shared DCEL was already mutated; aborting (a legacy fallback here would corrupt the cross-section mesh) | [log](example-logs/ex_uh60a.log) |
| PASS | `ex_uh60a_f` | 0 | `uh60a.sg.fi` | 0.784s |  | [log](example-logs/ex_uh60a_f.log) |
| PASS | `ex_airfoil_multi-cells` | 0 | `mh104_9webs.sg.K` | 0.744s |  | [log](example-logs/ex_airfoil_multi-cells.log) |
