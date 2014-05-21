bundler
=======

A command-line compression tool

### usage
```
Usage:
        bundler [options] files_and_or_folders...

Options:
        -h or --help        this screen
        -v or --verbose     show extra info
        -q or --quiet       do not print to screen, unless errors are found
        -t or --test        do not write files into disk
        -r or --recursive   glob files in subdirectories
        -p or --pack (alg)  pack all files found using provided algorithm = { lz4, lzma (default), lz77, shoco }
        -u or --unpack      unpack all files found
        -x or --xor         unpack all packed files found; pack all unpacked files found

        -w or --wrap file   wrap all files found into a zip container
        -i or --index       show table-of-contents, if using --wrap
        -f or --flat        discard path filename information, if using --wrap
        -m or --move        purge files after wrapping
```

### build
```
echo windows && cl bundler.cc -I deps /Ox /Oy /MT /DNDEBUG /EHsc /link setargv.obj
echo mac osx && clang++ bundler.cc -obundler.mac -I deps -O3 -DNDEBUG --std=c++11
```
