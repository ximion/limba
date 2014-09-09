BinReloc is a utility that allows your program to be "location independent".

Traditionally, UNIX programs have hard coded paths to e.g. data files which are set during
compilation which means that a program cannot be moved to a new folder after it has been compiled.

With BinReloc, you can locate your external files using paths that are relative to your program.

So if you have your program installed in
	/usr/bin/yourprogram
and your data is in
	/usr/share/yourprogram/
you would, with binreloc, do:
  	string myDataFolder = Autopackage.BinReloc.DataDir;
instead of
    string myDataFolder = "/usr/share/yourprogram";

This means that the program can be put anywhere on the users system (including the $HOME directory) as long
as the relative folder hierarchy is preserved.

For more information about the problem and how we solve it, see http://www.autopackage.org/docs/binreloc/

== Difference between BinReloc C and C# versions ==
Note that BinReloc for .NET/Mono is not using the same technical solution as for c/c++ apps. .NET supports
binary relocation in the framework so the c# version of BinReloc is just utilizing what is already available
and putting it all inside an API that is similar to our C-version.

== Usage ==
Just copy BinReloc.cs to your C# project and then use the static methods inside it to get folder information.
If you have multiple projects (i.e. one .exe and several .dll's), you can put BinReloc.cs in just one of them
and then have the other projects depend on this one project.

Then you just call the static properties inside Autopackage.BinReloc whenever you need to locate a folder:

	Image myImage = new Bitmap(Path.Combine (BinReloc.DataDir, "myImage.png"));

== Technical details / Troubleshooting ==
The technique used is to ask .NET where the calling assembly is located, and then build path information from
that. If BinReloc.cs is inside your single project, you only access BinReloc members from your main program, or
all DLLs that access BinReloc members are in the same folder then everything will work just fine.

A problem could arise if you access BinReloc members from a third assembly, which is NOT in the same folder as
the main program.

The following setup would not work, if
	myprogram calls function in mylib.dll which in turn calls a BinReloc member inside myotherlib.

	/usr/libexec/myprogram/mylib.dll
	/usr/libexec/myprogram/myotherlib-with-binreloc.dll
	/usr/bin/myprogram

The work around is to make myprogram call BinReloc directly, or to modify binreloc to use a custom assembly as reference
instead of blindly getting the calling assembly.