// @todo: bundle: flush zip stream while compressing (flushing a single memory block when compressing large folders is a crazy idea)
// @todo: bundler: parallel unpack (good idea?)
// @todo: bundler: if lzma_big_dicts * num threads > current_mem force threads=1; || warn "allocation error" 

#ifdef _WIN32
#define NOMINMAX
#include <winsock2.h>
#include <direct.h>
#define mkdir(str,mode) _mkdir(str)
#else
#include <sys/stat.h>
#endif

#include <wire/wire.hpp>
#include <sao/sao.hpp>
#include <bubble/bubble.hpp>
#include <bundle/bundle.hpp>

#include <cassert>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <future>
#include <iostream>
#include <string>
#include <thread>

#define BUNDLER_VERSION "2.0.7" /* (2015/10/05) recreate folder structure when unpacking (@snail23)
#define BUNDLER_VERSION "2.0.6" // (2015/09/28) add CSC/SHRINKER support
#define BUNDLER_VERSION "2.0.5" // (2015/09/24) pump up bundle (pump up brotli; split brotli9/11)
#define BUNDLER_VERSION "2.0.4" // (2015/04/08) BSC stream support
#define BUNDLER_VERSION "2.0.3" // (2015/04/07) pump up bundle (new brotli; new zstd; split lzma20/25)
#define BUNDLER_VERSION "2.0.2" // (2015/04/06) add bypass flag for slow compressors; pump up bubble
#define BUNDLER_VERSION "2.0.1" // (2015/04/06) add @filelist.txt support
#define BUNDLER_VERSION "2.0.0" // (2015/01/28) enforce brotli single-thread; add zstd 
#define BUNDLER_VERSION "1.8.5" // (2014/08/29) bugfix de/compressing empty data; write aligned data unconditionally
#define BUNDLER_VERSION "1.8.4" // (2014/07/31) limit up to 16 threads max
#define BUNDLER_VERSION "1.8.3" // (2014/07/17) bugfix case when decompressing malformed streams
#define BUNDLER_VERSION "1.8.2" // (2014/06/30) pump up bundle
#define BUNDLER_VERSION "1.8.1" // (2014/06/27) add list command;
#define BUNDLER_VERSION "1.1.8" // (2014/06/26) pump up bundle; remove extra check guard in test mode; extract mode
#define BUNDLER_VERSION "1.1.7" // (2014/06/23) add lzh4c mode
#define BUNDLER_VERSION "1.1.6" // (2014/06/09) add zpaq, -a/--add aliases, bugfix --quiet  
#define BUNDLER_VERSION "1.1.5" // (2014/06/05) pump up bundle; default to lzma sdk 9.22; add about dialog 
#define BUNDLER_VERSION "1.1.4" // (2014/06/02) pump up libraries, bugfix for missing files in bundles and dialogs on top; add multithread support (experimental)
#define BUNDLER_VERSION "1.1.3" // (2014/05/28) pump up sao and bundle libraries
#define BUNDLER_VERSION "1.1.2" // (2014/05/23) bugfix for nil sized files; simplify usage; handle destructive operations safely
#define BUNDLER_VERSION "1.1.1" // (2014/05/21) add optional arg to specify compression algorithm
#define BUNDLER_VERSION "1.1.0" // (2014/05/20) add move parameter; bugfix crash with very small files; add progress dialog
#define BUNDLER_VERSION "1.0.0" // (2014/05/14) initial version */

#define BUNDLER_BUILD "DEBUG"
#define BUNDLER_URL "https://github.com/r-lyeh/bundler"
#define BUNDLER_TEXT "Bundler " BUNDLER_VERSION " (" BUNDLER_BUILD ")"

#if defined(NDEBUG) || defined(_NDEBUG)
#undef  BUNDLER_BUILD
#define BUNDLER_BUILD "RELEASE"
#endif

auto now = []() {
    return std::chrono::steady_clock::now();
};
auto then = now();
auto taken = []() -> double {
    return std::chrono::duration_cast< std::chrono::milliseconds >( now() - then ).count() / 1000.0;
};

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

std::string head( const std::string &appname ) {
    std::stringstream cout;
    cout << appname << ": " << BUNDLER_TEXT ". Compiled on " __DATE__ " - " BUNDLER_URL;
    return cout.str();
}

std::string help( const std::string &appname ) {
    std::stringstream cout;
    cout << std::endl;
    cout << "Usage:" << std::endl;
    cout << "\t" << appname << " command archive.zip files[...] [options[...]]" << std::endl;
    cout << "\t" << appname << " command archive.zip @filelist.txt[...] [options[...]]" << std::endl;
    cout << std::endl;
    cout << "Command:" << std::endl;
    cout << "\ta or add                       pack files into archive" << std::endl;
    cout << "\tp or pack                      pack files into archive (same than above)" << std::endl;
    cout << "\tm or move                      move files to archive" << std::endl;
    cout << "\tx or extract                   extract archive" << std::endl;
    cout << "\tt or test                      test archive" << std::endl;
    cout << "\tl or list                      list archive" << std::endl;
    cout << "Options:" << std::endl;
    cout << "\t-h or --help                   this screen" << std::endl;
    cout << "\t-v or --verbose                show extra info" << std::endl;
    cout << "\t-r or --recursive              recurse subdirectories" << std::endl;
    cout << "\t-q or --quiet                  be silent, unless errors are found" << std::endl;
    cout << "\t-f or --flat                   discard path filename information, if using --pack or --move" << std::endl;
    cout << "\t-t or --threads NUM            maximum number of parallel threads, if possible. defaults to 8 (threads)" << std::endl;
    cout << "\t-b or --bypass-slow SIZE       bypass slow zpaq/brotli11 compressors on files larger than given size (in KiB). defaults to 0 (disabled)" << std::endl;
    cout << "\t-i or --ignore PERCENTAGE      ignore compression on files that compress less than given treshold percentage. defaults to 95.0 (percent)" << std::endl;
    cout << "\t-u or --use ENCODER            use compression encoder = { none, all, lz4f, lzma20 (default), lzip, lzma25, deflate, shoco, zpaq, lz4, brotli9, zstd, bsc, brotli11, shrinker, csc20 } (*)" << std::endl;
    cout << "\t-d or --delete ENCODER         delete compression encoder from useable list (useful after -u all)" << std::endl;
    cout << std::endl;
    cout << "\t(*): Specify as many encoders as desired. Bundler will evaluate and choose the best compressor for each file." << std::endl;
    cout << std::endl;

    return cout.str();
}

template<typename T, typename U>
double ratio( const T &A, const U &B ) {
    if( A <= 0 || B <= 0 ) return 0;
    double min = ( A <  B ? double(A) : double(B) );
    double max = ( A >= B ? double(A) : double(B) );
    double ratio = (100 * min) / max;
    return ratio;
}

int main( int argc, const char **argv ) {
    struct getopt args( argc, argv );

    if( args.has("-?") || args.has("-h") || args.has("--help") || args.size() <= 3 ) {
        std::cout << head(args[0]) << std::endl;
        std::cout << help(args[0]);

        bubble::show( bubble::string() <<
            "title.text=About;"
            "body.icon=8;"
            "head.text=" << BUNDLER_TEXT << ";"
            "body.text=" << "<a href\a\"" << BUNDLER_URL << "\">" << BUNDLER_URL << "</a>;"
            "style.minimizable=1;"
        );

        return 0;
    }

    const bool moveit = args[1] == "m" || args[1] == "move";
    const bool packit = args[1] == "p" || args[1] == "pack" || args[1] == "a" || args[1] == "add";
    const bool testit = args[1] == "t" || args[1] == "test";
    const bool upckit = args[1] == "x" || args[1] == "extract";
    const bool listit = args[1] == "l" || args[1] == "list";

    std::vector<unsigned> encoders, fast_encoders;
    const std::string archive = args[2];
    unsigned max_threads = 8;

    const bool flat = args.has("-f") || args.has("--flat");
    const bool quiet = args.has("-q") || args.has("--quiet");
    const bool recursive = args.has("-r") || args.has("--recursive");
    const bool use = args.has("-u") || args.has("--use");
    const bool verbose = ( args.has("-v") || args.has("--verbose") ) && !quiet;
    double treshold = 95.00;    // ignore compression settings if compression ratio below of given treshold 
    size_t lte = 0;             // bypass slow encoders on files larger or equal than given size (in KiB)

    if( !quiet ) {
        std::cout << head(args[0]) << std::endl;
    }

    if( verbose ) {
        std::cout << "options: ";
        std::cout << "moveit=" << moveit << ',';
        std::cout << "packit=" << packit << ',';
        std::cout << "testit=" << testit << ',';
        std::cout << "xtrcit=" << upckit << ',';
        std::cout << "archive=" << archive << ',';
        std::cout << "flat=" << flat << ',';
        std::cout << "quiet=" << quiet << ',';
        std::cout << "recursive=" << recursive << ',';
        std::cout << "use=" << use << ',';
        std::cout << "verbose=" << verbose << ',';
        std::cout << "treshold=" << treshold << ',';
        std::cout << "lte=" << lte;
        /*
        std::cout << "args=";
        for( auto &arg : args ) {
            std::cout << arg.first << ' ';
        }
        */
        std::cout << std::endl;
    }

    int numerrors = 0, processed = 0;
    std::uint64_t total_input = 0, total_output = 0;

    if( !moveit && !packit && !testit && !upckit && !listit ) {
        std::cout << help(args[0]);
        std::cout << "No command." << std::endl;
        return -1;
    }

    bundle::archive archived;
    sao::folder to_pack;

    for( int i = 3; args.has(i); ++i ) {
        if( args[i] == "-f" || args[i] == "--flat" ||
            args[i] == "-q" || args[i] == "--quiet" ||
            args[i] == "-r" || args[i] == "--recursive" ||
            args[i] == "-v" || args[i] == "--verbose" ) {
            continue;
        }
        if( args[i] == "-t" || args[i] == "--threads" ) {
            if( args.has(++i) ) {
                max_threads = args[i].as<unsigned>();
            }
            continue;
        }        
        if( args[i] == "-i" || args[i] == "--ignore" ) {
            if( args.has(++i) ) {
                treshold = args[i].as<double>();
            }
            continue;
        }        
        if( args[i] == "-b" || args[i] == "--bypass-slow" ) {
            if( args.has(++i) ) {
                lte = args[i].as<unsigned>(); 
            }
            continue;
        }        
        if( args[i] == "-u" || args[i] == "--use" ) {
            if( args.has(++i) ) {
                /**/ if( args[i].lowercase() == "none" )      encoders.push_back( bundle::NONE ),     fast_encoders.push_back( bundle::NONE );
                else if( args[i].lowercase() == "lz4f" )      encoders.push_back( bundle::LZ4F ),     fast_encoders.push_back( bundle::LZ4F );
                else if( args[i].lowercase() == "lzip" )      encoders.push_back( bundle::LZIP ),     fast_encoders.push_back( bundle::LZIP );
                else if( args[i].lowercase() == "deflate" )   encoders.push_back( bundle::DEFLATE ),  fast_encoders.push_back( bundle::DEFLATE );
                else if( args[i].lowercase() == "shoco" )     encoders.push_back( bundle::SHOCO ),    fast_encoders.push_back( bundle::SHOCO );
                else if( args[i].lowercase() == "lz4"   )     encoders.push_back( bundle::LZ4 ),      fast_encoders.push_back( bundle::LZ4 );
                else if( args[i].lowercase() == "zstd" )      encoders.push_back( bundle::ZSTD ),     fast_encoders.push_back( bundle::ZSTD );
                else if( args[i].lowercase() == "lzma" )      encoders.push_back( bundle::LZMA20 ),   fast_encoders.push_back( bundle::LZMA20 );
                else if( args[i].lowercase() == "lzma20" )    encoders.push_back( bundle::LZMA20 ),   fast_encoders.push_back( bundle::LZMA20 );
                else if( args[i].lowercase() == "lzma25" )    encoders.push_back( bundle::LZMA25 ),   fast_encoders.push_back( bundle::LZMA25 );
                else if( args[i].lowercase() == "bsc" )       encoders.push_back( bundle::BSC ),      fast_encoders.push_back( bundle::BSC );
                else if( args[i].lowercase() == "csc20" )     encoders.push_back( bundle::CSC20 ),    fast_encoders.push_back( bundle::CSC20 );
                else if( args[i].lowercase() == "shrinker" )  encoders.push_back( bundle::SHRINKER ), fast_encoders.push_back( bundle::SHRINKER );
                else if( args[i].lowercase() == "brotli9" )   encoders.push_back( bundle::BROTLI9 ),  fast_encoders.push_back( bundle::BROTLI9 );
                else if( args[i].lowercase() == "brotli11" )  encoders.push_back( bundle::BROTLI11 );
                else if( args[i].lowercase() == "zpaq" )      encoders.push_back( bundle::ZPAQ );
                else if( args[i].lowercase() == "all" )       encoders = fast_encoders = bundle::encodings();
                else --i;
//                ++i;
            }
            continue;
        }
        if( args[i] == "-d" || args[i] == "--delete" ) {
            auto erase = [&]( int Q ) {
                encoders.erase( std::remove( encoders.begin(), encoders.end(), Q ), encoders.end() );
                fast_encoders.erase( std::remove( fast_encoders.begin(), fast_encoders.end(), Q ), fast_encoders.end() );
            };
            if( args.has(++i) ) {
                /**/ if( args[i].lowercase() == "none" )      erase( bundle::NONE );
                else if( args[i].lowercase() == "lz4f" )      erase( bundle::LZ4F );
                else if( args[i].lowercase() == "lzip" )      erase( bundle::LZIP );
                else if( args[i].lowercase() == "deflate" )   erase( bundle::DEFLATE );
                else if( args[i].lowercase() == "shoco" )     erase( bundle::SHOCO );
                else if( args[i].lowercase() == "lz4"   )     erase( bundle::LZ4 );
                else if( args[i].lowercase() == "zstd" )      erase( bundle::ZSTD );
                else if( args[i].lowercase() == "lzma" )      erase( bundle::LZMA20 );
                else if( args[i].lowercase() == "lzma20" )    erase( bundle::LZMA20 );
                else if( args[i].lowercase() == "lzma25" )    erase( bundle::LZMA25 );
                else if( args[i].lowercase() == "bsc" )       erase( bundle::BSC );
                else if( args[i].lowercase() == "csc20" )     erase( bundle::CSC20 );
                else if( args[i].lowercase() == "shrinker" )  erase( bundle::SHRINKER );
                else if( args[i].lowercase() == "brotli9" )   erase( bundle::BROTLI9 );
                else if( args[i].lowercase() == "brotli11" )  erase( bundle::BROTLI11 );
                else if( args[i].lowercase() == "zpaq" )      erase( bundle::ZPAQ );
                else --i;
//                ++i;
            }
            continue;
        }

        if( args[i][0] == '@' ) {
            // is it a @list.txt kind of file ?
            std::ifstream ifs( &args[i][1] );
            if( ifs.good() ) {
                std::stringstream ss; 
                if( ss << ifs.rdbuf() ) {
                    auto lines = wire::string( ss.str() ).tokenize("\t\f\v\r\n");
                    for( auto end = lines.size(), it = end - end; it < end; ++it ) {
                        to_pack.include( lines[it], {"*"}, recursive );
                    }                                    
                }
            }
        } else {
            // regular file or mask
            to_pack.include( args[i], {"*"}, recursive );
        }
    }

    if( encoders.empty() ) {
        encoders.push_back( bundle::LZMA20 );
    }

    if( fast_encoders.empty() ) {
        fast_encoders.push_back( bundle::LZMA20 );
    }

    if( (packit || moveit) && to_pack.empty() ) {
        std::cout << help(args[0]);
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
//          "style.minimized=1;"
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

    auto normalize = []( std::string pathfile ) -> std::string {
        for( auto &p : pathfile ) {
            if( p == '\\' ) p = '/';
            if( p == ':' ) p = '/';
        }
        return pathfile.size() && pathfile[0] == '/' ? pathfile.substr(1) : pathfile;
    };

    // app starts here

    std::vector<std::thread> threads;
    auto wait_for_threads = [&]() {
        for( auto &in : threads ) {
            if( in.joinable() ) {
                in.join();
            }
        }
    };

    progress_idx = 0;

    if( moveit || packit ) {

        bool single_thread = false;
        for( auto &PACKING_ALGORITHM : encoders ) {
            if( PACKING_ALGORITHM == bundle::ZPAQ || PACKING_ALGORITHM == bundle::BROTLI11 ) {
                single_thread = true;
            }
        }

        std::string algorithms;
        for( auto &u : encoders ) { algorithms += std::string( bundle::name_of(u) ) + ","; }
        if( algorithms.size() ) algorithms.pop_back();
        title_mode = std::string() + ( packit ? "pack" : "move" ) + " (" + algorithms + ")";

        archived.resize( to_pack.size() );

        for( auto &file : to_pack ) {
            progress_pct = (++progress_idx * 100) / to_pack.size();

            if( file.is_dir() ) {
                title_name.clear();
                continue;
            } else {
                title_name = file.name();
            }

            static std::mutex mutex;

            threads.emplace_back( [&]( int idx, std::string filename ){

                auto &with = archived[idx];

                auto pair = readfile( filename );
                const std::string &input = pair.second;

                if( !pair.first ) {
                    return;
                }

                auto measures = bundle::measures( input, lte && (input.size() >= lte) * 1024 ? fast_encoders : encoders );

                auto slot1 = bundle::find_slot_for_smallest_compressor( measures, 100.00 - treshold ); // for_fastest_decompressor
                auto slot2 = bundle::find_slot_for_fastest_decompressor( measures );
                bool skipped = (slot1 == ~0);

                const std::string &output = skipped ? input : measures[ slot1 ].packed;

                double ratio = ::ratio( input.size(), output.size() );
                bool ignored = ratio >= treshold;

                bool valid = !skipped && !ignored;

                with["name"] = flat ? flatten( normalize(filename) ) : normalize(filename);
                with["data"] = valid ? output : input;

                mutex.lock();

                if( !quiet ) {
                    std::string extra = ( valid ? bundle::name_of( measures[ slot1 ].packed ) : "skipped" );
                    std::cout << "[ OK ] " << title_mode << ": " << filename << ": " << input.size() << " -> " << output.size() << " (" << ratio << "%) (" << extra << ")" << std::endl;
                }

                total_input += input.size();
                total_output += valid ? output.size() : input.size();

                mutex.unlock();
            }, processed++, file.name() );

            if( single_thread ) {
                if( threads.back().joinable() ) {
                    threads.back().join();
                }
            }

            if( threads.size() > max_threads ) {
                wait_for_threads();
                threads.clear();
            }
        }

        wait_for_threads();
        progress_pct = 101; // show marquee

        if( 0 == numerrors ) {
            if( !quiet ) {
                std::cout << "[    ] flushing to disk..." << '\r';
            }
            archived.resize( processed );
            bool ok = writefile( archive, archived.bin(bundle::NONE) );
            if( !quiet ) {
                std::cout << ( ok ? "[ OK ] " : "[FAIL] " ) << "flushing to disk..." << std::endl;
            }
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
        // testit, listit or extractit
        title_mode = listit ? "list" : (testit ? "test" : "extract");

        {
            auto result = readfile( archive );
            if( 0 == numerrors ) {
                archived.bin( result.second );
            }
        }

        auto is_ok = []( std::string &output, const std::string &input ) -> bool {
            if( bundle::is_packed( input ) ) {
                return bundle::unpack(output, input);
            } else {
                return bundle::unpack(output, input), true;
            }
        };

        if( listit ) {
            std::cout << archived.toc() << std::endl;
        }

        for( auto &file : archived ) {
            progress_pct = (++progress_idx * 100) / archived.size();

#if 0
            bool found = false;
            for( auto &m : to_pack ) {
                if( wire::string( file["name"] ).matches( m.name() ) ) {
                    found = true;
                    break;
                }
            }

            if( !found ) {
                continue;
            }
#endif

            title_name = file["name"];

            std::cout << "[    ] " << title_mode << ": " << file["name"] << " ...\r";

            std::string uncmp;
            bool ok = true;

            if( upckit || testit ) {
                ok = is_ok( uncmp, file["data"] );
            }

            if( upckit && ok ) {
                // recreate folder structure
                wire::string  path = file["name"];
                wire::strings dirs = path.tokenize("\\/");

                if( path[-1] != '\\' && path[-1] != '/' ) {
                    dirs.pop_back();
                }

                path.clear();
                for( auto &dir : dirs ) {
                    path += dir + "/";
                    mkdir(path.c_str(), 0777);
                }

                // try to unpack it
                std::ofstream ofs( file["name"].c_str(), std::ios::binary );
                ofs << uncmp;
                ok = ofs.good();
            }

            std::cout << ( ok ? "[ OK ] " : "[FAIL] " ) << title_mode << ": " << file["name"] << "    \n";
            numerrors += ok ? 0 : 1;

            processed++;
        }
    }

    bool resume = ( quiet ? ( numerrors > 0 ) : true );
    if( resume ) {
        std::cout << (numerrors > 0 ? "[FAIL] " : "[ OK ] ");
        if( moveit || packit ) {
            std::cout << processed << " processed files, " << numerrors << " errors; " <<  total_input << " bytes -> " << total_output << " bytes (" << ratio( total_input, total_output ) << "%); " << taken() << " secs" << std::endl;
        } else {
            std::cout << processed << " processed files, " << numerrors << " errors;" << std::endl;
        }
    }

    appexit = 1;
    bubble.join();

    return numerrors;
}

#include <bundle/bundle.cpp>
#include <bubble/bubble.cpp>
#include <sao/sao.cpp>
