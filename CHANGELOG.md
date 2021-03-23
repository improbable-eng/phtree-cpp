# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/en/1.0.0/)
and this project adheres to [Semantic Versioning](http://semver.org/spec/v2.0.0.html).

## [Unreleased]
- Nothing yet.


## [1.0.0] - 2021-03-23
### Added
- API: `MultiMap`: A wrapper that makes PH-Tree behave as a multi-map.
- API: `erase(iterator)`
- API: `emplace_hint(iterator, ...)`
- API for `PhTreeF` and `PhTreeBoxF`: 32bit floating point options
- Support for custom key classes

### Changed
- BREAKING CHANGE: The query functions now require a query box as input (instead of a min/max point pair)
- BREAKING CHANGE: `phtree_box_d.h` has been removed, please use `phtree.h instead.
- BREAKING CHANGE: `phtree_d.h` has been removed, please use `phtree.h` instead.
- BREAKING CHANGE: Data converters (IEEE, Multiply, etc) are now structs i.o. functions/functors
- BREAKING CHANGE: `PhFilterNoOp` has been renamed to `FilterNoOp`
- BREAKING CHANGE: kNN queries now always require the distance function to be specified.
- BREAKING CHANGE: Preprocessors have been refactored and renamed to Converter/ScalarConverter
- Moved CI builds from Travis to GitHub actions

### Removed
- Nothing.

### Fixed
- GCC warnings from `-Wsign-compare` and `-Wsequence-point`.


## 0.1.0 - 2020-07-02
### Added
- Initial version.

### Changed
- Nothing.

### Removed
- Nothing.

### Fixed
- Nothing.


[Unreleased]: https://github.com/improbable-eng/phtree-cpp/compare/v1.0.0...HEAD
[1.0.0]: https://github.com/improbable-eng/phtree-cpp/compare/v0.1.0...v1.0.0
[0.2.0]: https://github.com/improbable-eng/phtree-cpp/compare/v0.1.0...v0.2.0
