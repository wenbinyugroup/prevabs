(input-material-failure-criterion)=
# Failure criterion

## Isotropic

For isotropic materials, the following five failure criteria are available, followed by the strength constants needed:

1. **Max principal stress**. Strength constants (2):

   - 1 tensile strength ($X$)
   - 1 compressive strength ($X'$).

2. **Max principal strain**. Strength constants (2):

   - 1 tensile strength ($X_{\varepsilon}$)
   - 1 compressive strength ($X'_{\varepsilon}$).

3. **Max shear stress** or **Tresca**. Strength constant (1):

   - 1 shear strength ($S$).

4. **Max shear strain**. Strength constant (1):

   - 1 shear strength ($S_{\varepsilon}$)

5. **Mises**. Strength constant (1):

   - 1 strength ($X$).



Different failure criterion will use different strength properties.
This is summarized in the table below.

| Failure criterion | x/xt/t1 | y/yt/t2 | z/zt/t3 | xc/c1 | yc/c2 | zc/c3 | s/s12 | t/s13 | r/s23 |
|------------------|---------|---------|---------|-------|-------|-------|-------|-------|-------|
| Max principal stress | $X$ | | | $X'$ | | | | | |
| Max principal strain | $X_{\varepsilon}$ | | | $X'_{\varepsilon}$ | | | | | |
| Max shear stress | | | | | | | $S$ | | |
| Max shear strain | | | | | | | $S_{\varepsilon}$ | | |
| Mises | $X$ | | | | | | | | |

More details can be found in the VABS users manual.

**Specification**

- `<failure_criterion>` - Name or ID of the failure criterion used.

  - `1` or `max principal stress`
  - `2` or `max principal strain`
  - `3` or `max shear stress` or `tresca`
  - `4` or `max shear strain`
  - `5` or `mises`


---

## Not isotropic

For other type materials (transversely isotropic, orthotropic / engineering, anisotropic), the following five failure criteria are available:

1. **Max stress**. Strength constants (9):

   - 3 tensile strengths in three directions ($X, Y, Z$)
   - 3 compressive strengths in three directions ($X', Y', Z'$)
   - 3 shear strengths in three principal planes ($S, T, R$)

2. **Max strain**. Strength constants (9):

   - 3 tensile strengths in three directions ($X_{\varepsilon}, Y_{\varepsilon}, Z_{\varepsilon}$)
   - 3 compressive strengths in three directions ($X'_{\varepsilon}, Y'_{\varepsilon}, Z'_{\varepsilon}$)
   - 3 shear strengths in three principal planes ($S_{\varepsilon}, T_{\varepsilon}, R_{\varepsilon}$)

3. **Tsai-Hill**. Strength constants (6):

   - 3 normal strengths in three directions ($X, Y, Z$)
   - 3 shear shear strengths in three principal planes ($S, T, R$).

4. **Tsai-Wu**. Strength constants (9):

   - 3 tensile strengths in three directions ($X, Y, Z$)
   - 3 compressive strengths in three directions ($X', Y', Z'$)
   - 3 shear strengths in three principal planes ($S, T, R$)

5. **Hashin**. Strength constants (6):

   - 2 tensile strengths in two directions ($X, Y$)
   - 2 compressive strengths in two directions ($X', Y'$)
   - 2 shear strengths in two principal planes ($S, R$)

Different failure criterion will use different strength properties.
This is summarized in the table below.

| Failure criterion | x/xt/t1 | y/yt/t2 | z/zt/t3 | xc/c1 | yc/c2 | zc/c3 | s/s12 | t/s13 | r/s23 |
|------------------|---------|---------|---------|-------|-------|-------|-------|-------|-------|
| Max stress | $X$ | $Y$ | $Z$ | $X'$ | $Y'$ | $Z'$ | $S$ | $T$ | $R$ |
| Max strain | $X_{\varepsilon}$ | $Y_{\varepsilon}$ | $Z_{\varepsilon}$ | $X'_{\varepsilon}$ | $Y'_{\varepsilon}$ | $Z'_{\varepsilon}$ | $S_{\varepsilon}$ | $T_{\varepsilon}$ | $R_{\varepsilon}$ |
| Tsai-Hill | $X$ | $Y$ | $Z$ | | | | $S$ | $T$ | $R$ |
| Tsai-Wu | $X$ | $Y$ | $Z$ | $X'$ | $Y'$ | $Z'$ | $S$ | $T$ | $R$ |
| Hashin | $X$ | $Y$ | | $X'$ | $Y'$ | | $S$ | | $R$ |

More details can be found in the VABS users manual.

**Specification**

- `<failure_criterion>` - Name or ID of the failure criterion used.

  - `1` or `max stress`
  - `2` or `max strain`
  - `3` or `tsai-hill`
  - `4` or `tsai-wu`
  - `5` or `hashin`

