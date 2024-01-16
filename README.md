# Introduction

`GrADS2gt` is an extension of GrADS Version 2.x (GrADS2).

The differences from the original GrADS2 are as follows:
1. Support GTOOL3 format (read)
2. Support some kinds of calendars: noleap, 360_day, julian
3. Add a command `cd` (change directory)

# Install
You need to install [libgtool3](https://github.com/kjoti/libgtool3)
before `GrADS2gt` installation
if you want to add GTOOL3 format support.


# Examples

## Command `gtopen`
To access variables in a GTOOL3-formatted file,
you must first open a GTOOL3 file with `gtopen`.
Then its file name can be treated as the variable name.
After that, the variable can be handled in the same way as original GrADS2.

```
ga-> gtopen T2                                # Open a file named `T2`.
The global calendar is set to noleap.         # Guess a calendar automatically.
Data file T2 is open as file 1
LON set to 0 360
LAT set to -88.9277 88.9277
LEV set to 1 1
Time values set: 1699:1:1:0 1699:1:1:0
E set to 1 1
ga-> d T2                # `T2` can be used as a variable name.
ga-> d t2                # This also works (variable name is case-insensitive).
ga-> d t2(t=2)           # View data (No.2)
```

```
ga-> reinit
All files closed; all defined objects released;
All GrADS attributes have been reinitialized
ga-> gtopen u10
The global calendar is set to noleap.
Data file u10 is open as file 1
LON set to 0 360
LAT set to -88.9277 88.9277
LEV set to 1 1
Time values set: 1699:1:1:0 1699:1:1:0
E set to 1 1
ga-> gtopen v10
The global calendar is set to noleap.
Data file v10 is open as file 2
ga-> q files
File 1 : MIROC6
  Descriptor: u10
  Binary: u10
File 2 : MIROC6
  Descriptor: v10
  Binary: v10
ga-> d sqrt(u10 * u10 + v10.2 * v10.2) # v10 is in File #2.
Contouring: 0 to 11 interval 1
ga-> d sqrt(u10 * u10 + v10 * v10)  # File number (#2) can be omitted.
v10 is found in File #2             # Look up the name (v10) automatically.
v10 is found in File #2
Contouring: 0 to 11 interval 1
```

## Command `vgtopen`

`vgtopen` is another version of `gtopen`.
`vgtopen` opens a new file,
but its file number is the same as the file previously opened by `gtopen`.
As a result, these variables appear to be in the same dataset.

```
ga-> reinit
ga-> gtopen u10    # At first, use gtopen.
ga-> vgtopen v10   # Next, use vgtopen.
ga-> vgtopen olr   # ditto
ga-> vgtopen osr   # ditto
ga-> q files
File 1 : MIROC6    # Only File #1 exists.
  Descriptor: u10
  Binary: u10
ga-> q file 1
File 1 : MIROC6
  Descriptor: u10
  Binary: u10
  Type = Gridded
  Xsize = 256  Ysize = 128  Zsize = 1  Tsize = 12  Esize = 1
  Number of Variables = 4                    # Four variables are in File #1.
     u10  0    10m zonal wind [m/s]
     v10  0    10m meridional wind [m/s]
     olr  0    top longwave [W/m**2]
     osr  0    top shortwave [W/m**2]
ga-> d sqrt(u10 * u10 + v10 * v10)           # Both u10 and v10 are in File #1.
ga-> d olr + osr                             # ditto.
```

## `as` clause
By default, the variable name is set from the file name,
but user can specify any name with `as` clause.
`as` clause can be used for both `gtopen` and `vgtopen`.

```
ga-> gtopen precipitation as pr                      # Use pr as variable name.
The global calendar is set to noleap.
Data file precipitation is open as file 1
LON set to 0 360
LAT set to -88.9277 88.9277
LEV set to 1 1
Time values set: 1699:1:1:0 1699:1:1:0
E set to 1 1
ga-> q files
File 1 : MIROC6
  Descriptor: pr
  Binary: precipitation
ga-> q file
File 1 : MIROC6
  Descriptor: pr
  Binary: precipitation
  Type = Gridded
  Xsize = 256  Ysize = 128  Zsize = 1  Tsize = 12  Esize = 1
  Number of Variables = 1
     pr  0    precipitation [kg/m**2/s]
ga-> d pr                            # pr can be used instead of precipitation.
```
