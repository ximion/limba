/* buildlist.vala -- Generate apsymbols.h file
 *
 * Copyright (C) 2009-2010 Jan Niklas Hasse <jhasse@gmail.com>
 * Copyright (C) 2010-2012 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

using GLib;

class VersionNumber : Object
{
	private int major    { get; set; } // x.0.0
	private int minor    { get; set; } // 0.x.0
	private int revision { get; set; } // 0.0.x
	private string originalString;

	public VersionNumber(string version)
	{
		originalString = version;
		try
		{
			var regex = new Regex("([[:digit:]]*)\\.([[:digit:]]*)\\.*([[:digit:]]*)");
			var split = regex.split(version);
			assert(split.length > 1); // TODO: Don't use assert, print a nice error message instead
			major = split[1].to_int();
			if(split.length > 2)
			{
				minor = split[2].to_int();
			}
			else
			{
				minor = 0;
			}
			if(split.length > 3)
			{
				revision = split[3].to_int();
			}
			else
			{
				revision = 0;
			}
		}
		catch(GLib.RegexError e)
		{
			stdout.printf("Error compiling regular expression!");
			Posix.exit(-1);
		}
	}

	public bool newerThan(VersionNumber other)
	{
		if(major > other.major)
		{
			return true;
		}
		else if(major == other.major)
		{
			if(minor > other.minor)
			{
				return true;
			}
			else if(minor == other.minor)
			{
				if(revision > other.revision)
				{
					return true;
				}
			}
		}
		return false;
	}
	public string getString()
	{
		return originalString;
	}
}

int main(string[] args)
{
	try
	{
		if(args.length != 3)
		{
			stdout.printf("Usage: buildlist <output path of apsymbols.h> <minimum glibc version>\n");
			return 1;
		}

		var minimumVersion = new VersionNumber(args[2]);
		var filename = args[1] + "/apsymbols.h";

		// We need to check if the buildlist executable changed.
		int fd = Posix.open(args[0], 0);
		Posix.Stat stat;
		Posix.fstat(fd, out stat);
		int modificationTime = (int)stat.st_mtime;
		Posix.close(fd);

		string firstLine = "/* minimum glibc %s; modification time of buildlist %d */".printf(minimumVersion.getString(), modificationTime);
		string content;
		try
		{
			if(FileUtils.get_contents(filename, out content)) // TODO: Don't open the whole file just to read the first line
			{
				// Is this already the correct file?
				if(content.split("\n")[0] == firstLine)
				{
					return 0; // Don't generate it again
				}
			}
		}
		catch(Error e)
		{
			// The file couldn't be opened, but we don't care
		}

		var headerFile = new StringBuilder();
		headerFile.append(firstLine + "\n");

		stdout.printf("Generating %s (glibc %s) .", filename, minimumVersion.getString());
		stdout.flush();

		string output, errorOutput;
		int returnCode;
		var libPath = File.new_for_path("/lib/");
		var enumerator = libPath.enumerate_children(FileAttribute.STANDARD_NAME, 0, null);
		FileInfo fileinfo;
		var counter = 0;

		var symbolMap = new Gee.HashMap<string, VersionNumber> ();
		// This map contains every symbol and the version as close to the minimum version as possible

		var symbolsNewerThanMinimum = new Gee.HashSet<string> ();
		// This set contains all symbols used by glibc versions newer than minimumVersion

		while((fileinfo = enumerator.next_file(null)) != null)
		{
			++counter;
			if(counter % 50 == 0)
			{
				stdout.printf(".");
				stdout.flush();
			}
			Process.spawn_command_line_sync("objdump -T /lib/" + fileinfo.get_name(), out output, out errorOutput, out returnCode);
			if(returnCode == 0)
			{
				foreach(var line in output.split("\n"))
				{
					var regex = new Regex("(.*)(GLIBC_)([[:digit:]]\\.([[:digit:]]\\.)*[[:digit:]])(\\)?)([ ]*)(.+)");
					if(regex.match(line) && !("PRIVATE" in line))
					{
						var version = new VersionNumber(regex.split(line)[3]);
						var symbolName = regex.split(line)[7];
						var versionInMap = symbolMap.get(symbolName);
						if(symbolName == "2")
						{
							stdout.printf("%s\n", line);
						}
						if(versionInMap == null)
						{
							symbolMap.set(symbolName, version);
						}
						else
						{
							// Is this version older then the version in the map?
							if(versionInMap.newerThan(minimumVersion) && versionInMap.newerThan(version))
							{
								symbolMap.set(symbolName, version);
							}
							// If the version in the map is already older then the minimum version, we should only add newer versions
							if(minimumVersion.newerThan(versionInMap) && version.newerThan(versionInMap) && minimumVersion.newerThan(version))
							{
								symbolMap.set(symbolName, version);
							}
						}
						if(version.newerThan(minimumVersion))
						{
							symbolsNewerThanMinimum.add(symbolName);
						}
					}
				}
			}
		}

		headerFile.append("""/* libuild embedded metadata */
#define LIBUILD_NOTE_METADATA(s)   __asm__(".section .metadata, \"MS\", @note, 1\n\t.string \"" s "\"\n\t.previous\n\t")

#ifdef LIBUILD_VERSION
LIBUILD_NOTE_METADATA("libuild.version=" LIBUILD_VERSION);
#endif

/* libuild generated symbol exclusion list */
""");
		var it = symbolMap.keys.iterator ();
		while (it.next ())
		{
			// Remove all symbols which aren't obsoleted by newer versions
			if(!symbolsNewerThanMinimum.contains (it.get ()))
			{
				continue;
			}

			var version = symbolMap.get (it.get ());
			string versionToUse = version.getString ();
			if(version.newerThan (minimumVersion))
			{
				versionToUse = "DONT_USE_THIS_VERSION_%s".printf(version.getString ());
			}
			headerFile.append ("__asm__(\".symver %s, %s@GLIBC_%s\");\n".printf(it.get (), it.get (), versionToUse));
		}
		FileUtils.set_contents (filename, headerFile.str);
	}
	catch(Error e)
	{
		warning ("%s", e.message);
		return 1;
	}
	stdout.printf (" OK\n");

	return 0;
}
