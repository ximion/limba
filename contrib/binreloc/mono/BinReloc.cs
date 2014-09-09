// BinReloc - A class for creating relocatable applications
// Written By: Isak Savo <isak.savo@gmail.com>
// http://www.autopackage.org/
//
// This source code is public domain. You can relicense this code
// under whatever license you want.
//
// See http://autopackage.org/docs/binreloc/ for
// more information and how to use this.

using System;

namespace Autopackage
{
	///<summary>
	/// Inheriting from or instanciating this class will give you
	/// easy access to the directories where your data files are located.
	///</summary>
	public sealed class BinReloc
	{
		/// <value>
		/// Gets the directory where the assembly is located in
		/// </value>
		private static string BaseDir
		{
			get {
				return System.IO.Path.GetDirectoryName(System.Reflection.Assembly.GetCallingAssembly().Location);
			}
		}
		
		///<summary>The prefix of your application. For instance "/usr" or "/usr/local"</summary>
		public static string Prefix {
			get {
				return BaseDir;
			}
		}
		/// <summary>The binary directory. (prefix + "/bin")</summary>
		public static string BinDir {
			get {
				return System.IO.Path.Combine (BaseDir, "bin");
			}
		}
		/// <summary>The data directory. (prefix + "/share")</summary>
		public static string DataDir {
			get {
				return System.IO.Path.Combine (BaseDir, "share");
			}
		}
		/// <summary>The library directory. (prefix + "/lib")</summary>
		public static string LibDir {
			get {
				return System.IO.Path.Combine (BaseDir, "lib");
			}
		}
		
		/// <summary>The binary directory. (prefix + "/libexec")</summary>
		public static string LibExecDir {
			get {
				return System.IO.Path.Combine (BaseDir, "libexec");
			}
		}
		
		/// <summary>The locale (contains translation databases) directory. (prefix + "/share/locale")</summary>
		public static string LocaleDir {
			get {
				return System.IO.Path.Combine (BaseDir, "share/locale");
			}
		}
		
		/// <summary>The superuser binary directory. (prefix + "/sbin")</summary>
		public static string SbinDir {
			get {
				return System.IO.Path.Combine (BaseDir, "sbin");
			}
		}		
	} 
} 