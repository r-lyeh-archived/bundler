#include <cassert>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <future>

#include <wire/wire.hpp>
#include <sao/sao.hpp>
#include <bundle/bundle.hpp>
#include <bubble/bubble.hpp>

#define BUNDLER_BUILD "DEBUG"
#define BUNDLER_URL "https://github.com/r-lyeh/bundler"
#define BUNDLER_VERSION "1.1.2"
#define BUNDLER_TEXT "Bundler " BUNDLER_VERSION " (" BUNDLER_BUILD ")"

#if defined(NDEBUG) || defined(_NDEBUG)
#undef  BUNDLER_BUILD
#define BUNDLER_BUILD "RELEASE"
#endif

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

int head( const std::string &appname ) {
    std::cout << appname << ": " << BUNDLER_TEXT ". Compiled on " __DATE__ " - " BUNDLER_URL << std::endl;
    return 0;
}

int help( const wire::string &app ) {
    std::cout << std::endl;
    std::cout << "Usage:" << std::endl;
    std::cout << "\t" << app << " command archive.zip files[...] [options[...]]" << std::endl;
    std::cout << std::endl;
    std::cout << "Command:" << std::endl;
    std::cout << "\tm or move              move files to archive" << std::endl;
    std::cout << "\tp or pack              pack files into archive" << std::endl;
    std::cout << "\tt or test              test archive" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "\t-f or --flat           discard path filename information, if using --pack or --move" << std::endl;
    std::cout << "\t-h or --help           this screen" << std::endl;
    std::cout << "\t-q or --quiet          be silent, unless errors are found" << std::endl;
    std::cout << "\t-r or --recursive      recurse subdirectories" << std::endl;
    std::cout << "\t-u or --use ALGORITHM  use compression algorithm = { none, lz4, lzma (default), deflate, shoco }" << std::endl;
    std::cout << "\t-v or --verbose        show extra info" << std::endl;
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

    if( args.has("-?") || args.has("-h") || args.has("--help") || args.size() <= 3 ) {
        head(args[0]);
        help(args[0]);
        return 0;
    }

    const bool moveit = args[1] == "m" || args[1] == "move";
    const bool packit = args[1] == "p" || args[1] == "pack";
    const bool testit = args[1] == "t" || args[1] == "test";

    int PACKING_ALGORITHM = bundle::LZMA;
    const std::string archive = args[2];

    const bool flat = args.has("-f") || args.has("--flat");
    const bool quiet = args.has("-q") || args.has("--quiet");
    const bool recursive = args.has("-r") || args.has("--recursive");
    const bool use = args.has("-u") || args.has("--use");
    const bool verbose = ( args.has("-v") || args.has("--verbose") ) && !quiet;

    if( !quiet )
    head(args[0]);

    if( verbose ) {
        std::cout << "options: ";
        std::cout << "moveit=" << moveit << ',';
        std::cout << "packit=" << packit << ',';
        std::cout << "testit=" << testit << ',';
        std::cout << "archive=" << archive << ',';
        std::cout << "flat=" << flat << ',';
        std::cout << "quiet=" << quiet << ',';
        std::cout << "recursive=" << recursive << ',';
        std::cout << "use=" << use << ',';
        std::cout << "verbose=" << verbose;
        std::cout << std::endl;
    }

    int numerrors = 0, processed = 0;
    std::uint64_t total_input = 0, total_output = 0;

    if( !moveit && !packit && !testit ) {
        head(args[0]);
        help(args[0]);
        std::cout << "No command." << std::endl;
        return -1;
    }

    bundle::pak archived;
    sao::folder to_pack;

    for( int i = 3; args.has(i); ++i ) {
        if( args[i] == "-f" || args[i] == "--flat" ||
            args[i] == "-q" || args[i] == "--quiet" ||
            args[i] == "-r" || args[i] == "--recursive" ||
            args[i] == "-v" || args[i] == "--verbose" ) {
            continue;
        }
        if( args[i] == "-u" || args[i] == "--use" ) {
            if( args.has(++i) ) {
                /**/ if( args[i].lowercase() == "none" )    PACKING_ALGORITHM = bundle::NONE;
                else if( args[i].lowercase() == "lz4" )     PACKING_ALGORITHM = bundle::LZ4;
                else if( args[i].lowercase() == "lzma" )    PACKING_ALGORITHM = bundle::LZMA;
                else if( args[i].lowercase() == "deflate" ) PACKING_ALGORITHM = bundle::DEFLATE;
                else if( args[i].lowercase() == "shoco" )   PACKING_ALGORITHM = bundle::SHOCO;
                else --i;
                ++i;
            }
            continue;
        }

        to_pack.include( args[i], {"*"}, recursive );
    }

    if( (packit || moveit) && to_pack.empty() ) {
        head(args[0]);
        help(args[0]);
        std::cout << "No files provided." << std::endl;
        return -1;
    }

    int progress_pct = 0, progress_idx = 0, appexit = 0;
    std::string title_mode, title_name;
    std::thread bubble( [&]() {
        if( !quiet )
        bubble::show( bubble::string() <<
            "title.text=" << BUNDLER_TEXT ";"
            "body.icon=8;"
            "head.text=;"
            "body.text=;"
            "style.minimizable=1;"
            "progress=0;",
            [&]( bubble::vars &vars ) {
                vars["head.text"] = title_mode;
                vars["title.text"] = std::string() + BUNDLER_TEXT " - " + std::to_string( progress_pct > 100 ? 100 : progress_pct ) + "%";
                vars["progress"] = progress_pct;
                vars["body.text"] = title_name;
                if( appexit ) vars["exit"] = 0;
            }
        );
    } ) ;

    auto readfile = [&]( const std::string &pathfile ) -> std::pair<bool,std::string> {
        std::stringstream ss;
        std::ifstream file( pathfile.c_str(), std::ios::binary | std::ios::ate);
        if( file.good() && !file.tellg() )
            return std::pair<bool,std::string>( true, std::string() );
        std::ifstream ifs( pathfile.c_str(), std::ios::binary );
        if( ifs.good() ) {
            if( ss << ifs.rdbuf() )
                return std::pair<bool,std::string>( true, ss.str() );
        }
        std::cerr << "[FAIL] " << pathfile << ": cannot read file" << std::endl;
        numerrors ++;
        return std::pair<bool,std::string>( false, std::string() );
    };

    auto writefile = [&]( const std::string &pathfile, const std::string &data ) -> bool {
        std::ofstream ofs( pathfile.c_str(), std::ios::binary );
        if( ofs.good() ) {
            ofs.write( &data[0], data.size() );
            if( ofs.good() ) {
                return true;
            }
        }
        std::cerr << "[FAIL] " << pathfile << ": cannot write to disk" << std::endl;
        numerrors ++;
        return false;
    };

    auto flatten = []( const std::string &pathfile ) -> std::string {
        unsigned a = pathfile.find_last_of('/'); a = ( a == std::string::npos ? 0 : a + 1 );
        unsigned b = pathfile.find_last_of('\\'); b = ( b == std::string::npos ? 0 : b + 1 );
        return pathfile.substr( a > b ? a : b );
    };

    // app starts here

    if( moveit || packit ) {
        title_mode = std::string() + ( packit ? "pack" : "move" ) + " (" + bundle::name_of(PACKING_ALGORITHM) + ")";

        for( auto &file : to_pack ) {
            progress_pct = (++progress_idx * 100) / to_pack.size();

            if( file.is_dir() ) {
                title_name.clear();
                continue;
            } else {
                title_name = file.name();
            }

            processed++;

            auto pair = readfile( file.name() );
            const std::string &input = pair.second;

            if( !pair.first ) {
                continue;
            }

            if( !quiet ) {
                std::cout << "[    ] " << title_mode << ": " << file.name() << " ...\r";
            }

            const std::string output = bundle::pack( PACKING_ALGORITHM, input );

            double ratio = ::ratio( input.size(), output.size() );
            bool skipped = ratio < 5.00;

            archived.push_back( bundle::pakfile() );
            archived.back()["filename"] = flat ? flatten( file.name() ) : file.name();
            archived.back()["content"] = skipped ? input : output;

            if( !quiet ) {
                std::string extra = ( skipped ? "(skipped)" : "" );
                std::cout << "[ OK ] " << title_mode << ": " << file.name() << ": " << input.size() << " -> " << output.size() << " (" << ratio << "%)" << extra << std::endl;
            }

            total_input += input.size();
            total_output += output.size();
        }

        progress_pct = 101; // show marquee

        if( 0 == numerrors ) {
            if( !quiet ) {
                std::cout << "[    ] flushing to disk..." << '\r';
            }
            std::cout << ( writefile( archive, archived.bin(bundle::NONE) ) ? "[ OK ] " : "[FAIL] " ) << "flushing to disk..." << std::endl;
        }

        if( 0 == numerrors && verbose ) {
            std::cout << "TOC " << archived.toc() << std::endl;
        }

        if( 0 == numerrors && moveit ) {
            for( auto &file : to_pack ) {
                bool ok = sao::file( file ).remove();
                if( !ok ) numerrors ++;
                if( !ok ) std::cout << "[FAIL] cannot delete file: " << file.name() << std::endl;
            }
        }

    } else {
        // testit
        title_mode = "test";

        {
            auto result = readfile( archive );
            if( 0 == numerrors ) {
                archived.bin( result.second );
            }
        }

        auto is_ok = []( const std::string &data ) -> bool {
            if( bundle::is_unpacked(data) )
                return true;
            const std::string out = bundle::unpack(data);
            return out != data;
        };

        for( auto &file : archived ) {

            progress_pct = (++progress_idx * 100) / archived.size();

            title_name = file["filename"];

            std::cout << "[    ] " << title_mode << ": " << file["filename"] << " ...\r";
            bool ok = is_ok( file["content"] );
            std::cout << ( ok ? "[ OK ] " : "[FAIL] " ) << title_mode << ": " << file["filename"] << "    \n";
            numerrors += ok ? 0 : 1;

            processed++;
        }
    }

    bool resume = ( quiet ? ( numerrors > 0 ) : true );
    if( resume ) {
        std::cout << (numerrors > 0 ? "[FAIL] " : "[ OK ] ");
        if( moveit || packit ) {
            std::cout << processed << " processed files, " << numerrors << " errors; " <<  total_input << " bytes -> " << total_output << " bytes; (" << ratio( total_input, total_output ) << "%)" << std::endl;
        } else {
            std::cout << processed << " processed files, " << numerrors << " errors;" << std::endl;
        }
    }

    appexit = 1;
    bubble.join();

    return numerrors;
}

#include <sao/sao.cpp>
#include <bundle/bundle.cpp>
#include <bubble/bubble.cpp>
