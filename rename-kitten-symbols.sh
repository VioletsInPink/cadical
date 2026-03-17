#!/bin/bash

grep -oE "[ck]itten[A-Za-z_0-9]+" src/kitten.h | while read x ; do
	sed -i 's/\b'$x'\b/cadi'$x'/g' src/*.{h,c,hpp,cpp}
done

