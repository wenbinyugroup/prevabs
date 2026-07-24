# Example Variants Report

- Generated: 2026-07-24 16:20:59 -04:00
- Executable: `C:\Users\tian50\work\dev\prevabs\build_msvc\Release\prevabs.exe`
- Filter: `(none)`
- Per-variant timeout: 60s
- Total time: 19.168s

## Summary

| Total | Passed | Failed | Errors | Timeouts |
| ---: | ---: | ---: | ---: | ---: |
| 28 | 25 | 3 | 0 | 0 |

## Variants

| Status | Variant | Base | Exit | Time | Message | Log |
| --- | --- | --- | ---: | ---: | --- | --- |
| PASS | `ex_airfoil_cspar_v1` | `ex_airfoil_cspar` | 0 | 0.83s |  | [log](ex_airfoil_cspar_v1/run.log) |
| PASS | `ex_airfoil_cspar_v2` | `ex_airfoil_cspar` | 0 | 0.763s |  | [log](ex_airfoil_cspar_v2/run.log) |
| FAIL | `ex_airfoil_dspar_v1` | `ex_airfoil_dspar` | 1 | 0.102s | [error]     fatal exception: homogenization failed: layered offset[sgm_blk_1]: failed to split layer 3 into cells after the shared DCEL was already mutated; aborting (a legacy fallback here would corrupt the cross-section mesh) | [log](ex_airfoil_dspar_v1/run.log) |
| FAIL | `ex_airfoil_dspar_v2` | `ex_airfoil_dspar` | 1 | 0.112s | [error]     fatal exception: homogenization failed: layered offset[sgm_blk_1]: failed to split layer 2 into cells after the shared DCEL was already mutated; aborting (a legacy fallback here would corrupt the cross-section mesh) | [log](ex_airfoil_dspar_v2/run.log) |
| PASS | `ex_airfoil_multicells_v1` | `ex_airfoil_multi-cells` | 0 | 0.851s |  | [log](ex_airfoil_multicells_v1/run.log) |
| PASS | `ex_airfoil_multicells_v2` | `ex_airfoil_multi-cells` | 0 | 0.743s |  | [log](ex_airfoil_multicells_v2/run.log) |
| PASS | `ex_airfoil_r_v1` | `ex_airfoil_r` | 0 | 0.93s |  | [log](ex_airfoil_r_v1/run.log) |
| PASS | `ex_airfoil_r_v2` | `ex_airfoil_r` | 0 | 0.928s |  | [log](ex_airfoil_r_v2/run.log) |
| PASS | `ex_airfoil_v1` | `ex_airfoil` | 0 | 1.583s |  | [log](ex_airfoil_v1/run.log) |
| PASS | `ex_airfoil_v2` | `ex_airfoil` | 0 | 1.516s |  | [log](ex_airfoil_v2/run.log) |
| PASS | `ex_box_ii_quad_v1` | `ex_box_ii_quad` | 0 | 0.401s |  | [log](ex_box_ii_quad_v1/run.log) |
| PASS | `ex_box_ii_quad_v2` | `ex_box_ii_quad` | 0 | 0.355s |  | [log](ex_box_ii_quad_v2/run.log) |
| FAIL | `ex_box_nested_webs_v1` | `ex_box_nested_webs` | 1 | 0.096s | [error]     fatal exception: homogenization failed: layered offset[sgm_1]: failed to tile closed layer 2 after the shared DCEL was already mutated; aborting (a legacy fallback here would corrupt the cross-section mesh) | [log](ex_box_nested_webs_v1/run.log) |
| PASS | `ex_box_nested_webs_v2` | `ex_box_nested_webs` | 0 | 1.039s |  | [log](ex_box_nested_webs_v2/run.log) |
| PASS | `ex_box_v1` | `ex_box` | 0 | 0.537s |  | [log](ex_box_v1/run.log) |
| PASS | `ex_box_v2` | `ex_box` | 0 | 0.417s |  | [log](ex_box_v2/run.log) |
| PASS | `ex_channel_v1` | `ex_channel` | 0 | 0.356s |  | [log](ex_channel_v1/run.log) |
| PASS | `ex_channel_v2` | `ex_channel` | 0 | 0.375s |  | [log](ex_channel_v2/run.log) |
| PASS | `ex_ibeam_v1` | `ex_ibeam` | 0 | 0.426s |  | [log](ex_ibeam_v1/run.log) |
| PASS | `ex_ibeam_v2` | `ex_ibeam` | 0 | 0.399s |  | [log](ex_ibeam_v2/run.log) |
| PASS | `ex_pipe_v1` | `ex_pipe` | 0 | 0.447s |  | [log](ex_pipe_v1/run.log) |
| PASS | `ex_pipe_v2` | `ex_pipe` | 0 | 0.448s |  | [log](ex_pipe_v2/run.log) |
| PASS | `ex_tube_v1` | `ex_tube` | 0 | 0.547s |  | [log](ex_tube_v1/run.log) |
| PASS | `ex_tube_v2` | `ex_tube` | 0 | 1.309s |  | [log](ex_tube_v2/run.log) |
| PASS | `ex_uh60a_f_v1` | `ex_uh60a_f` | 0 | 0.772s |  | [log](ex_uh60a_f_v1/run.log) |
| PASS | `ex_uh60a_f_v2` | `ex_uh60a_f` | 0 | 0.758s |  | [log](ex_uh60a_f_v2/run.log) |
| PASS | `ex_uh60a_v1` | `ex_uh60a` | 0 | 0.9s |  | [log](ex_uh60a_v1/run.log) |
| PASS | `ex_uh60a_v2` | `ex_uh60a` | 0 | 1.103s |  | [log](ex_uh60a_v2/run.log) |
