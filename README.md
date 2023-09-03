# openg-lvzip

September 1, 2023 Rolf Kalbermatter

This is the latest and active development version for the OpenG ZIP library.
This new branch attempts to maintain the old API but implement lots of changes under the hood by using library private
path handling routines. These path routines will be able to work with long file names and also full Unicode support.
The whole thing is fairly well working under Windows but the Linux and possibly Mac OS side of things has not seen any
testing so far.
While the Linux version is quite likely to be tackled, other platforms including MacOS X, LabVIEW Pharlap ETS and
LabVIEW VxWorks are not very likely to be ever released, unless someone really is willing to step in with some effort
of their own.
