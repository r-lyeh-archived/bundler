#include <cassert>
#include <fstream>
#include <iostream>
#include <string>
#include <cstdint>

#include <wire/wire.hpp>
#include <sao/file.hpp>
#include <bundle/bundle.hpp>

struct getopt : public std::map< wire::string, wire::string >
{
    getopt()
    {}

    explicit
    getopt( int argc, const char **argv ) {
        wire::strings args( argc, argv );

        // create key=value and key= args as well
        for( auto &it : args ) {
            wire::strings tokens = it.split( "=" );

            if( tokens.size() == 3 && tokens[1] == "=" )
                (*this)[ tokens[0] ] = tokens[2];
            else
            if( tokens.size() == 2 && tokens[1] == "=" )
                (*this)[ tokens[0] ] = true;
            else
            if( tokens.size() == 1 && tokens[0] != argv[0] )
                (*this)[ tokens[0] ] = true;
        }

        // create args
        while( argc-- ) {
            (*this)[ argc ] = argv[argc];
        }
    }

    bool has( const wire::string &op ) const {
        return this->find(op) != this->end();
    }

    std::string str() const {
        wire::string ss;
        for( auto &it : *this )
            ss << it.first << "=" << it.second << ',';
        return ss.str();
    }

    std::string cmdline() const {
        wire::string cmd;

        // concatenate args
        for( unsigned i = 0; has(i); ++i ) {
            const auto it = this->find(i);
            cmd << it->second << ' ';
        }

        // remove trailing space, if needed
        if( cmd.size() )
            cmd.pop_back();

        return cmd;
    }
};

int head( const wire::string &app ) {
#if defined(NDEBUG) || defined(_NDEBUG)
    const char *build = "RELEASE";
#else
    const char *build = "DEBUG";
#endif
    const char *version = "v1.0.0";
    const char *url = "https://github.com/r-lyeh/bundler";

    std::cout << app << " " << version << " " << build << ". Compiled on " << __DATE__  << " - " << url << std::endl;
    return 0;
}

int help( const wire::string &app ) {
    std::cout << std::endl;
    std::cout << "Usage:" << std::endl;
    std::cout << "\t" << app << " [options] files_and_or_folders..." << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "\t-h or --help        this screen" << std::endl;
    std::cout << "\t-v or --verbose     show extra info" << std::endl;
    std::cout << "\t-q or --quiet       do not print to screen, unless errors are found" << std::endl;
    std::cout << "\t-t or --test        do not write files into disk" << std::endl;
    std::cout << "\t-r or --recursive   glob files in subdirectories" << std::endl;
    std::cout << "\t-p or --pack        pack all files found" << std::endl;
    std::cout << "\t-u or --unpack      unpack all files found" << std::endl;
    std::cout << "\t-x or --xor         unpack all packed files found; pack all unpacked files found" << std::endl;
    std::cout << "\t-w or --wrap file   wrap all files found into a zip container" << std::endl;
    std::cout << "\t-i or --index       show table-of-contents, if using --wrap" << std::endl;
    std::cout << "\t-f or --flat        discard path filename information, if using --wrap" << std::endl;
    std::cout << std::endl;
    return 0;
}

template<typename T, typename U>
double ratio( const T &A, const U &B ) {
    if( A <= 0 || B <= 0 ) return 0;
    double min = ( A <  B ? double(A) : double(B) );
    double max = ( A >= B ? double(A) : double(B) );
    double ratio = 100 - ( (100 * min) / max );
    return ratio;
}

int main( int argc, const char **argv ) {
    getopt args( argc, argv );

    if( args.size() == 1 ) {
        head(args[0]);
        help(args[0]);
        std::cout << "no files provided" << std::endl;
        return -1;
    }

    if( args.has("-?") || args.has("-h") || args.has("--help") ) {
        head(args[0]);
        help(args[0]);
        return 0;
    }

    const bool quiet = args.has("-q") || args.has("--quiet");
    const bool verbose = ( args.has("-v") || args.has("--verbose") ) && !quiet;
    const bool test = args.has("-t") || args.has("--test");
    const bool recursive = args.has("-r") || args.has("--recursive");
    const bool xorit = args.has("-x") || args.has("--xor");
    const bool packit = args.has("-p") || args.has("--pack");
    const bool unpackit = args.has("-u") || args.has("--unpack");
    const bool wrapit = args.has("-w") || args.has("--wrap");
    const bool flat = args.has("-f") || args.has("--flat");
    const bool index = args.has("-i") || args.has("--index");
    std::string wrapname;

    if( !quiet )
    head(args[0]);

    int numerrors = 0, processed = 0;
    std::uint64_t total_input = 0, total_output = 0;

    if( !xorit && !packit && !unpackit ) {
        std::cout << "[FAIL] undefined argument. please provide any -x, -p or -u argument. optionally also -w" << std::endl;
        return -1;
    }

    sao::folder folder;

    for( int i = 1; args.has(i); ++i ) {
         if( args[i] == "-v" || args[i] == "--verbose"
            || args[i] == "-q" || args[i] == "--quiet"
            || args[i] == "-t" || args[i] == "--test"
            || args[i] == "-r" || args[i] == "--recursive"
            || args[i] == "-x" || args[i] == "--xor"
            || args[i] == "-p" || args[i] == "--pack"
            || args[i] == "-u" || args[i] == "--unpack"
            || args[i] == "-f" || args[i] == "--flat"
            || args[i] == "-i" || args[i] == "--index" ) {
            continue;
         }
         if( args[i] == "-w" || args[i] == "--wrap" ) {
            if( args.has(++i) ) wrapname = args[i];
            continue;
         }

         folder.include( args[i], {"*"}, recursive );
     }

     if( folder.empty() ) {
        std::cout << "No files provided." << std::endl;
        return -1;
     }

    for( auto &file : folder ) {

        if( file.is_dir() )
            continue;

         std::string pathfile = file.name(), input, output;

         processed++;

        {
            bool ok = false;
            std::stringstream ss;
            std::ifstream ifs( pathfile.c_str(), std::ios::binary );
            if( ifs.good() )
                if( ss << ifs.rdbuf() )
                    input = ss.str(), ok = true;
            if( !ok ) {
                std::cerr << "[FAIL] " << pathfile << ": cannot read file" << std::endl;
                numerrors ++;
                continue;
            }
        }

        /*
        if( verbose ) {
            std::cout <<
            "input {" << std::endl <<
                "\tis_packed: " << bundle::is_packed( input ) << ',' << std::endl <<
                "\ttype-of: " << bundle::typeof( input ) << ',' << std::endl <<
                "\tlen: " << bundle::length( input ) << ',' << std::endl <<
                "\tz-len: " << bundle::zlength( input ) << ',' << std::endl <<
            "}" << std::endl;
        } */

        int packit_ = ( xorit ? bundle::is_packed( input ) ^ true : packit );

        if( packit_ ) {
            output = bundle::pack( bundle::LZMA, input );
        } else {
            output = bundle::unpack( input );
        }

        /*
        if( verbose ) {
            std::cout <<
            "output {" << std::endl <<
                "\tis_packed: " << bundle::is_packed( output ) << ',' << std::endl <<
                "\ttype-of: " << bundle::typeof( output ) << ',' << std::endl <<
                "\tlen: " << bundle::length( output ) << ',' << std::endl <<
                "\tz-len: " << bundle::zlength( output ) << ',' << std::endl <<
            "}" << std::endl;
        } */

        double ratio = ::ratio( input.size(), output.size() );
        bool skipped = false;
        bool ok = true;

        if( !test ) {

            if( packit_ && ratio < 5.00 ) {
                skipped = true;
            }

            if( (!packit_) || (!skipped) ) {
                std::ofstream ofs( pathfile.c_str(), std::ios::binary );
                ofs.write( &output[0], output.size() );
                if( !ofs.good() ) {
                    ok = false;
                }
            }
        }

        numerrors += ok ? 0 : 1;

        if( !ok ) {
            std::cerr << "[FAIL] " << pathfile << ": " << "cannot write to file." << std::endl;
        } else {
            if( !quiet ) {
                std::string extra = ( skipped ? "(skipped)" : "" );
                if( packit_ ) {
                    std::cout << "[ OK ] pack: " << pathfile << ": " << input.size() << " -> " << output.size() << " (" << ratio << "%)" << extra << std::endl;
                } else {
                    std::cout << "[ OK ] unpack: " << pathfile << ": " << input.size() << " -> " << output.size() << " (" << ratio << "%)" << extra << std::endl;
                }
            }

            total_input += input.size();
            total_output += output.size();
        }
    }

    if( wrapit && !wrapname.empty() ) {

        if( !quiet ) {
            std::cout << "[ OK ] wrap: building ZIP container..." << std::endl;
        }

        bundle::pak pak;

        for( auto &file : folder ) {
            if( file.is_dir() ) continue;

            std::string pathfile = file.name();
            if( flat ) {
                unsigned a = pathfile.find_last_of('/'); a = ( a == std::string::npos ? 0 : a + 1 );
                unsigned b = pathfile.find_last_of('\\'); b = ( b == std::string::npos ? 0 : b + 1 );
                pathfile = pathfile.substr( a > b ? a : b );
            }

            pak.push_back( bundle::pakfile() );
            pak.back()["filename"] = pathfile;
            pak.back()["content"] = file.read();
        }

        if( index ) {
            std::cout << "TOC " << pak.toc() << std::endl;
        }

        if( !test ) {
            std::ofstream ofs( wrapname, std::ios::binary );
            ofs << pak.bin(bundle::NONE);
        }
    }

    if( !quiet ) {
        std::cout << (numerrors > 0 ? "[FAIL] " : "[ OK ] ");
        std::cout << processed << " processed files, " << numerrors << " errors; " <<  total_input << " bytes -> " << total_output << " bytes; (" << ratio( total_input, total_output ) << "%)" << std::endl;
    }

    return numerrors;
}

#include <sao/file.cpp>
#include <bundle/bundle.cpp>
