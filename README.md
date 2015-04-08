bundler
=======

A command-line compression tool that uses [bundle compression suite](https://github.com/r-lyeh/bundle).

### usage
```c++
bundler: Bundler 2.0.4 (RELEASE). Compiled on Apr  8 2015 - https://github.com/r-lyeh/bundler

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
        -r or --recursive              recurse subdirectories
        -q or --quiet                  be silent, unless errors are found
        -f or --flat                   discard path filename information, if using --pack or --move
        -t or --threads NUM            maximum number of parallel threads, if possible. defaults to 8 (threads)
        -b or --bypass-slow SIZE       bypass slow zpaq/brotli compressors on files larger than given size (in KiB). defaults to 0 (disabled)
        -i or --ignore PERCENTAGE      ignore compression on files that compress less than given treshold percentage. defaults to 95.0 (percent)
        -u or --use ENCODER            use compression encoder = { none, lz4, lzma20 (default), lzip, lzma25, deflate, shoco, zpaq, lz4hc, brotli, zstd, bsc } (*)

        (*): Specify as many encoders as desired. Bundler will evaluate and choose the best compressor for each file.
```

### build
```
echo windows && cl bundler.cc -I deps /Ox /Oy /MT /DNDEBUG /EHsc /link setargv.obj && mt.exe -manifest bundler.exe.manifest -outputresource:bundler.exe;1
echo mac osx && clang++ bundler.cc -obundler.osx -I deps -O3 -DNDEBUG --std=c++11
```

### changelog
- v2.0.4: BSC stream support;
- v2.0.3: faster list command; updated to latest bundle (new brotli, new zstd; split lzmasdk encoder into lzma20 and lzma25 modes) 
- v2.0.2: added bypass flag for slow compressors
