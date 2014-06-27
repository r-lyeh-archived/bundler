bundler
=======

A command-line compression tool

### usage
```
bundler: Bundler 1.1.81 (RELEASE). Compiled on Jun 27 2014 - https://github.com/r-lyeh/bundler

Usage:
        bundler command archive.zip files[...] [options[...]]

Command:
        a or add               pack files into archive
        p or pack              pack files into archive (same than above)
        m or move              move files to archive
        x or extract           extract archive
        t or test              test archive
        l or list              list archive
Options:
        -f or --flat           discard path filename information, if using --pack or --move
        -h or --help           this screen
        -q or --quiet          be silent, unless errors are found
        -r or --recursive      recurse subdirectories
        -u or --use ALGORITHM  use compression algorithm = { none, lz4, lzma (default), lzip, deflate, shoco, zpaq }
        -v or --verbose        show extra info
```

### build
```
echo windows && cl bundler.cc -I deps /Ox /Oy /MT /DNDEBUG /EHsc /link setargv.obj && mt.exe -manifest bundler.exe.manifest -outputresource:bundler.exe;1
echo mac osx && clang++ bundler.cc -obundler.mac -I deps -O3 -DNDEBUG --std=c++11
```
