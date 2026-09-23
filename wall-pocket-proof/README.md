# WallPocket physical calibration proofs

These are geometry proofs, not Verified calibration evidence. Create the managed
sessions from Tools → LEGO Fit Calibration → New: Stud Receiving Clutch →
New Calibration, choosing WallPocket brick depth and WallPocket plate depth in
turn. The application writes a companion session JSON using the selected
manufacturing workspace and records subsequent physical observations there.

| Candidate | Correction (mm) | Square opening (mm) |
| --- | ---: | ---: |
| 1 | -0.30 | 4.50 |
| 2 | -0.20 | 4.60 |
| 3 | -0.10 | 4.70 |
| 4 (nominal) | 0.00 | 4.80 |
| 5 | +0.10 | 4.90 |
| 6 | +0.20 | 5.00 |
| 7 | +0.30 | 5.10 |

The brick-depth strip has 8.0 mm pockets and is 84 × 12 × 10 mm. The
plate-depth strip has 1.6 mm pockets and is 84 × 12 × 3.6 mm. Both have a
2.0 mm floor, open-top pockets and a Candidate #1-side notch. Print flat-base
down at 100% scale. Record printer, material, nozzle, process, layer height,
orientation and slicer compensation. Test with genuine LEGO studs; record
every candidate's result separately for each depth. Do not transfer a result
between depths without separate verification.

The `3005` and `3024` files are *nominal PreparedMesh* examples for Bambu
inspection. Neither has a WallPocket correction. Export ManufacturingMesh only
after the matching depth's physical evidence is explicitly marked Verified
and its compatible Fit Profile is created or updated.
