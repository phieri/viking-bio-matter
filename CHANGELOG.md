# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Fixed
- Fixed missing NULL check in `serial_handler_read()` to prevent potential crashes
- Fixed race condition in `serial_handler_data_available()` by adding atomic read with interrupts disabled
- Fixed potential buffer overflow in Viking Bio protocol parser
- Fixed temperature range validation for both binary and text protocols (0-500°C for binary, -50-500°C for text)
- Improved error handling in `matter_bridge_init()` to continue in degraded mode on failures
- Added output parameter initialization in `viking_bio_parse_data()` to prevent uninitialized data

### Added
- Added comprehensive function documentation (doxygen-style) to all header files
- Added named constants for protocol limits (VIKING_BIO_MAX_TEMPERATURE, etc.)
- Added .editorconfig file for consistent code style across editors
- Added better initialization logging in main.c
- Added explicit type casts to prevent implicit conversions

### Changed
- Improved bounds checking in `viking_bio_parse_data()` for clearer logic
- Enhanced sscanf usage with explicit buffer length checks
- Updated serial handler to be fully thread-safe with atomic operations
- Improved code comments and documentation throughout

### Security
- Added validation for temperature values to prevent invalid data from being processed
- Added bounds checking to prevent buffer overflows in text protocol parsing
- Improved interrupt safety in circular buffer operations
