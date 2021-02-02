<?xml version="1.0" encoding="UTF-8"?>

<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">

<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en-us" lang="en-us">

  <head>
    <title>TI-99/Sim</title>
    <meta name="Author" content="Marc Rousseau"/>
    <link rel="stylesheet" href="main.css" type="text/css" title="Default"/>
    <link rel="shortcut icon" href="/images/ti.ico" type="image/x-icon"/>
  </head>

  <body>

    <div class="toc">
      <p><b>Table of Contents:</b></p>
      <p class="toc1"><a href="#TOC_1">Introduction</a></p>
      <p class="toc1"><a href="#TOC_2">Getting Started</a></p>
      <p class="toc2"><a href="#TOC_2A">Building</a></p>
      <p class="toc2"><a href="#TOC_2B">Installation</a></p>
      <p class="toc1"><a href="#TOC_3">Overview</a></p>
      <p class="toc2"><a href="#disks">Disks</a></p>
      <p class="toc2"><a href="#files">Files</a></p>
      <p class="toc1"><a href="#TOC_4">Included Programs</a></p>
      <p class="toc2"><a href="#TOC_4A">catalog</a></p>
      <p class="toc2"><a href="#TOC_4B">convert-ctg</a></p>
      <p class="toc2"><a href="#TOC_4C">decode</a></p>
      <p class="toc2"><a href="#TOC_4D">disk</a></p>
      <p class="toc2"><a href="#TOC_4E">dumpcpu</a></p>
      <p class="toc2"><a href="#TOC_4F">dumpgrom</a></p>
      <p class="toc2"><a href="#TOC_4G">dumpspch</a></p>
      <p class="toc2"><a href="#TOC_4H">list</a></p>
      <p class="toc2"><a href="#TOC_4I">mkcart</a></p>
      <p class="toc2"><a href="#TOC_4J">mkspch</a></p>
      <p class="toc2"><a href="#TOC_4K">say</a></p>
      <p class="toc2"><a href="#TOC_4L">ti99sim-console</a></p>
      <p class="toc2"><a href="#TOC_4M">ti99sim-sdl</a></p>
    </div>

    <div class="header2">

    <div style="text-align: center;">
      <p class="title">TI-99/Sim</p>
      <p class="sub-title">a Texas Instruments Home Computer Emulator</p>
      <p class="hide"><em>NOTE:</em> This page is designed to use CSS styles - you may want to upgrade your browser.</p>
    </div>

    </div>

    <h1><a name="TOC_1">Introduction</a></h1>

      <div class="indent">

      <p>This is the latest incarnation of an emulator for the TI-99/4A that I've been working on, on and off, since 1993. At first it was a simple text-based simulation of the TI (ti99sim-console) running under MS-DOS&trade;. Then it was ported to OS/2 and graphics support for the OS/2 Presentation Manager was added. Later, it was ported to Windows and had sound support added. Now it's been ported to Linux and Mac OS X and more features are still being added.</p>

      <p><em>DISCLAIMER</em>: I'm not one for writing a lot of documentation, so you're encouraged to look at the code to see how some things work if they're not described here. If you're interested in writing documentation, let me know.</p>

      <p><em>NOTE</em>: In order to run the emulator, you need to create a cartridge that contains the console ROM &amp; GROMs from the TI-99/4A. Texas Instruments will not allow these to be distributed, so they are not included with ti99sim. In order to get a working installation, you should run the included setup script which will create the required files.</p>

      </div>

    <h1><a name="TOC_2">Getting Started</a></h1>

      <div class="indent">

      <p>Before building or running TI-99/Sim you must install the <acronym title="Simple DirectMedia Layer">SDL</acronym> library. The latest version is available at <a href="http://www.libsdl.org/download-1.2.php">http://www.libsdl.org/download-1.2.php</a>. If you are planning on building TI-99/Sim, you will need to install the <q>Development Libraries</q> version otherwise, the <q>Runtime Libraries</q> package is sufficient.</p>

      <h2><a name="TOC_2A">Building</a></h2>

        <div class="indent">

        <p>If you downloaded the binary-only version, you can skip straight to <a href="#TOC_2B">Installation</a>. Before building you should make sure that the SDL development code has been installed on your system and ensure that the sdl2-config utility is in your search path.</p>

        <p>If everything is installed properly, running 'make' will build all the executables found in the binary-only package. Any OS specific operations are detailed in the following sections.</p>

        <p><em>NOTE</em>: When building from the source, all the executables are left in their corresponding directories.</p>

        <h3>Linux &amp; Raspberry Pi</h3>

          <div class="indent">

          <p>Since this is the primary development environment, you should have few problems building under Linux.</p>

          <p>First, make sure you have the following installed:</p>

          <ul>
              <li>Either: g++ v4.8 or newer, or clang++ v3.4 or newer</li>
              <li>libsdl1.2-dev</li>
              <li>libssl-dev</li>
              <li>libboost-regex-dev</li>
          </ul>

          <p>Then, from the ti99sim folder simply type:</p>

          <pre>
            <b>~/ti99sim&gt;</b> make
          </pre>

          <p><em>NOTE</em>: If you are seeing messages like 'plugin needed to handle to object', you will need to type 'make AR=gcc-ar RANLIB=gcc-ranlib' in order to build successfully.</p>

          </div>

        <h3>Macintosh OS X</h3>

          <div class="indent">

          <p>In order to build ti99sim under Macintosh OS X, you'll need to install the developer tools from Apple. You will also need to copy the SDL main files (SDLMain.m and SDLMain.h) from the development libraries to the src/sdl folder. Then, from a bash shell, just type:</p>

          <pre>
            <b>~/ti99sim&gt;</b> make
          </pre>

          <p><em>NOTE</em>: I'm not a Mac OS X guru, so there is no neat little package for TI-99/Sim. This probably means that to run it you'll need to do so from a command window. (If anybody is interested in working on a <q>proper</q> Mac OS X install is more than welcome to do so.)</p>

          </div>

        <h3>Windows</h3>

          <div class="indent">

          <p>Solution and project files for Microsoft's Visual Studio 2015 can be found in the v14 folder.</p>

          <p>These files assume that you have installed the SDL development files in a folder called SDL-1.2.15 in a directory at the same level as the TI-99/Sim directory.</p>

          </div>

        </div>

      <h2><a name="TOC_2B">Installation</a></h2>

        <div class="indent">

        <h3><a name="TOC_2B1">The Directories</a></h3>

          <div class="indent">

          <p>Installing the emulator is easy. Make sure you have sufficient privileges to create/write to the installation directories and type:</p>

          <pre>
            make install
          </pre>

          <p>This will create the default directories and install the binaries. The default directory is /opt/ti99sim and the binaries will be installed in /opt/ti99sim/bin. In addition to installing the binaries, symbolic links to them will be created in /usr/local/bin (which should already be in your path). When loading files (ROMs, cartridges, disk images, &hellip;), ti99sim will look in the current directory, ~/.ti99sim, and finally, /opt/ti99sim. Under Windows, the search is limited to the current directory followed by the directory specified in the environment variable HOME (if it is defined at all). When searching directories other than the current directory, the following sub-directories are considered:</p>

          <pre>
            &#x2500;&#x252C;&#x2500; cartridges
             &#x251C;&#x2500; console
             &#x2514;&#x2500; disks
          </pre>

          <p>When looking for a file, the appropriate sub-directory for that file type will be searched. If there is no logical directory for a given file, only the base directory will be searched.</p>

          </div>

        <h3><a name="TOC_2B2">The Console ROM &amp; Cartridges</a></h3>

        <div class="indent">

          <p><em>NOTE</em>: Since v0.14.0, the <b>setup</b> script found in the scripts folder is provided to help create a set of both console &amp; cartridge files to get you started. This section describes how to create and install your console ROM files by hand if you prefer to do it yourself.</p>

          <p>Once you have run <q>make install</q>, you will need to create the console ROM cartridge <q>TI-994A.ctg</q> (see <a href="#TOC_4B">convert-ctg</a> below for instructions) in order to run the emulator. This ROM cartridge contains the operating system, the BASIC interpreter, and a description of the RAM configuration for the computer. Without it, the emulator will not do anything useful. Once you have created the console ROM cartridge, copy it to either ~/.ti99sim/console or /opt/ti99sim/console.</p>

          <p>You should also place a copy of spchrom.bin into this directory to enable speech synthesis. If you don't have this file, you can use <a href="#TOC_4J">mkspch</a> to create one that will keep the emulator happy.</p>

          <p>If you want to use the disk emulation features, you will need to create another cartridge named <q>ti-disk.ctg</q> that contains a copy of the TI Disk Controller ROM (the section on <b>convert-ctg</b> contains information about creating cartridges for peripheral DSR ROMs).</p>

          <p>In order to use the CF7+ disk emulation features, you will need to create a cartridge named <q>cf7+.ctg</q> using the supplied cf7+.dat file.</p>

          <p>A sample installation is shown below. In order to get up and running, however, the only file needed is the TI-994A.ctg file.</p>

          <pre>
            &#x2500;&#x252C;&#x2500; cartridges
             &#x2502;  &#x251C;&#x2500; Alpiner.ctg
             &#x2502;  &#x251C;&#x2500; Editor-Assembler.ctg
             &#x2502;  &#x251C;&#x2500; Parsec.ctg
             &#x2502;  &#x2514;&#x2500; TI Extended BASIC.ctg
             &#x251C;&#x2500; disks
             &#x2502;  &#x251C;&#x2500; ASSM1                   <span class="description">- E/A Disk 1</span>
             &#x2502;  &#x251C;&#x2500; ASSM2                   <span class="description">- E/A Disk 2</span>
             &#x2502;  &#x251C;&#x2500; DISKASSM                <span class="description">- Miller Graphics' Disk-Assembler</span>
             &#x2502;  &#x251C;&#x2500; dsk1.dsk                <span class="description">- Default disk image</span>
             &#x2502;  &#x251C;&#x2500; image.cf7               <span class="description">- Fle accessed by CF7+ module</span>
             &#x2502;  &#x2514;&#x2500; MG_DIAG                 <span class="description">- Miller Graphics' Advanced Diagnostics</span>
             &#x2514;&#x2500; console
                &#x251C;&#x2500; cf7+.ctg                <span class="description">- CF7+ ROM DSR module</span>
                &#x251C;&#x2500; Gram Kracker.ctg        <span class="description">- Miller Graphic's Gram Kracker ROMs</span>
                &#x251C;&#x2500; spchrom.bin             <span class="description">- TI Speech Synthesizer ROM</span>
                &#x251C;&#x2500; ti-disk.ctg             <span class="description">- TI ROM DSR module for disk access</span>
                &#x2514;&#x2500; TI-994A.ctg             <span class="description">- TI-99/4A ROM</span>
          </pre>

          </div>

        <h3><a name="TOC_2B3">Up and Running</a></h3>

          <div class="indent">

          <p>Now you're all set to run the emulator, just type:</p>

          <pre>
            ti99sim-sdl
          </pre>

          <p>If everything is installed correctly, you should see a window on your screen with the TI-99/4A startup screen followed by a beep. Enjoy!</p>

          <p>Using the sample configuration in the previous section, the following are valid commands:</p>

          <pre>
            ti99sim-sdl --dsk2=ASSM1 --dsk3=ASSM2 Editor-Assembler.ctg   <span class="description">(DSK1 will default to using the dsk1.dsk image)</span>
            ti99sim-sdl --dsk1=MG_DIAG "TI Extended BASIC.ctg"           <span class="description">(Note the use of quotes when using modules with spaces in their name)</span>
            ti99sim-sdl -f Parsec.ctg                                    <span class="description">(OK, bring 'em on ...)</span>
          </pre>

          </div>

        </div>

      </div>

    <h1><a name="TOC_3">Overview</a></h1>

      <div class="indent">

      <h2><a name="disks">Disks</a></h2>

        <div>

        <p>What good is a computer is you can't save information or programs? On the TI-99/4A, there are two standard storage media: floppy disks and cassette tapes. This section deals with floppy disks. Depending on your disk controller, the TI could access either 3 or 4 floppy drives. These drive could be either single-density (using FM encoding) or high-density (using MFM encoding). Finally, disks can be either single-sided or double-sided. The various disk sizes are listed in the following table.</p>

        <table class="figure" align="center" cellspacing="0">
          <caption>TI-99/4A Disk Sizes & Nominal Values</caption>
          <tr><th>Notation</th><th>Sides</th><th>Recording Mode</th><th>Sectors/Track</th><th>Tracks</th><th>Capacity</th></tr>
          <tr><td>SSSD</td><td>1</td><td>FM</td><td>9</td><td>40</td><td>90K</td></tr>
          <tr><td>SSDD</td><td>1</td><td>MFM</td><td>18</td><td>40</td><td>180K</td></tr>
          <tr><td>DSSD</td><td>2</td><td>FM</td><td>9</td><td>80</td><td>180K</td></tr>
          <tr><td>DSDD</td><td>2</td><td>MFM</td><td>18</td><td>80</td><td>360K</td></tr>
          <tr><td>CF7+</td><td>2</td><td>CF*</td><td>20</td><td>80</td><td>400K</td></tr>
        </table>

        <p>* CF7+ is not an actual floppy disk. This format was introduced by the CF7+/nanoPEB peripherals for the TI-99/4A. These devices use CompactFlash to store multiple disk images (referred to as volumes) on a single device. The number of images stored depends on the size of the CompactFlash memory. These disk images are larger than typical disk images and require a custom DSR to read them properly. The <b>disk</b> utility is able to read and write to these devices.</p>

        <p>The problem with emulating the TI (besides finding a 5¼ʺ disk drive) on an IBM PC is that most PC's don't easily allow single-density (FM) disks to be read. In addition, modern operating system don't make it easy to access the disk at the level required to read TI disks. To get around this, disks are read (using special programs, usually written for MS-DOS, to get easy access to the disk) and converted to a single file on the local hard drive. However, there is no one standard file format for disks. The formats that I'm aware of, all of which are supported by TI-99/Sim, are shown in table below.</p>

        <p>In addition to supporting disk images, TI-99/Sim also supports archive files. Archive files were developed by Barry Boone and use a Limpel-Zev compression scheme similar to .zip files on the PC. Each file stored in the archive contains all the information to completely recover the original file including the file descriptor record. One way of looking at an archive file is as a cross between a file and a disk (in fact, the <b>disk</b> and other utilities treat these files as disks).</p>

        <table class="figure" align="center" cellspacing="0" width="90%">
          <caption>TI-99/4A Emulator Disk Formats</caption>
          <tr><th width="10%">Name</th><th width="10%">Format</th><th>Pros</th><th>Cons</th></tr>
          <tr><td>AnaDisk</td><td>Header &amp; Data</td><td>Retains sector ordering and special formatting information</td><td>None</td></tr>
          <tr><td>v9t9</td><td>Raw sectors</td><td>Compact image size</td><td>Unable to distinguish between SSDD &amp; DSSD disks</td></tr>
          <tr><td>PC99</td><td>Raw tracks</td><td>Stores a complete copy of a track</td><td>Hard to parse. Lots of extra data (roughly 40% more than v9t9's format)</td></tr>
          <tr><td>HxC Floppy Emulator (hfe)</td><td>Raw Data Pulses</td><td>Stores a complete copy of a track including clock information</td><td>Hardest to parse. Lots of extra data (roughly double PC99's format)</td></tr>
          <tr><td>Archive</td><td>Compressed files</td><td>Uses the least amount of space</td><td>Not a real disk image</td></tr>
        </table>

        </div>

      <h2><a name="files">Files</a></h2>

        <div>

        <p>TI-99/Sim only deals with files contained within disk images. However, there are reasons to deal with individual files rather than entire disks. As with disks, there are a few different formats that individual files may use. The most common format for v9t9 systems is the FIAD format. This format allows the emulator to identify all of the attributes of the original file. A similar, but slightly different format is known as TIFILES. This is the format used to transfer files to/from the TI using the X-Modem protocol. Finally, files can be stored as 'native' files. When this format is used, the TI file is converted, as best as possible, to a native file on the host operating system.</p>

        <table class="figure" align="center" cellspacing="0" width="90%">
          <caption>TI-99/4A File Formats</caption>
          <tr><th>Format</th><th>Description</th></tr>
          <tr><td>FIAD</td><td>v9t9's native file format. The actual file descriptor record from the file is included in the header of the file, followed by the raw sectors that make up the file.</td></tr>
          <tr><td>TIFILES</td><td>Very similar to FIAD in structure. The difference is limited to the 128 byte header. There are at least 3 different flavors of this format. Each flavor includes most or all of the information normally found in the file descriptor record (the original name is not always present).</td></tr>
          <tr><td>'Native'</td><td>This format has no header or formatting information.</td></tr>
        </table>

        <p>Two of the utility applications, <b>dumpcpu</b> and <b>list</b>, access files. These files can be either standalone files or files contained within a disk. To access either a FIAD or TIFILES files, simply use the name of the file on the host filesystem. Files contained within a disk are identified by the name of the containing disk and the intended filename, separated by a colon. For example, to list a BASIC program called GAME on a disk named sample.dsk the syntax would be: <i>list sample.dsk:GAME</i></p>

        </div>

      </div>

    <h1><a name="TOC_4">Included Programs</a></h1>

      <div class="indent">

      <h2><a name="TOC_4A">catalog</a></h2>

        <div class="indent">

        <p>This is a simple utility that will list all of the cartridges/disks/files in the path(s) specified. Each entry is displayed as a SHA1 hash of the entry followed by the filename. Disks and cartridges will additionally display either the disk name or cartridge title. All output is sorted by SHA1 and any duplicate entries are marked to make it easier to clean up multiple copies.</p>

        <p><em>NOTES:</em></p>

        <div class="indent">
          <p>The SHA1 values displayed are hashes of the relevant object being checked. That means the SHA1 of a disk will not match the sha1sum of the disks image file from the command line.</p>

          <p>For disks, the SHA1 is a hash of all the files found on the disk. It is not affected by the disk format or file layout. So, a highly fragmented disk copied to a clean disk file by file will both have the same SHA1 hash.</p>

          <p>For cartridges, the SHA1 is a hash of all the ROM/GROM sections. It is not affected by the Title or any other 'features'.</p>

          <p>For files, the SHA1 is a hash of all relevant data. It does not take into account the unused empty space at the end of sectors.</p>
        </div>

        <p>Command-line syntax:</p>

        <div class="usage">
          Usage: catalog [options] path [path...]

          Options:
             -c, --cartridges          List all cartridges
             -d, --disks               List all disks
             -f, --files               List all files on disks
        </div>

        </div>

      <h2><a name="TOC_4B">convert-ctg</a></h2>

        <div class="indent">

        <p>The ti99sim emulator uses special .ctg files to store the ROM and/or GROM images contained within the computer console and plug-in cartridges. The <b>convert-ctg</b> program allows you to create these cartridges. It will convert your existing v9t9 ROMs or create cartridges using output from a hex dump of a ROM image. A set of sample files, TI-994A.dat, Mini-Memory.dat, and Gram Kracker.dat, located in the roms directory, are supplied as examples of the format of the hex dump file. <em>NOTE</em>: These files do <i>not contain complete hex dumps</i> of their corresponding ROMs but can be used as templates if you have the required information.</p>

        <p>To convert existing v9t9 cartridges, type <q>convert-ctg foo.bin</q> where foo is the base name of a set of ROM files. The base name is usually the portion of the filenames that are common between all files associated with a particular cartridge. <b>convert-ctg</b> understands the naming conventions used by present and past versions of v9t9, and will attempt to determine which version of files that you have installed. For example, the following command will convert the files PARSECC.BIN and PARSECG.BIN to PARSEC.ctg:</p>

        <pre>
          convert-ctg /<i>your-path-here</i>/v9t9/v6.0/modules/PARSEC.BIN
        </pre>

        <p>Files to be converted can be in any directory. The new cartridge will be created in the current working directory. To create the console ROM cartridge you need to specify either 994a.bin or ti.hex (depending on your version of v9t9 or TI Emulator) as the filename. For example:</p>

        <pre>
          convert-ctg /<i>your-path-here</i>/v9t9/v6.0/roms/994a.bin
        </pre>

        <p>This will create the special TI-994A.ctg file that contains the console ROM, the system GROMs, scratch-pad RAM, and 32K memory expansion RAM. Without this file, ti99sim will not run!</p>

        <p>There are three types of cartridges. The first is the special TI-994A.ctg file, the second are DSR ROMs, and the third are <q>normal cartridges</q>. The first two types are expected to be found in the 'console' directories, while the last is expected in the 'cartridges' directories.</p>

        <p>DSR ROMs are special ROM cartridges that are associated with a particular peripheral device. These cartridges must be associated with a particular CRU address. The ROM in these cartridges is located at a different address than the ROMs normally found in a cartridge and without a CRU address, the CPU won't be able to see the ROM within the cartridge. To create a DSR ROM cartridge, simply use the <q>--cru=xxxx</q> option to <b>convert-ctg</b>. This will instruct <b>convert-ctg</b> to store the ROM at the special DSR memory range, and mark the cartridge with the correct CRU information. In order to use the disk emulation features, you need to create a cartridge from a TI disk ROM with the name <q>ti-disk.ctg</q> with the CRU address 1100.</p>

        <p>When creating cartridges, <b>convert-ctg</b> will attempt to add a cartridge title by searching for valid GROM headers in the files being converted. If it does not find any valid names, or if you want to change the one it uses, you can specify a title on the command line after the base filename. ti99sim uses the cartridge title when saving/loading memory images to ensure that the correct cartridges are present when loading. If you intend to use this feature, please make sure that all of your cartridges have unique names.</p>

        <p><b>convert-ctg</b> will also allow you to dump the contents of a cartridge to a specially formatted hex dump file. This feature is useful if you want to see the contents of a cartridge's ROM/GROM code, or to patch the code and rebuild the cartridge with your changes.</p>

        <p>Command-line syntax:</p>

        <div class="usage">
          Usage: convert-ctg [options] file [title]

          Options:
             -8, --378                 Create a ROM cartridge using 74LS378 bank switching
             -9, --379                 Create a ROM cartridge using 74LS379 inverted bank switching
             -6, --force6K             Treat all GROM banks as 6K when creating new cartridges
             --cru=base                Create a DSR cartridge at the indicated CRU address
             -d, --dump                Create a hex dump of the cartridge
             -e, --extract             Create bin files of the cartridge
             -v, --verbose=            Display extra information
        </div>

        </div>

      <h2><a name="TOC_4C">decode</a></h2>

        <div class="indent">

        <p>If you have old TI audio tapes hanging around, <b>decode</b> will convert them into binary files that can be used (in some future version) by ti99sim. It attempts to intelligently find sections of stored data while ignoring any other sounds it finds.  This makes it easier to get all the data from a tape - just create a .wav file that covers the entire tape. There is no need to create a separate .wav file for each data file. Once it's done, you will have a separate file for each track of audio found on the tape. If you have a TI-BASIC or TI Extended BASIC program stored on tape, you can use the <b>list</b> utility to display a listing of the program from the file(s) created by <b>decode</b>.</p>

        <p>Only .wav files are currently supported. Any .wav file stored using a standard format (i.e. non-compressed, PCM audio using integer samples) should work. <b>decode</b> can handle files at any sampling rate and bit depth. This means you can keep your recordings as small as possible by using 8-bit mono 8000Hz .wav files. If your file is not recorded in mono (i.e. stereo or multi-track), <b>decode</b> will only attempt to use data from the first channel (i.e. left (2,3), front left (4), left center (6), ...). Of course, a higher sampling rate or resolution may help the decoding process in cases where the audio is garbled. If you find that a file is not decoding properly, you may want to re-record the tape using higher settings. If you have the proper hardware, for example, you could record a 32-bit mono 44100Hz file (this will create a file over 20 times larger than the one mentioned earlier, so it should only be used as a last resort).</p>

        <p>Command-line syntax:</p>

        <div class="usage">
          Usage: decode [options] file.wav

          Options:
             -v, --verbose=n           Display extra information
        </div>

        </div>

      <h2><a name="TOC_4D">disk</a></h2>

        <div class="indent">

        <p>The emulator allows you to use disks created by a TI-99/4A computer within the emulator. The emulator can read disks created using the dump feature of AnaDisk - with or without the optional 8-byte header for each sector. The latter is the format used by v9t9 disks. The emulator can also read disks that contain raw track data (the format used by PC99). <b>disk</b> can understand both single and double density disk formats.</p>

        <p>The <b>disk</b> utility allows you to look at the contents of a disk image and optionally create native files from those in the disk image. This allows you to create files that can be used by <b>convert-ctg</b> to create cartridge files that you can use with the emulator. There are two output formats for files extracted from a disk. The first (default) format is the v9t9 FIAD format. This format contains a 128-byte header that preserves all of the original file information. The second format is a <q>naked</q> file. These files use only a filename extension to convey the original file format. The actual contents of the file are converted to a conventional file as best as possible. Files using this format <em>may</em> be readable on the host computer (normally, DISPLAY type files are readable).</p>

        <p>the --sha1 option will display a SHA1 hash of the contents of each file. The SHA1 calculation only includes data belonging to the file (it ignores unused bytes at the end of sectors). This means that if a file has been copied to another disk, it should generate the same checksum regardless of how it was copied. The SHA1 value is useful for locating duplicate files across disks (or volumes on a CF7+ image) or determining if two copies of similarly named file are really the same.</p>

        <p>CF7+ devices can be read/written using the <b>disk</b> utility. The syntax for accessing a volume on the device is: &lt;device-file&gt;#&lt;volume&gt;. Where &lt;device-file&gt; can be either the device node where the CompactFlash card is located (e.g. /dev/sdb on a Linux machine) or a file image taken from an actual device. The &lt;volume&gt; is a number (starting at 1) indicating which volume on the device to access.</p>

        <p>The <b>disk</b> utility can also be used to convert disk images between formats. By specifying <q>--output={anadisk|v9t9|pc99|cf7+}</q>, a disk image can be converted to any format.</p>

        <p><em>NOTES</em>:</p>

        <ul>

          <li>If a CF7+ image is specified without specifying a volume index, the only options that will be honored are: --check, --layout, --verbose, and --sha1.</li>

          <li>In order to convert the format of a CF7+ volume, an output file must be specified so that all of the other volumes in the image are not destroyed.</li>

        </ul>

        <p>Command-line syntax:</p>

        <div class="usage">
          Usage: disk [options] file

          Options:
             -a, --add=&lt;filename&gt;      Add &lt;filename&gt; to the disk image
             -b, --bare                Restrict output to a list of files
             --check                   Check the integrity of the disk structures
             -c, --convert             Convert extracted files to DOS files
             --create=&lt;C:H:S&gt;          Create a blank disk image with (C)Tracks, (H)Sides, and (S)Sectors
             --create=&lt;size&gt;           Create a blank disk image of given size (90K,160K,180K,320K,360K,400K,640K,720K,1.44M)
             --create=&lt;type&gt;           Create a blank disk image of given type (SSSD,SSDD,DSSD,DSDD,CF7+,DSHD)
             -d, --dump                Extract all files to FIAD files
             -e, --extract=&lt;filename&gt;  Extract &lt;filename&gt; to v9t9 FIAD file
             --filename=&lt;filename&gt;     Name of the file to convert to
             --force=&lt;format&gt;          Force the disk image to be treated as the specified format (PC99,V9T9,AnaDisk,CF7+,HFE)
             -l, --layout              Display the disk sector layout
             --output=&lt;format&gt;         Convert disk to the specified format (PC99,V9T9,AnaDisk,CF7+,HFE)
             -r, --remove=&lt;filename&gt;   Remove &lt;filename&gt; from the disk image
             -s, --sha1                Display the SHA1 checksum for each file
             -v, --verbose             Display information about the disk image
        </div>

        </div>

      <h2><a name="TOC_4E">dumpcpu</a></h2>

        <div class="indent">

        <p>This program will allow you to disassemble any CPU ROM section in a cartridge file, a DISPLAY/FIXED FIAD file, or RAM from a save image file. Through the use of an external configuration file, you can assign labels and EQUs to make the disassembly more readable.</p>

        <p><em>NOTE</em>: This program is a work in progress and does not always create good listings.</p>

        <p>Command-line syntax:</p>

        <div class="usage">
          Usage: dumpcpu [options] file

          Options:
             --code                    Treat data as CODE by default
             --image                   Treat 'file' as a memory image file
             --no-overwrite            Stop loading DF80 files when data is overwritten
             --range=lo-hi             Only dump the indicate range
             --load=address            Treat address as the start of code
             --scratch-pad             Include scratch pad RAM in disassembly
             -v, --verbose             Display information about the file being analyzed
        </div>

        </div>

      <h2><a name="TOC_4F">dumpgrom</a></h2>

        <div class="indent">

        <p>Ever wondered what the code behind that game looks like? This program is similar to <b>dumpcpu</b> but disassembles the GPL code in the GROM cartridge. It uses the standard GROM headers to find starting addresses and strings.</p>

        <p><em>NOTE</em>: This program is a work in progress and does not always create good listings.</p>

        <p>Command-line syntax:</p>

        <div class="usage">
          Usage: dumpgrom [options] file

          Options:
             --range=lo-hi             Only dump the indicate range
        </div>

        </div>

      <h2><a name="TOC_4G">dumpspch</a></h2>

        <div class="indent">

        <p>Here is a utility that can help you analyze the contents of your speech synthesizer's ROM (spchrom.bin). This program will create a text file, spchrom.dat, which contains the contents of the ROM that can later be used by <b>mkspch</b> to create a new speech ROM. It can create files in two different formats. The first (default) is a simple hexadecimal listing of the speech data for each phrase contained in the ROM file. The second breaks down the raw data into a 'parsed' format that exposes the individual <acronym title="Linear Predictive Code">LPC</acronym>-10 speech frames. Each speech frame represents 25ms worth of sound. If you're feeling adventurous, you can edit the speech frames in the generated .dat file, create a new spchrom.bin file, and see how things sound!</p>

        <p>For those that are interested, the parsed speech frames are of the form:</p>

        <pre>
          STOP CODE      S                - Signals the end of the speech data
          ZERO CODE      Z                - Indicates 1 frames worth of silence
          REPEAT Frame   Pxx Exx R        - Repeats the previous frame using the indicated pitch and energy
          Unvoiced Frame Pxx Exx &lt;Kxx&gt;4   - An unvoiced (fricative) frame with 4 reflection coefficients
          Voiced Frame   Pxx Exx &lt;Kxx&gt;10  - A voiced frame with 10 reflection coefficients
        </pre>

        <p><em>NOTE</em>: If you create a file using the 'spch' format, the resulting file will only contain the valid speech data. The actual TI speech ROM data contains a few extra bytes following several phrases. The parsed output file will not include this extraneous data. A spchrom.bin file created from the generated spchrom.dat file will not match the original ROM byte-for-byte, but will be a completely functional copy.</p>

        <p>Command-line syntax:</p>

        <div class="usage">
          Usage: dumpspch [options] file

          Options:
             --format=hex              Speech data listed in hexadecimal
             --format=spch             Decoded speech data
             -o, --output=&lt;filename&gt;   Create output file &lt;filename&gt;
             -v, --verbose             Display extra information
        </div>

        </div>

      <h2><a name="TOC_4H">list</a></h2>

        <div class="indent">

        <p>This utility can be used to list TI-BASIC (and TI Extended BASIC) programs. Files can be either a native file that has been extracted from a disk image (using 'disk -d xyz.dsk'), or a file inside a disk image (using xyz.dsk:filename, or cf7#5:filename). Find the file you want listed (i.e. HELLO) and type 'list HELLO'. If you have selected a valid BASIC file, a listing similar to the one printed by the BASIC interpreter should print.</p>

        <p>Command-line syntax:</p>

        <div class="usage">
          Usage: list [options] file

          Options:
             -v, --verbose=n           Display extra information about the file
        </div>

        </div>

      <h2><a name="TOC_4I">mkcart</a></h2>

        <div class="indent">

        <p>If you have a directory full of ROM files for a large collection of cartridges (or even a single file), <b>mkcart</b> can help create a set of cartridge files from them in one quick pass. It was designed to be a simple tool for mass cartridge conversion and does not have all the features of <b>convert-ctg</b> but does a much better job when faced with a large set of files. It also understands the <acronym title="The Old School Emulation Center">TOSEC</acronym> naming convention, so the cartridges created will usually have a more descriptive title.</p>

        <p><em>NOTE: <b>mkcart</b> can only create 'normal' cartridges, it cannot be used to create device DSR cartridges or the system ROM cartridge.</em></p>

        <p>Command-line syntax:</p>

        <div class="usage">
          Usage: mkcart [options] directory

          Options:
             -6, --6K                  Treat all GROM banks as 6K when creating new cartridges
             -r, --recurse             Recursively descend into subdirectories when searching for files
             -v, --verbose=n           Display extra information
        </div>

        </div>

      <h2><a name="TOC_4J">mkspch</a></h2>

        <div class="indent">

        <p>If you can't find a copy of spchrom.bin, you can use this utility to create one that can be used by the ti99sim. The spchrom.dat file located in the console directory contains the information required to create a stripped down speech ROM. This file does not contain any actual speech data, but instead contains the bare minimum number of phrases required for the proper operation of TI Extended BASIC's <em>CALL SAY</em> logic. If you have your own <acronym title="Linear Predictive Code">LPC</acronym>-10 coded speech samples (I have no idea how you would create them - <em>yet</em>, so please don't ask), you can use this utility to create a speech ROM using your own voice samples!</p>

        <p>Command-line syntax:</p>

        <div class="usage">
          Usage: mkspch [options] file

          Options:
             -o, --output=&lt;filename&gt;   Create output file &lt;filename&gt;
             -v, --verbose             Display extra information
        </div>

        </div>

      <h2><a name="TOC_4K">say</a></h2>

        <div class="indent">

        <p>Do you miss being able to type CALL SAY("HELLO") and hear the TI's speech synthesizer? Well, <b>say</b> is your answer. This program attempts to mimic the behavior of the TI Extended BASIC routine SAY. It will look through the spchrom.bin file for the indicated pattern and do it's best to produce synthesized speech. Some phrases in the speech library contain spaces. If you want to say one of these ("Texas Instruments" for example), you will need to put quotes around it on the command line, otherwise say will look for two or more separate words that are probably not present.</p>

        <p>Command-line syntax:</p>

        <div class="usage">
          Usage: say [options] &lt;Text-to-speak&gt;

          Options:
             -s, --sample=&lt;freq&gt;       Select sampling frequency for audio playback
             -v, --verbose=n           Display extra information
             --volume=n                Set the audio volume
        </div>

        </div>

      <h2><a name="TOC_4L">ti99sim-console</a></h2>

        <div class="indent">

        <p>A simple text-based version of the emulator. It includes an interface, similar to Miller Graphics' Explorer, which allows you to step through code at the assembly or GPL instruction level. It has two modes: command and run. While it is in command mode, the CPU is paused. You can edit any of the register values, clear the PC breakpoint, load/save memory images, exit... In run mode, the CPU is running and all keystrokes go to the TI. Pressing &lt;Esc&gt; will stop the CPU and return to command mode. Since its text-mode only, it does not support graphics or sprites. If you just want to see what the computer is doing, this is the program to do it with.</p>

        <p>Command-line syntax:</p>

        <div class="usage">
          Usage: ti99sim-console [options] [cartrigde.ctg] [memory.img]

          Options:
             --cf7=&lt;filename&gt;          Use &lt;filename&gt; for CF7+ disk image
             --console=&lt;filename&gt;      Use &lt;filename&gt; for system ROM image
             --dskn=&lt;filename&gt;         Use &lt;filename&gt; disk image for DSKn
             --no-cf7                  Disable CF7+ disk support
             --NTSC                    Emulate a NTSC display (60Hz)
             --PAL                     Emulate a PAL display (50Hz)
             --ucsd                    Enable the UCSD p-System device if present
             -v, --verbose=n           Display extra information
        </div>

        <p><em>NOTE</em>: If you try to load a memory image, you must make sure that any cartridge(s) that were running when the image was made are also specified.</p>

        <div style="FLOAT: left; MARGIN-LEFT: 0.5in">

          <p>Command Mode:</p>

          <ul>
            <li>C - Clear the PC interrupt</li>
            <li>&lt;Space&gt; - Single step</li>
            <li>G - Enter Run Mode</li>
            <li>R - Edit CPU registers</li>
            <li>Q - Exit the program</li>
            <li>L - Load Image</li>
            <li>S - Save Image</li>
            <li>B - Set 'BASIC' display bias</li>
            <li>N - Set 'normal' display bias</li>
          </ul>

        </div>

        <div style="FLOAT: left; MARGIN-LEFT: 0.5in">

          <p>Run Mode:</p>

          <ul>
            <li>&lt;Esc&gt; - Return to command mode</li>
          </ul>

        </div>

        </div>

      <h2><a name="TOC_4M">ti99sim-sdl</a></h2>

        <div class="indent">

        <p>The SDL-based emulator</p>

        <p>Command-line syntax:</p>

        <div class="usage">
          Usage: ti99sim-sdl [options] [cartridge.ctg] [image.img]

          Options:
             -4                        Double width/height window
             --cf7=&lt;filename&gt;          Use &lt;filename&gt; for CF7+ disk image
             --console=&lt;filename&gt;      Use &lt;filename&gt; for system ROM image
             --dskn=&lt;filename&gt;         Use &lt;filename&gt; disk image for DSKn
             --framerate={n/d|p}       Reduce frame rate to fraction n/d or percentage p
             -f, --fullscreen          Fullscreen
             --joystickn=i             Use system joystick i as TI joystick n
             --list-joysticks          Print a list of all detected joysticks
             --no-cf7                  Disable CF7+ disk support
             --no-joystick             Disable hardware joystick support
             -q, --no-sound            Turn off all sound/speech
             --no-speech                Disable speech synthesis
             --NTSC                    Emulate a NTSC display (60Hz)
             --PAL                     Emulate a PAL display (50Hz)
             -p, --palette=n           Select a color palette (1-3)
             --bw                      Display black &amp; white video
             -s, --sample=&lt;freq&gt;       Select sampling frequency for audio playback
             --scale=n                 Scale the window width & height by scale
             --scale2x                 Use the Scale2X algorithm to scale display
             --ucsd                    Enable the UCSD p-System device if present
             -v, --verbose=n           Display extra information
             --volume=n                Set the audio volume
        </div>

        <p><em>NOTE</em>: If you try to load a memory image, you must make sure that any cartridge(s) that were running when the image was made are also specified.</p>

        <p>There is no GUI yet. The following keys are defined:</p>

        <ul>
          <li>ESC - exit</li>
          <li>F2 - Save memory image</li>
          <li>F3 - Load memory image</li>
          <li>F10 - Reboot</li>
        </ul>

        <p>For those of you that don't have easy access to the TI-99/4A keyboard or overlay, here is a summary of the special function keys:</p>

        <div style="FLOAT: left; MARGIN-LEFT: 0.5in">

          <ul>
            <li>&lt;Alt&gt;-1 - DEL</li>
            <li>&lt;Alt&gt;-2 - INS</li>
            <li>&lt;Alt&gt;-3 - ERASE</li>
            <li>&lt;Alt&gt;-4 - CLEAR</li>
            <li>&lt;Alt&gt;-5 - BEGIN</li>
            <li>&lt;Alt&gt;-6 - PROC'D</li>
            <li>&lt;Alt&gt;-7 - AID</li>
            <li>&lt;Alt&gt;-8 - REDO</li>
            <li>&lt;Alt&gt;-9 - BACK</li>
            <li>&lt;Alt&gt;-= - QUIT</li>
          </ul>

        </div>

        <div style="FLOAT: left; MARGIN-LEFT: 0.5in">

          <ul>
            <li>&lt;Alt&gt;-W - ~</li>
            <li>&lt;Alt&gt;-E - &lt;Up Arrow&gt;</li>
            <li>&lt;Alt&gt;-R - [</li>
            <li>&lt;Alt&gt;-T - ]</li>
            <li>&lt;Alt&gt;-U - _</li>
            <li>&lt;Alt&gt;-I - ?</li>
            <li>&lt;Alt&gt;-O - '</li>
            <li>&lt;Alt&gt;-P - "</li>
          </ul>

        </div>

        <div style="FLOAT: left; MARGIN-LEFT: 0.5in">

          <ul>
            <li>&lt;Alt&gt;-A - |</li>
            <li>&lt;Alt&gt;-S - &lt;Left Arrow&gt;</li>
            <li>&lt;Alt&gt;-D - &lt;Right Arrow&gt;</li>
            <li>&lt;Alt&gt;-F - {</li>
            <li>&lt;Alt&gt;-G - }</li>
          </ul>

        </div>

        <div style="FLOAT: left; MARGIN-LEFT: 0.5in">

          <ul>
            <li>&lt;Alt&gt;-Z - \</li>
            <li>&lt;Alt&gt;-X - &lt;Down Arrow&gt;</li>
            <li>&lt;Alt&gt;-C - `</li>
          </ul>

        </div>

        <h3><a name="TOC_4K1">Disk support</a></h3>

          <div class="indent">

          <p>Using the ti-disk.ctg cartridge, the emulator is capable of reading and writing single-density disk images. The TI disk controller is not capable of accessing double-density disks. Internally, the emulator is ready to support double-density disks, but I don't have access to a controller ROM that supports them, so I haven't been able to test them yet.</p>

          <p>The emulator supports all operations on a disk image. When the emulator shuts down, any changes made to the disk image are saved. A disk can be made <q>write-protected</q> by making the file read-only.</p>

          <p><em>NOTE: The disk routines can read and write files in the PC99 format, but disks formatted in the emulator by routines other than those in the DSR ROM (like those formatted using a program like Miller Graphics' <q>Advanced Diagnostics</q>) may not be readable by PC99 or other PC99-compatible software.</em></p>

          </div>

        <h3><a name="TOC_4K2">CF7+ support</a></h3>

          <div class="indent">

          <p>The supplied cf7+.ctg cartridge allows the emulator to access CF7+/nanoPEB files. By default, the emulator will look for a file named 'image.cf7' in the disks subdirectory. In a POSIX environment (Mac OSX, Linux, BSD, ...), this may be a symbolic link to the device containing your CF7+ CompactFlash. If you don't have an actual CF7+ CompactFlash, you can create a file to be used as a virtual CompactFlash file (the CF7+ module will determine the number of volumes based on the size of your file). The <b>disk</b> utility can be used to create an arbitrarily sized file by specifying an appropriate volume number number:</p>

          <pre>
            disk --output=cf7+ --filename=image.cf7#30 blank.dsk     <span class="description">(Copies an existing disk image 'blank.dsk' to volume # 30 of a CF7+ image file name 'image.cf7')</span>
          </pre>

          <p>Unlike normal disk emulation, all operations on the image are immediate (i.e. they are not deferred until the emulator shuts down).</p>

          <p><em>NOTE: In order for this feature to function properly, you must ensure that you have read/write access to the CompactFlash file.</em></p>

          <p><em>NOTE: CF7+ support is limited to disk access. The parallel/serial port features of the CF7+/nanoPEB are currently not supported.</em></p>

          </div>

        <h3><a name="TOC_4K3">RS-232 support</a></h3>

          <div class="indent">

          <p>Sorry, this feature is not implemented - yet.</p>

          </div>

        <h3><a name="TOC_4K4">Gram Kracker&trade; emulation</a></h3>

          <div class="indent">

          <p>If you have created the Gram Kracker cartridge properly and placed the cartridge in the console directory (the one where you placed the TI-994A.ctg file), the following extra keys are active:</p>

          <ul>
            <li>&lt;Ctrl&gt;-F1 - Toggle Gram Kracker On/Off</li>
            <li>&lt;Ctrl&gt;-F2 - Toggle GROM 0/Operating System</li>
            <li>&lt;Ctrl&gt;-F3 - Toggle GROMs 1&amp;2/TI-BASIC</li>
            <li>&lt;Ctrl&gt;-F4 - Disable write-protect/Select BANK 1</li>
            <li>&lt;Ctrl&gt;-F5 - Enable write-protect</li>
            <li>&lt;Ctrl&gt;-F6 - Disable write-protect/Select BANK 2</li>
            <li>&lt;Ctrl&gt;-F7 - Toggle loader On/Off</li>
          </ul>

          <p>Consult your Gram Kracker manual for a description of each of these switches.</p>

          <p>To create a Gram Kracker cartridge, the loader ROM must be added to the <q>Gram Kracker.dat</q> file supplied in the roms directory. Getting the loader ROM is left as an exercise for the reader. (<em>Hints</em>: Save it to disk, transfer it to the PC using AnaDisk, extract it using the <b>disk</b> utility, and a) make a hex dump after stripping off the header or b) create a normal cartridge file using the <b>convert-ctg</b> utility then create a .dat file using the -d option. OR Use the dump feature built into the Edit Memory option of the Gram Kracker loader to send a hex dump to the serial port and capture it on the PC to a disk file).</p>

          </div>

        </div>

      </div>

      <hr/>

      <div style="text-align: center;">
        Homepage: <a href="http://www.mrousseau.org/programs/ti99sim/">http://www.mrousseau.org/programs/ti99sim/</a><br/>
        Contact: <a href="mailto:ti99sim at mrousseau.org?subject=ti99sim">ti99sim<span>@</span>mrousseau<span class="hide">.nonsense</span>.org</a>
      </div>

  </body>

</html>
