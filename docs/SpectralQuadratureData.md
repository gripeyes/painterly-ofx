# Spectral quadrature data attribution

The 21 observer/D65 rows in `src/core/ColorInteraction.cpp` are the 380–780 nm, 20 nm subsampling of CIE's public datasets, without smoothing. Trapezoidal integration and reference-white normalization are prototype approximations, not new observer data.

- CIE 2019, *Colour-matching functions of CIE 1931 standard colorimetric observer*, DOI [10.25039/CIE.DS.xvudnb9b](https://doi.org/10.25039/CIE.DS.xvudnb9b). [Source CSV](https://files.cie.co.at/Publications-datasets/CIE_xyz_1931_2deg.csv), [metadata](https://files.cie.co.at/Publications-datasets/CIE_xyz_1931_2deg.csv_metadata.json).
- CIE 2019, *CIE standard illuminant D65*, DOI [10.25039/CIE.DS.hjfjmt59](https://doi.org/10.25039/CIE.DS.hjfjmt59), [source CSV](https://files.cie.co.at/Publications-datasets/CIE_std_illum_D65.csv), [metadata](https://files.cie.co.at/Publications-datasets/CIE_std_illum_D65.csv_metadata_v2.json).

These derived data rows are licensed [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/), as specified by the CIE metadata. This attribution and share-alike apply to the derived data, not a claim of ownership over CIE's observations. Source algorithms are independently implemented from cited equations; no Mixbox/rgb2spec code or tables are copied.
