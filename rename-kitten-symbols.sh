#!/bin/bash

(grep -oE "[ck]itten[A-Za-z_0-9]+" src/kitten.h ;
	echo "new_learned_klause" ;
	echo "completely_backtrack_to_root_level" ) | while read x ; do
	sed -i 's/\b'$x'\b/cadi_'$x'/g' src/*.{h,c,hpp,cpp}
done

