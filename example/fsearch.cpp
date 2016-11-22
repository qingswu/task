//
// fsearch; a simple file search program.
//
// author: Dalton Woodard
// contact: daltonmwoodard@gmail.com
// repository: https://github.com/daltonwoodard/awaitable-task.git
// license:
//
// Copyright (c) 2016 DaltonWoodard. See the COPYRIGHT.md file at the top-level
// directory or at the listed source repository for details.
//
//      Licensed under the Apache License. Version 2.0:
//          https://www.apache.org/licenses/LICENSE-2.0
//      or the MIT License:
//          https://opensource.org/licenses/MIT
//      at the licensee's option. This file may not be copied, modified, or
//      distributed except according to those terms.
//

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <regex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include "../task.hpp"

namespace bfs = boost::filesystem;
namespace bpo = boost::program_options;

struct context
{
    bfs::path search_path;
    std::vector <std::string> matcher_strings;
    std::vector <std::regex> matchers;
    std::string filter_string;
    std::regex filter;
    std::string syntax;
    std::size_t num_threads;
    bool help;
    bool use_file;
    bool print_matches;
    bool print_files;
    bool verbose;
};

static context parse_args (int argc, char ** argv)
{
    static std::map <std::string, std::regex_constants::syntax_option_type>
        const syntaxes =
    {
        {"ECMAScript", std::regex_constants::ECMAScript},
        {"posix", std::regex_constants::basic},
        {"eposix", std::regex_constants::extended},
        {"awk", std::regex_constants::awk},
        {"grep", std::regex_constants::grep},
        {"egrep", std::regex_constants::egrep},
    };

    static std::map <std::string, std::string> const filters =
    {
        {"ECMAScript", ".*"},
        {"posix", ".*"},
        {"eposix", ".*"},
        {"awk", "/.*/"},
        {"grep", ".*"},
        {"egrep", ".*"},
    };

    bpo::options_description opdesc {
        "usage: fsearch <match_regex> [options...]"
    };
    opdesc.add_options ()
        ("help,h", "print this message")
        ("use-file,u",
         "the <match_regex> argument refers to a file containing one or"
         " more search patterns")
        ("search",
         bpo::value <std::vector <std::string>> (),
         "the match regex(es) to search for (set syntax with --syntax,-s)")
        ("path,p",
         bpo::value <std::string> ()->default_value ("."),
         "the search directory path")
        ("filter,f",
         bpo::value <std::string> ()->default_value (".*"),
         "a filter regex to determine which files to examine (set syntax"
         " with --syntax,-s)")
        ("syntax,s",
         bpo::value <std::string> ()->default_value ("ECMAScript"),
         "the match regex syntax; available syntaxes are ECMAScript, posix,"
         " eposix (extended POSIX), awk, grep, and egrep (extended grep).")
        ("threads,t",
         bpo::value <std::size_t> ()
            ->default_value (std::thread::hardware_concurrency ()),
         "number of worker threads to use")
        ("suppress-matches,M",
         "output only names of files containing search matches (i.e., do"
         " not list each match)")
        ("suppress-files,F",
         "output only the matches found (i.e., do not list each file where a"
         " match was found)")
        ("verbose,v", "run in verbose mode");

    bpo::variables_map vars;
    bpo::store (
        bpo::command_line_parser (argc, argv)
            .options (opdesc)
            .run (),
        vars
    );
    bpo::notify (vars);

    if (!vars.count ("search")) {
        std::ostringstream err;
        err << "input error -- must provide a search regex\n"
            << opdesc;
        throw std::runtime_error (err.str ());
    }

    context c;
    c.search_path = bfs::path (vars ["path"].as <std::string> ());
    c.matcher_strings = vars ["search"].as <std::vector <std::string>> ();
    c.filter_string = vars ["filter"].as <std::string> ();
    c.syntax = vars ["syntax"].as <std::string> ();
    c.num_threads = vars ["threads"].as <std::size_t> ();
    c.help  = vars.count ("help") != 0;
    c.use_file = vars.count ("use-file") != 0;
    c.print_matches = vars.count ("suppress-matches") == 0;
    c.print_files = vars.count ("suppress-files") == 0;
    c.verbose = vars.count ("verbose") != 0;

    if (!bfs::is_directory (c.search_path)) {
        std::ostringstream err;
        err << "input error -- search path ["
            << c.search_path
            << "] is not a directory\n"
            << opdesc;
        throw std::runtime_error (err.str ());
    } else if (!bfs::exists (c.search_path)) {
        std::ostringstream err;
        err << "input error -- search path ["
            << c.search_path
            << "] does not exist\n"
            << opdesc;
        throw std::runtime_error (err.str ());
    } else if (syntaxes.find (c.syntax) == syntaxes.end ()) {
        std::ostringstream err;
        err << "input error -- unrecognized regex syntax ["
            << c.syntax
            << "]\n"
            << opdesc;
        throw std::runtime_error (err.str ());
    }

    auto const regex_syntax = syntaxes.at (c.syntax);
    c.filter = std::regex {
        c.filter_string.empty () ?
        filters.at (c.syntax) : c.filter_string,
        regex_syntax
    };

    if (c.use_file) {
        for (auto const & f : c.matcher_strings) {
            if (!bfs::is_regular_file (bfs::path (f))) {
                std::ostringstream err;
                err << "input error -- input ["
                    << f << "] is not a file\n"
                    << opdesc;
                throw std::runtime_error (err.str ());
            }

            std::ifstream regex_file (f);
            std::string line;
            while (std::getline (regex_file, line) && !line.empty ()) {
                c.matchers.emplace_back (line, regex_syntax);
            }
        }
    } else {
        for (auto const & s : c.matcher_strings) {
            c.matchers.emplace_back (s, regex_syntax);
        }
    }

    if (c.matchers.empty ()) {
        std::ostringstream err;
        err << "input error -- no provided search regex\n"
            << opdesc;
        throw std::runtime_error (err.str ());
    }

    return c;
}

static std::tuple <
    std::map <
        std::string, std::future <std::pair <bool, std::vector <std::string>>>
    >,
    std::size_t,
    std::size_t,
    std::size_t
> perform_search (context const & cntx)
{
    dsa::task_system <> work_pool {cntx.num_threads};
    std::map <
        std::string, std::future <std::pair <bool, std::vector <std::string>>>
    > results;
    std::size_t dirs_searched {0};
    std::size_t files_searched {0};
    std::atomic_size_t bytes_read {0};

    auto find_matches =
    [&cntx, &results, &bytes_read] (bfs::path const & filepath)
        -> std::pair <bool, std::vector <std::string>>
    {
        static auto read_file =
        [] (bfs::path const & filepath, std::vector <char> & buf)
            -> bool
        {
            std::ifstream fs (
                filepath.string (),
                std::ios::binary|std::ios::ate
            );
            auto const size = fs.tellg ();
            fs.seekg (0, std::ios::beg);
            buf.resize (size, ' ');
            fs.read (buf.data (), size);
            return fs.good ();
        };

        std::vector <char> contents;
        if (read_file (filepath, contents)) {
            bytes_read += contents.size ();
            std::vector <std::string> results;

            for (auto const & m : cntx.matchers) {
                auto const begin = std::cregex_iterator {
                    contents.data (),
                    contents.data () + contents.size (),
                    m
                };
                auto const end = std::cregex_iterator {};
                for (auto it = begin; it != end; ++it)
                    results.emplace_back ((*it) [0]);
            }

            auto const match = !results.empty ();
            return std::make_pair (match, std::move (results));
        } else {
            throw std::runtime_error (
                "failed to read file " + filepath.string ()
            );
        }
    };

    for (auto && e : bfs::recursive_directory_iterator (cntx.search_path)) {
        auto && path = bfs::path (e.path ());
        if (bfs::exists (path)) {
            auto && name = std::string (path.string ());
            if (!bfs::is_directory (e) &&
                std::regex_search (name, cntx.filter))
            {
                files_searched++;
                results.emplace (
                    std::move (name),
                    work_pool.push (find_matches, std::move (path))
                );
            } else if (bfs::is_directory (e)) {
                dirs_searched++;
            }
        }
    }

    work_pool.done ();
    work_pool.wait_to_completion ();

    return std::make_tuple (
        std::move (results), dirs_searched, files_searched, bytes_read.load ()
    );
}

int main (int argc, char ** argv)
{
    try {
        auto const cntx = parse_args (argc, argv);

        if (cntx.verbose) {
            std::cerr << "[[info: search path " << cntx.search_path << "]]\n"
                << "[[info: filter regex \"" << cntx.filter_string << "\"]]\n"
                << "[[info: syntax \"" << cntx.syntax << "\"]]\n"
                << "[[info: num. workers " << cntx.num_threads << "]]\n"
                << "[[info: displaying files " << cntx.print_files << "]]\n"
                << "[[info: displaying matches " << cntx.print_matches << "]]\n";
            for (auto const & r : cntx.matcher_strings) {
                std::cerr << "[[info: search regex \"" << r << "\"]]\n";
            }
        }

        auto results = perform_search (cntx);

        if (cntx.verbose) {
            std::cerr << "[[info: searched " << std::get <2> (results)
                << " files in " << std::get <1> (results)
                << " directories]]\n"
                << "[[info: read " << std::get <3> (results)
                << " bytes in total]]"
                << std::endl;
        }

        auto & match_results = std::get <0> (results);
        for (auto & mr : match_results) {
            try {
                auto const match_pair = mr.second.get ();
                if (match_pair.first) {
                    if (cntx.print_files && cntx.print_matches) {
                        for (auto const & m : match_pair.second)
                            std::cout << mr.first << ':' << m << '\n';
                    } else if (cntx.print_files) {
                        std::cout << mr.first;
                    } else if (cntx.print_matches) {
                        for (auto const & m : match_pair.second)
                            std::cout << m << '\n';
                    }
                }
            } catch (std::exception const & ex) {
                std::cerr << "[[exception: " << ex.what () << "]]"
                          << std::endl;
            }
        }

        std::cout << std::flush;
    } catch (std::exception const & ex) {
        std::cerr << ex.what () << std::endl;
        return 1;
    }

    return 0;
}
