bundler
=======

A command-line compression tool that uses [bundle compression suite](https://github.com/r-lyeh/bundle).

### usage
```
bundler: Bundler 1.1.87 (RELEASE). Compiled on Dec  3 2014 - https://github.com/r-lyeh/bundler

Usage:
        bundler command archive.zip files[...] [options[...]]

Command:
        a or add                       pack files into archive
        p or pack                      pack files into archive (same than above)
        m or move                      move files to archive
        x or extract                   extract archive
        t or test                      test archive
        l or list                      list archive
Options:
        -f or --flat                   discard path filename information, if using --pack or --move
        -h or --help                   this screen
        -i or --ignore PERCENTAGE      ignore compression on files that compress less than given treshold. default is 95 (percent)
        -q or --quiet                  be silent, unless errors are found
        -r or --recursive              recurse subdirectories
        -u or --use ALGORITHM          use compression algorithm = { none, lz4, lzma (default), lzip, deflate, shoco, zpaq, lz4hc, brotli }
        -v or --verbose                show extra info
```

### build
```
echo windows && cl bundler.cc -I deps /Ox /Oy /MT /DNDEBUG /EHsc /link setargv.obj && mt.exe -manifest bundler.exe.manifest -outputresource:bundler.exe;1
echo mac osx && clang++ bundler.cc -obundler.osx -I deps -O3 -DNDEBUG --std=c++11
```
