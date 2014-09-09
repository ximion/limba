# CMakeLists for BinReloc

set(BINRELOC_FILES basic.c
		binreloc.m4
		fdbr-test.c
		fdbr.c
		generate.pl
		glib.c
		normal.c
		README
)

set(BINRELOC_JAVA java/BinReloc.java)
set(BINRELOC_MONO mono/BinReloc.cs
		mono/Readme.txt
)
set(BINRELOC_PYTHON python/reloc.py
		python/reloc.txt
		python/testreloc.py
)

set(BINRELOC_DOC "")

# TODO: Generate & install documentation

install(FILES ${BINRELOC_FILES} DESTINATION share/listaller/binreloc COMPONENT binreloc)

install(FILES ${BINRELOC_JAVA} DESTINATION share/listaller/binreloc/java COMPONENT binreloc)
install(FILES ${BINRELOC_MONO} DESTINATION share/listaller/binreloc/mono COMPONENT binreloc)
install(FILES ${BINRELOC_PYTHON} DESTINATION share/listaller/binreloc/python COMPONENT binreloc)
