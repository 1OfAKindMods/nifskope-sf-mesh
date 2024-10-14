﻿# NifSkope SF Mesh Tool 2.0.dev9

NifSkope is a tool for opening and editing the NetImmerse file format (NIF). NIF is used by video games such as Morrowind, Oblivion, Skyrim, Fallout 3/NV/4/76, Starfield, Civilization IV, and more.

This is an experimental fork of 2.0.dev9 with many improvements to Starfield support, and a number of fixes to issues related to older games. See [CHANGELOG.md](https://github.com/fo76utils/nifskope/blob/develop/CHANGELOG.md) for details.

### Meshageddon Edition

This is a pure fork of fo76utils version of Nifskope with a few additional spells for a rather specific issue:

When version 1.11.36 of Starfield was released, Bethesda updated every mesh in the game, causing evey mesh to be renamed.
Any .nif file that referenced vanilla .mesh files was broken. Basically it broke most mods.

Starfield 1.11.36 was released before Creation Kit, Q1 2024 - it has been quite a while. I didn't expect this to still be necessary, but I was quite
new to the community, and did not yet know that mod outhers could forbid others from fixing their mods then disappearing.
This is probably why you are here, and it is most unfortunate.

This is intended as a rough-around the edges tool for mod authors to fix their mods, but if they haven't, and don't allow patching... players must.

## Repairing the old .nifs in bulk

Use the bulk mesh fix spell to repair the .nifs, then convert them to internal geometry so it can't happen again.

# Fix the mesh paths

* Disable "Convert to internal Geometry on load" under Settings->NIF
* Disable Auto-Sanitizie on Save in the File menu
* Go to spells, bulk update mesh paths
* Select the folder for the mod you need to fix, hit go.
* After it finishes the last .nif, Nifskope will act like you need to save, and you can, but you don't need to.

Don't worry if things fail to show up -- some things wont until the mod is deployed.
It reports on what it did and generates a detailed log file.

# Convert to Internal Geometry (And Sanitize) in Bulk
(Technically optional, but recommended)

* ENABLE "Convert to internal Geometry on load" under Settings->NIF
* ENABLE The Auto-Sanitizie on Save checkbox in the File menu if so desired
* Go to spells, bulk Open and Save - again browse to the same folder and go

  Note: If any issues are encoutered, there will be popups. Most of them are informational - just let them stack up and close the porgram when it is done to be rid of the popups.
  Note2: This spell does no logging


### Download

Binary packages for Windows and Linux can be downloaded from [Releases](https://github.com/fo76utils/nifskope/releases). The most up to date builds are automatically generated on any change to the source code, and are available as artifacts from GitHub workflows under [Actions](https://github.com/fo76utils/nifskope/actions). Note that downloading artifacts requires signing in to GitHub. Binaries have been built for CPUs with support for AVX2, or AVX and F16C (NifSkope\_noavx2.exe).

You can also download the latest official release from [niftools/nifskope](https://github.com/niftools/nifskope/releases), or development builds from [hexabits/nifskope](https://github.com/hexabits/nifskope/releases).

**Notes:**

Running NifSkope under Wayland on Linux may require setting the QT\_QPA\_PLATFORM environment variable to "xcb":

    QT_QPA_PLATFORM=xcb ./NifSkope

The resource manager in this version of NifSkope is optimized for PCs with solid-state drives. While hard disk drives generally also work, if the number of loose resources is large, load times can be significantly shorter on an SSD when the data is not cached yet by the operating system.

#### Building from source code

Compiling NifSkope requires Qt 6.2 or newer. On Windows, [MSYS2](https://www.msys2.org/) can be used for building. After running the MSYS2 installer, use the following commands in the MSYS2-UCRT64 shell to install required packages:

    pacman -S base-devel mingw-w64-ucrt-x86_64-gcc
    pacman -S mingw-w64-ucrt-x86_64-qt6-base mingw-w64-ucrt-x86_64-qt6-5compat
    pacman -S mingw-w64-ucrt-x86_64-qt6-imageformats mingw-w64-ucrt-x86_64-qt6-tools
    pacman -S git

Using the MSYS2-CLANG64 environment instead of UCRT64 is also supported, in this case, the **ucrt** in the package names needs to be replaced with **clang**.

All installed MSYS2 packages can be updated anytime later by running the command '**pacman -Syu**'. To download the complete NifSkope source code, use '**git clone**' as follows:

    git clone --recurse-submodules https://github.com/fo76utils/nifskope.git

Finally, run '**qmake6**' and then '**make**' in MSYS2-UCRT64 to build the source code (the -j 8 option sets the number of processes to run in parallel). The resulting binaries and required DLL files and resources are placed under '**release**'.

    cd nifskope
    qmake6 NifSkope.pro
    make -j 8

By default, code is generated for Intel Haswell or compatible CPUs, including the AMD Zen series or newer. Running qmake with the **noavx2=1** option reduces the requirement to Intel Ivy Bridge or AMD FX CPUs, and **nof16c=1** to Sandy Bridge. To build for even older hardware, use **noavx=1** or edit the compiler flags in NifSkope.pro.

Adding the **debug=1** option to the qmake command enables compiling a debug build of NifSkope.

### Issues

Anyone can report issues specific to this fork at [GitHub](https://github.com/fo76utils/nifskope/issues).


### Contribute

You can fork the latest source from [GitHub](https://github.com/fo76utils/nifskope). See [Fork A Repo](https://help.github.com/articles/fork-a-repo) on how to send your contributions upstream. To grab all submodules, make sure to use `--recursive` like so:

```
git clone --recursive git://github.com/<YOUR_USERNAME>/nifskope.git
```

For information about development:

- Refer to our [GitHub wiki](https://github.com/niftools/nifskope/wiki#wiki-development) for information on compilation.


### Miscellaneous

Refer to these other documents in your installation folder or at the links provided:


## [GLTF IMPORT/EXPORT](https://github.com/fo76utils/nifskope/blob/develop/README_GLTF.md)

## [TROUBLESHOOTING](https://github.com/fo76utils/nifskope/blob/develop/TROUBLESHOOTING.md)

## [CHANGELOG](https://github.com/fo76utils/nifskope/blob/develop/CHANGELOG.md)

## [CONTRIBUTORS](https://github.com/fo76utils/nifskope/blob/develop/CONTRIBUTORS.md)

## [LICENSE](https://github.com/fo76utils/nifskope/blob/develop/LICENSE.md)

