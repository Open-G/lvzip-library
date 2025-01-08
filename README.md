# openg-lvzip

December 14, 2024 Rolf Kalbermatter

This is the latest and active development version for the OpenG ZIP library.

The new version 5.x attempts to maintain the old API but implement lots of changes under the hood by using library private
path handling routines. These path routines are able to work with long file names and also full Unicode support.
The whole thing is fairly well working under Windows and Linux and possibly may work with little effort on the Mac OS
side of things.

While the Windows and Linux version are under active maintenance, other platforms like MacOS X, LabVIEW Pharlap ETS and
LabVIEW VxWorks are not very likely to be ever released, unless someone really is willing to step in with some effort
of their own.

Work on support for the NI Linux RT platforms is underway but not yet finalized.
