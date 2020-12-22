<p align="center"><img src="https://www.empirical-soft.com/logo.png"></p>

<p align="center">
<a href="https://github.com/empirical-soft/empirical-lang/actions?query=workflow%3AEmpirical"><img src="https://github.com/empirical-soft/empirical-lang/workflows/Empirical/badge.svg" alt="Build Status"></a>
</p>

## About Empirical

[Empirical](https://www.empirical-soft.com/) is a language for time-series analysis. It has statically typed Dataframes with integrated queries and builtin timestamps.

## Examples

Read CSV files and infer their types (at compile time!):

```
>>> let trades = load("trades.csv"), quotes = load("quotes.csv")
```

Compute a one-minute volume-weighted average price (VWAP):

```
>>> from trades select vwap = wavg(size, price) by symbol, bar(timestamp, 1m)
```

Get the most recent quote for each trade:

```
>>> join trades, quotes on symbol asof timestamp
```

Get the closest event within three seconds:

```
>>> join trades, events on symbol asof timestamp nearest within 3s
```

See the [website](https://www.empirical-soft.com/) for more examples and full documentation.

## Building Empirical

The build is handled by CMake. There are some scripts to help with the process, both for POSIX (`*.sh`) and Windows (`*.bat`). These can be called by `make` or `make.bat` for an easy workflow.

|     | POSIX | Windows |
| --- | ----- | --------|
| debug build | `make` | `make.bat` |
| production build | `make prod` | `make.bat prod` |
| run tests | `make test` | `make.bat test` |
| deploy binary | `make deploy` | `make.bat deploy` |
| clean build | `make clean` | `make.bat clean` |

Additionally, POSIX has `make website`.

While the actual code is all C++, there are code generators that need the JVM ([ANTLR](https://www.antlr.org)) and Python ([ASDL](https://github.com/empirical-soft/asdl4cpp)). These generators are included in this repo, so there is nothing to download.

The build is statically linked, so no libraries are needed to distribute the resulting binary.

## Structure

The code for Empirical is structured as follows:

 - `src/` - source code for Empirical
 - `tests/` - regression tests
 - `doc/` - documentation
 - `cmake/` - CMake files
 - `scripts/` - build scripts
 - `thirdparty/` - open-source dependencies

## Contributing

See the [Contributor's Notes](CONTRIBUTING.md). There is also the [Maintainer's Notes](MAINTAINING.md) for the curious.

## License

[Affero General Public License](https://www.gnu.org/licenses/agpl-3.0.en.html) (AGPL) with the [Commons Clause](https://commonsclause.com).

A proprietary license is available to vendors and customers that need more commercial-friendly terms.
