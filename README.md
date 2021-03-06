bundler :package:
=======

A command-line archiver that uses [bundle compression suite](https://github.com/r-lyeh/bundle).

### Usage
```lisp
bundler: Bundler 2.1.3 (64-bit RELEASE). Compiled on Feb  8 2016 - https://github.com/r-lyeh/bundler

Usage:
        bundler command archive.zip files[...] [options[...]]
        bundler command archive.zip @filelist.txt[...] [options[...]]

Command:
        a or add                       pack files into archive
        p or pack                      pack files into archive (same than above)
        m or move                      move files to archive
        x or extract                   extract archive
        t or test                      test archive
        l or list                      list archive
Options:
        -h or --help                   this screen
        -v or --verbose                show extra info
        -b or --bypass-slow SIZE       bypass slow compressors on files larger than given size (in KiB). defaults to 0 (disabled)
        -d or --delete ENCODER         delete compression encoder from useable list (useful after -u all)
        -f or --flat                   discard path filename information, if using --pack or --move
        -i or --ignore PERCENTAGE      ignore compression on files that compress less than given treshold percentage. defaults to 95.0 (percent)
        -q or --quiet                  be silent, unless errors are found
        -r or --recursive              recurse subdirectories
        -t or --threads NUM            maximum number of parallel threads, if possible. defaults to 8 (threads)
        -u or --use ENCODER            use compression encoder = { none, all, lz4, lz4f, zstd, zstdf, lzma20 (default), lzma25, brotli9, brotli11,
                                       bsc, csc20, shrinker, shoco, miniz, lzip, zpaq, tangelo, zmolly, zling, bcm, mcm, crush, lzjb, bzip2 } (*)

        (*): Specify as many encoders as desired. Bundler will evaluate and choose the best compressor for each file.
```

### Build

```
git clone https://github.com/r-lyeh/bundler --recursive
```

```
echo windows && cl bundler.cc -I deps /Ox /Oy /MT /DNDEBUG /DBUNDLE_USE_OMP_TIMER=1 /EHsc /link setargv.obj 
echo windows && mt.exe -manifest bundler.exe.manifest -outputresource:bundler.exe;1
echo windows && deps\rh\ResourceHacker.exe -addoverwrite bundler.exe,bundler.exe,deps\bundler.ico,ICONGROUP,MAINICON,0
echo mac osx && clang++ bundler.cc -obundler.osx -I deps -O3 -DNDEBUG --std=c++11
```

### Changelog
- v2.1.3 (2016/02/08): Add BZIP2 support
- v2.1.2 (2015/12/04): Update Bundle library
- v2.1.1 (2015/12/02): Add CRUNCH/LZJB support
- v2.1.0 (2015/11/24): Add ZMOLLY/ZLING/ZSTDF/TANGELO/BCM/MCM support
- v2.0.9 (2015/10/29): Display extra listing information
- v2.0.8 (2015/10/10): Display compression ranking for all processed files; new icon
- v2.0.7 (2015/10/05): Recreate folder structure when unpacking (@snail23)
- v2.0.6 (2015/09/28): Add CSC20/SHRINKER support
- v2.0.5 (2015/09/24): Pump up bundle (pump up brotli; split brotli9/11 enums)
- v2.0.4 (2015/04/08): BSC stream support
- v2.0.3 (2015/04/07): Pump up bundle (new brotli; new zstd; split lzma20/25 enums)
- v2.0.2 (2015/04/06): Add bypass flag for slow compressors
- v2.0.2 (2015/04/06): Pump up bubble
- v2.0.1 (2015/04/06): Add @filelist.txt support
- v2.0.0 (2015/01/28): Enforce brotli single-thread
- v2.0.0 (2015/01/28): Add zstd 
- v1.8.5 (2014/08/29): Bugfix de/compressing empty data
- v1.8.5 (2014/08/29): Write aligned data unconditionally
- v1.8.4 (2014/07/31): Limit up to 16 threads max
- v1.8.3 (2014/07/17): Bugfix case when decompressing malformed streams
- v1.8.2 (2014/06/30): Pump up bundle
- v1.8.1 (2014/06/27): Add list command
- v1.1.8 (2014/06/26): Pump up bundle
- v1.1.8 (2014/06/26): Remove extra check guard in test mode
- v1.1.8 (2014/06/26): Extract mode
- v1.1.7 (2014/06/23): Add lzh4c mode
- v1.1.6 (2014/06/09): Add zpaq, -a/--add aliases, bugfix --quiet
- v1.1.5 (2014/06/05): Pump up bundle
- v1.1.5 (2014/06/05): Default to lzma sdk 9.22
- v1.1.5 (2014/06/05): Add about dialog 
- v1.1.4 (2014/06/02): Pump up libraries, bugfix for missing files in bundles and dialogs on top
- v1.1.4 (2014/06/02): Add multithread support (experimental)
- v1.1.3 (2014/05/28): Pump up sao and bundle libraries
- v1.1.2 (2014/05/23): Bugfix for nil sized files
- v1.1.2 (2014/05/23): Simplify usage
- v1.1.2 (2014/05/23): Handle destructive operations safely
- v1.1.1 (2014/05/21): Add optional arg to specify compression algorithm
- v1.1.0 (2014/05/20): Add move parameter
- v1.1.0 (2014/05/20): Bugfix crash with very small files
- v1.1.0 (2014/05/20): Add progress dialog
- v1.0.0 (2014/05/14): Initial version
